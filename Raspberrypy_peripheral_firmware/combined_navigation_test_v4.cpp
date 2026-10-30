#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <string>
#include <vector>
#include <cmath>
#include <cstring>
#include <cerrno>
#include <algorithm>
#include <ctime>
#include <cstdint>

#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <linux/spi/spidev.h>

static const char *GPS_DEVICE = "/dev/serial0";
static const speed_t GPS_BAUD = B115200;

static const char *SPI_DEVICE = "/dev/spidev0.0";
static const uint8_t SPI_MODE_VALUE = SPI_MODE_0;
static const uint8_t SPI_BITS_PER_WORD = 8;
static const uint32_t SPI_SPEED_HZ = 500000;

static const uint8_t DREG_EULER_PHI_THETA = 0x70;
static const uint8_t DREG_EULER_PSI = 0x71;

static const double UM7_EULER_SCALE = 91.02222;

static const double SLOPE_THRESHOLD_DEG = 3.0;
static const double TURN_THRESHOLD_DEG = 5.0;
static const double MIN_TURN_SPEED_KMH = 5.0;

struct GPSData {
    bool validFix = false;
    bool hasDate = false;
    bool hasTime = false;
    bool hasLocation = false;
    bool hasSpeed = false;
    bool hasCourse = false;

    std::string date = "";
    std::string utcTime = "";
    std::string mode = "";

    double latitude = NAN;
    double longitude = NAN;
    double speedMps = NAN;
    double speedKmh = NAN;
    double speedKnots = NAN;
    double courseDeg = NAN;
};

struct IMUData {
    bool active = false;
    double pitch = NAN;
    double roll = NAN;
    double yaw = NAN;
};

std::vector<std::string> splitCSV(const std::string &line) {
    std::vector<std::string> fields;
    std::stringstream ss(line);
    std::string item;

    while (std::getline(ss, item, ',')) {
        fields.push_back(item);
    }

    return fields;
}

double parseDouble(const std::string &s, bool *ok = nullptr) {
    try {
        if (s.empty()) {
            if (ok) *ok = false;
            return NAN;
        }

        size_t idx = 0;
        double value = std::stod(s, &idx);

        if (idx == 0) {
            if (ok) *ok = false;
            return NAN;
        }

        if (ok) *ok = true;
        return value;
    } catch (...) {
        if (ok) *ok = false;
        return NAN;
    }
}

std::string stripChecksum(std::string s) {
    size_t pos = s.find('*');

    if (pos != std::string::npos) {
        s = s.substr(0, pos);
    }

    return s;
}

std::string formatNMEATime(const std::string &s) {
    if (s.size() < 6) return "";

    return s.substr(0, 2) + ":" +
           s.substr(2, 2) + ":" +
           s.substr(4, 2);
}

std::string formatNMEADate(const std::string &s) {
    if (s.size() < 6) return "";

    int day = std::stoi(s.substr(0, 2));
    int month = std::stoi(s.substr(2, 2));
    int year = 2000 + std::stoi(s.substr(4, 2));

    std::ostringstream oss;
    oss << std::setw(4) << std::setfill('0') << year << "-"
        << std::setw(2) << std::setfill('0') << month << "-"
        << std::setw(2) << std::setfill('0') << day;

    return oss.str();
}

double nmeaCoordToDecimal(const std::string &coord, const std::string &dir) {
    if (coord.empty() || dir.empty()) return NAN;

    bool ok = false;
    double raw = parseDouble(coord, &ok);

    if (!ok) return NAN;

    double degrees = std::floor(raw / 100.0);
    double minutes = raw - degrees * 100.0;
    double decimal = degrees + minutes / 60.0;

    if (dir == "S" || dir == "W") {
        decimal = -decimal;
    }

    return decimal;
}

std::string valueOrNA(double v, int precision = 3) {
    if (!std::isfinite(v)) return "not available";

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision) << v;
    return oss.str();
}

std::string valueOrBlank(double v, int precision = 3) {
    if (!std::isfinite(v)) return "";

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision) << v;
    return oss.str();
}

double normalizeAngle180(double angle) {
    if (!std::isfinite(angle)) {
        return NAN;
    }

    angle = std::fmod(angle, 360.0);

    if (angle > 180.0) {
        angle -= 360.0;
    }

    if (angle <= -180.0) {
        angle += 360.0;
    }

    return angle;
}

double angleDifference(double current, double previous) {
    if (!std::isfinite(current) || !std::isfinite(previous)) {
        return 0.0;
    }

    return normalizeAngle180(current - previous);
}

std::string classifySlope(double relativePitch) {
    if (!std::isfinite(relativePitch)) return "Unknown";

    if (relativePitch > SLOPE_THRESHOLD_DEG) return "Uphill";
    if (relativePitch < -SLOPE_THRESHOLD_DEG) return "Downhill";

    return "Flat";
}

std::string classifyTurn(double speedKmh, double yawChange) {
    if (!std::isfinite(speedKmh) || speedKmh < MIN_TURN_SPEED_KMH) {
        return "Low Speed / Unreliable";
    }

    if (yawChange > TURN_THRESHOLD_DEG) return "Right Turn";
    if (yawChange < -TURN_THRESHOLD_DEG) return "Left Turn";

    return "Straight";
}

std::string getLocalTimeString() {
    std::time_t now = std::time(nullptr);
    std::tm localTime;
    localtime_r(&now, &localTime);

    char buffer[64];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &localTime);

    return std::string(buffer);
}

std::string createLogFilename() {
    std::time_t now = std::time(nullptr);
    std::tm localTime;
    localtime_r(&now, &localTime);

    char buffer[128];
    std::strftime(buffer, sizeof(buffer),
                  "logs/navigation_log_v4_%Y-%m-%d_%H-%M-%S.csv",
                  &localTime);

    return std::string(buffer);
}

uint32_t bytesToU32BE(uint8_t b3, uint8_t b2, uint8_t b1, uint8_t b0) {
    return (static_cast<uint32_t>(b3) << 24) |
           (static_cast<uint32_t>(b2) << 16) |
           (static_cast<uint32_t>(b1) << 8) |
           static_cast<uint32_t>(b0);
}

int16_t highWordSigned(uint32_t word) {
    return static_cast<int16_t>((word >> 16) & 0xFFFF);
}

int16_t lowWordSigned(uint32_t word) {
    return static_cast<int16_t>(word & 0xFFFF);
}

int openGPS() {
    int fd = open(GPS_DEVICE, O_RDWR | O_NOCTTY | O_NONBLOCK);

    if (fd < 0) {
        std::cerr << "[GPS ERROR] Cannot open " << GPS_DEVICE << ": "
                  << std::strerror(errno) << std::endl;
        return -1;
    }

    struct termios tty;
    std::memset(&tty, 0, sizeof(tty));

    if (tcgetattr(fd, &tty) != 0) {
        std::cerr << "[GPS ERROR] tcgetattr failed: "
                  << std::strerror(errno) << std::endl;
        close(fd);
        return -1;
    }

    cfmakeraw(&tty);
    cfsetispeed(&tty, GPS_BAUD);
    cfsetospeed(&tty, GPS_BAUD);

    tty.c_cflag |= CLOCAL | CREAD;
    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;
    tty.c_cflag &= ~CRTSCTS;

    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 1;

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        std::cerr << "[GPS ERROR] tcsetattr failed: "
                  << std::strerror(errno) << std::endl;
        close(fd);
        return -1;
    }

    return fd;
}

void parseRMC(const std::vector<std::string> &f, GPSData &gps) {
    if (f.size() < 10) return;

    if (!f[1].empty()) {
        gps.utcTime = formatNMEATime(f[1]);
        gps.hasTime = !gps.utcTime.empty();
    }

    gps.validFix = (f[2] == "A");

    double lat = nmeaCoordToDecimal(f[3], f[4]);
    double lon = nmeaCoordToDecimal(f[5], f[6]);

    if (std::isfinite(lat) && std::isfinite(lon)) {
        gps.latitude = lat;
        gps.longitude = lon;
        gps.hasLocation = true;
    }

    bool okSpeed = false;
    double speedKnots = parseDouble(f[7], &okSpeed);

    if (okSpeed) {
        gps.speedKnots = speedKnots;
        gps.speedMps = speedKnots * 0.514444;
        gps.speedKmh = speedKnots * 1.852;
        gps.hasSpeed = true;
    }

    bool okCourse = false;
    double course = parseDouble(f[8], &okCourse);

    if (okCourse) {
        gps.courseDeg = course;
        gps.hasCourse = true;
    }

    if (!f[9].empty()) {
        gps.date = formatNMEADate(f[9]);
        gps.hasDate = !gps.date.empty();
    }

    if (f.size() > 12 && !f[12].empty()) {
        gps.mode = stripChecksum(f[12]);
    }
}

void parseGLL(const std::vector<std::string> &f, GPSData &gps) {
    if (f.size() < 7) return;

    double lat = nmeaCoordToDecimal(f[1], f[2]);
    double lon = nmeaCoordToDecimal(f[3], f[4]);

    if (std::isfinite(lat) && std::isfinite(lon)) {
        gps.latitude = lat;
        gps.longitude = lon;
        gps.hasLocation = true;
    }

    if (!f[5].empty()) {
        gps.utcTime = formatNMEATime(f[5]);
        gps.hasTime = !gps.utcTime.empty();
    }

    if (f[6] == "A") {
        gps.validFix = true;
    }

    if (f.size() > 7 && !f[7].empty()) {
        gps.mode = stripChecksum(f[7]);
    }
}

void parseVTG(const std::vector<std::string> &f, GPSData &gps) {
    if (f.size() < 8) return;

    bool okCourse = false;
    double course = parseDouble(f[1], &okCourse);

    if (okCourse) {
        gps.courseDeg = course;
        gps.hasCourse = true;
    }

    bool okKnots = false;
    double speedKnots = parseDouble(f[5], &okKnots);

    if (okKnots) {
        gps.speedKnots = speedKnots;
        gps.speedMps = speedKnots * 0.514444;
        gps.speedKmh = speedKnots * 1.852;
        gps.hasSpeed = true;
    }

    bool okKmh = false;
    double speedKmh = parseDouble(f[7], &okKmh);

    if (okKmh) {
        gps.speedKmh = speedKmh;
        gps.speedMps = speedKmh / 3.6;
        gps.speedKnots = speedKmh / 1.852;
        gps.hasSpeed = true;
    }

    if (f.size() > 9 && !f[9].empty()) {
        gps.mode = stripChecksum(f[9]);
    }
}

void parseNMEALine(const std::string &line, GPSData &gps) {
    if (line.empty() || line[0] != '$') return;

    std::vector<std::string> fields = splitCSV(line);
    if (fields.empty()) return;

    std::string type = fields[0];

    if (type.size() < 6) return;

    std::string sentence = type.substr(type.size() - 3);

    if (sentence == "RMC") {
        parseRMC(fields, gps);
    } else if (sentence == "GLL") {
        parseGLL(fields, gps);
    } else if (sentence == "VTG") {
        parseVTG(fields, gps);
    }
}

void readGPSAvailable(int gpsFd, std::string &buffer, GPSData &gps) {
    char temp[512];

    while (true) {
        ssize_t n = read(gpsFd, temp, sizeof(temp));

        if (n > 0) {
            buffer.append(temp, temp + n);
        } else {
            break;
        }
    }

    size_t pos;

    while ((pos = buffer.find('\n')) != std::string::npos) {
        std::string line = buffer.substr(0, pos);
        buffer.erase(0, pos + 1);

        line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());
        parseNMEALine(line, gps);
    }

    if (buffer.size() > 4096) {
        buffer.clear();
    }
}

int openSPI() {
    int fd = open(SPI_DEVICE, O_RDWR);

    if (fd < 0) {
        std::cerr << "[IMU ERROR] Cannot open " << SPI_DEVICE << ": "
                  << std::strerror(errno) << std::endl;
        return -1;
    }

    uint8_t mode = SPI_MODE_VALUE;
    uint8_t bits = SPI_BITS_PER_WORD;
    uint32_t speed = SPI_SPEED_HZ;

    if (ioctl(fd, SPI_IOC_WR_MODE, &mode) < 0) {
        std::cerr << "[IMU ERROR] Cannot set SPI mode: "
                  << std::strerror(errno) << std::endl;
        close(fd);
        return -1;
    }

    if (ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0) {
        std::cerr << "[IMU ERROR] Cannot set SPI bits per word: "
                  << std::strerror(errno) << std::endl;
        close(fd);
        return -1;
    }

    if (ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0) {
        std::cerr << "[IMU ERROR] Cannot set SPI speed: "
                  << std::strerror(errno) << std::endl;
        close(fd);
        return -1;
    }

    return fd;
}

bool readUM7Register32(int spiFd, uint8_t reg, uint32_t &value) {
    uint8_t tx[6] = {0x00, reg, 0x00, 0x00, 0x00, 0x00};
    uint8_t rx[6] = {0, 0, 0, 0, 0, 0};

    struct spi_ioc_transfer tr;
    std::memset(&tr, 0, sizeof(tr));

    tr.tx_buf = reinterpret_cast<unsigned long>(tx);
    tr.rx_buf = reinterpret_cast<unsigned long>(rx);
    tr.len = 6;
    tr.speed_hz = SPI_SPEED_HZ;
    tr.bits_per_word = SPI_BITS_PER_WORD;
    tr.delay_usecs = 10;

    int ret = ioctl(spiFd, SPI_IOC_MESSAGE(1), &tr);

    if (ret < 1) {
        return false;
    }

    value = bytesToU32BE(rx[2], rx[3], rx[4], rx[5]);
    return true;
}

bool readUM7Orientation(int spiFd, IMUData &imu) {
    uint32_t phiTheta = 0;
    uint32_t psi = 0;

    bool ok1 = readUM7Register32(spiFd, DREG_EULER_PHI_THETA, phiTheta);
    bool ok2 = readUM7Register32(spiFd, DREG_EULER_PSI, psi);

    if (!ok1 || !ok2) {
        imu.active = false;
        return false;
    }

    int16_t rollRaw = highWordSigned(phiTheta);
    int16_t pitchRaw = lowWordSigned(phiTheta);
    int16_t yawRaw = highWordSigned(psi);

    imu.roll = normalizeAngle180(static_cast<double>(rollRaw) / UM7_EULER_SCALE);
    imu.pitch = normalizeAngle180(static_cast<double>(pitchRaw) / UM7_EULER_SCALE);
    imu.yaw = normalizeAngle180(static_cast<double>(yawRaw) / UM7_EULER_SCALE);

    imu.active = true;
    return true;
}

int main() {
    std::cout << "Combined Navigation Data V4\n";
    std::cout << "Added: Initial Zero Calibration, Normalized Angles, Slope Status, Turn Status, CSV Logging\n";
    std::cout << "---------------------------------------------------------------\n";

    mkdir("logs", 0775);

    std::string csvFilename = createLogFilename();
    std::ofstream csvFile(csvFilename);

    if (!csvFile.is_open()) {
        std::cerr << "[CSV ERROR] Cannot create CSV file: " << csvFilename << std::endl;
        return 1;
    }

    csvFile << "sample_id,local_time,gps_date,utc_time,gps_valid,gps_mode,"
            << "latitude,longitude,speed_mps,speed_kmh,speed_knots,course_deg,"
            << "raw_pitch_deg,raw_roll_deg,raw_yaw_deg,"
            << "relative_pitch_deg,relative_roll_deg,relative_yaw_deg,"
            << "yaw_change_deg,slope_status,turn_status\n";

    std::cout << "CSV Log File: " << csvFilename << std::endl;

    int gpsFd = openGPS();
    int spiFd = openSPI();

    if (gpsFd < 0 || spiFd < 0) {
        std::cerr << "[SYSTEM ERROR] Failed to open GPS or IMU device.\n";

        if (gpsFd >= 0) close(gpsFd);
        if (spiFd >= 0) close(spiFd);

        return 1;
    }

    GPSData gps;
    IMUData imu;
    std::string gpsBuffer;

    bool imuCalibrated = false;
    double pitchOffset = 0.0;
    double rollOffset = 0.0;
    double yawOffset = 0.0;

    double previousYaw = NAN;

    int loopCounter = 0;
    int sampleId = 0;

    while (true) {
        readGPSAvailable(gpsFd, gpsBuffer, gps);

        bool imuOk = readUM7Orientation(spiFd, imu);

        if (imuOk && !imuCalibrated) {
            pitchOffset = imu.pitch;
            rollOffset = imu.roll;
            yawOffset = imu.yaw;
            previousYaw = imu.yaw;
            imuCalibrated = true;

            std::cout << "[IMU CALIBRATION] Initial angle set as zero.\n";
            std::cout << "Pitch Offset: " << pitchOffset << " deg\n";
            std::cout << "Roll Offset: " << rollOffset << " deg\n";
            std::cout << "Yaw Offset: " << yawOffset << " deg\n";
            sleep(2);
        }

        double relativePitch = NAN;
        double relativeRoll = NAN;
        double relativeYaw = NAN;

        if (imuOk && imuCalibrated) {
            relativePitch = normalizeAngle180(imu.pitch - pitchOffset);
            relativeRoll = normalizeAngle180(imu.roll - rollOffset);
            relativeYaw = normalizeAngle180(angleDifference(imu.yaw, yawOffset));
        }

        double yawChange = 0.0;

        if (imuOk && std::isfinite(previousYaw)) {
            yawChange = normalizeAngle180(angleDifference(imu.yaw, previousYaw));
        }

        if (imuOk && std::isfinite(imu.yaw)) {
            previousYaw = imu.yaw;
        }

        std::string slopeStatus = classifySlope(relativePitch);
        std::string turnStatus = classifyTurn(gps.speedKmh, yawChange);

        loopCounter++;

        if (loopCounter % 5 == 0) {
            sampleId++;

            csvFile << sampleId << ","
                    << getLocalTimeString() << ","
                    << gps.date << ","
                    << gps.utcTime << ","
                    << (gps.validFix ? "1" : "0") << ","
                    << gps.mode << ","
                    << valueOrBlank(gps.latitude, 7) << ","
                    << valueOrBlank(gps.longitude, 7) << ","
                    << valueOrBlank(gps.speedMps, 6) << ","
                    << valueOrBlank(gps.speedKmh, 3) << ","
                    << valueOrBlank(gps.speedKnots, 3) << ","
                    << valueOrBlank(gps.courseDeg, 2) << ","
                    << valueOrBlank(imu.pitch, 2) << ","
                    << valueOrBlank(imu.roll, 2) << ","
                    << valueOrBlank(imu.yaw, 2) << ","
                    << valueOrBlank(relativePitch, 2) << ","
                    << valueOrBlank(relativeRoll, 2) << ","
                    << valueOrBlank(relativeYaw, 2) << ","
                    << valueOrBlank(yawChange, 2) << ","
                    << slopeStatus << ","
                    << turnStatus << "\n";

            csvFile.flush();

            std::cout << "\033[2J\033[H";

            std::cout << "Combined Navigation Data V4\n";
            std::cout << "---------------------------------------------------------------\n";

            std::cout << "System Status: "
                      << ((gps.validFix && imuOk) ? "OK" : "WARNING")
                      << std::endl;

            std::cout << "GPS Status: "
                      << (gps.validFix ? "Valid Fix" : "No Valid Fix")
                      << std::endl;

            std::cout << "IMU Status: "
                      << (imuOk ? "Active" : "SPI Read Error")
                      << std::endl;

            std::cout << "Calibration: "
                      << (imuCalibrated ? "Initial angle set as zero" : "Not calibrated")
                      << std::endl;

            std::cout << "Slope Status: " << slopeStatus << std::endl;
            std::cout << "Turn Status: " << turnStatus << std::endl;

            std::cout << "CSV Log: " << csvFilename << std::endl;

            std::cout << "---------------------------------------------------------------\n";

            std::cout << std::left << std::setw(35)
                      << ("Date: " + (gps.hasDate ? gps.date : "not available"))
                      << " | "
                      << "Pitch: " << valueOrNA(relativePitch, 2) << " deg"
                      << std::endl;

            std::cout << std::left << std::setw(35)
                      << ("UTC Time: " + (gps.hasTime ? gps.utcTime + " UTC" : "not available"))
                      << " | "
                      << "Roll: " << valueOrNA(relativeRoll, 2) << " deg"
                      << std::endl;

            std::cout << std::left << std::setw(35)
                      << ("Latitude: " + valueOrNA(gps.latitude, 6))
                      << " | "
                      << "Yaw: " << valueOrNA(relativeYaw, 2) << " deg"
                      << std::endl;

            std::cout << std::left << std::setw(35)
                      << ("Longitude: " + valueOrNA(gps.longitude, 6))
                      << " | "
                      << "Yaw Change: " << valueOrNA(yawChange, 2) << " deg"
                      << std::endl;

            std::cout << std::left << std::setw(35)
                      << ("Speed m/s: " + valueOrNA(gps.speedMps, 6))
                      << " | "
                      << "Slope: " << slopeStatus
                      << std::endl;

            std::cout << std::left << std::setw(35)
                      << ("Speed km/h: " + valueOrNA(gps.speedKmh, 3))
                      << " | "
                      << "Turn: " << turnStatus
                      << std::endl;

            std::cout << std::left << std::setw(35)
                      << ("Speed knots: " + valueOrNA(gps.speedKnots, 3))
                      << " | "
                      << "GPS Mode: " << (gps.mode.empty() ? "not available" : gps.mode)
                      << std::endl;

            std::string courseText = gps.hasCourse ? valueOrNA(gps.courseDeg, 2) + " deg"
                                                   : "not available";

            std::cout << std::left << std::setw(35)
                      << ("Course: " + courseText)
                      << " | "
                      << "Motion Analysis: Active"
                      << std::endl;

            std::cout << "---------------------------------------------------------------\n";
            std::cout << "Note:\n";
            std::cout << "- Displayed UM7 angles are relative to initial calibration.\n";
            std::cout << "- Relative UM7 angles are normalized to -180 to +180 degrees.\n";
            std::cout << "- Raw angles and relative angles are both saved in the CSV file.\n";
            std::cout << "- Slope Status uses relative Pitch.\n";
            std::cout << "- Turn Status uses Yaw Change and GPS Speed.\n";
            std::cout << "- Positive Yaw Change = Right Turn based on your clockwise test.\n";
            std::cout << "---------------------------------------------------------------\n";
        }

        usleep(200000);
    }

    close(gpsFd);
    close(spiFd);

    return 0;
}
