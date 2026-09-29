#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>
#include <cstring>
#include <cstdint>
#include <cmath>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>

const char* GPS_DEVICE = "/dev/serial0";
const char* SPI_DEVICE = "/dev/spidev0.0";

const double IMU_SCALE = 91.02222;

// =========================
// Error / warning messages
// =========================
void printPiError(const std::string& message) {
    std::cerr << "[PI ERROR] " << message << std::endl;
}

void printPiWarning(const std::string& message) {
    std::cerr << "[PI WARNING] " << message << std::endl;
}

void printGpsError(const std::string& message) {
    std::cerr << "[GPS ERROR] " << message << std::endl;
}

void printGpsWarning(const std::string& message) {
    std::cerr << "[GPS WARNING] " << message << std::endl;
}

void printImuError(const std::string& message) {
    std::cerr << "[IMU ERROR] " << message << std::endl;
}

void printImuWarning(const std::string& message) {
    std::cerr << "[IMU WARNING] " << message << std::endl;
}

// =========================
// GPS data structure
// =========================
struct GPSData {
    bool deviceOpen = false;
    bool dataReceived = false;
    bool fixValid = false;

    std::string date = "not available";
    std::string utcTime = "not available";
    std::string latitude = "not available";
    std::string longitude = "not available";

    std::string speedMs = "not available";
    std::string speedKmh = "not available";
    std::string speedKnots = "not available";

    std::string course = "not available";
    std::string mode = "not available";
};

// =========================
// IMU data structure
// =========================
struct IMUData {
    bool deviceOpen = false;
    bool active = false;
    bool transferOk = false;

    double pitch = 0.0;
    double roll = 0.0;
    double yaw = 0.0;
};

// =========================
// Utility functions
// =========================
std::vector<std::string> splitCSV(const std::string& line) {
    std::vector<std::string> fields;
    std::stringstream ss(line);
    std::string item;

    while (std::getline(ss, item, ',')) {
        fields.push_back(item);
    }

    return fields;
}

std::string formatUtcTime(const std::string& raw) {
    if (raw.length() < 6) {
        return "not available";
    }

    std::string hh = raw.substr(0, 2);
    std::string mm = raw.substr(2, 2);
    std::string ss = raw.substr(4, 2);

    return hh + ":" + mm + ":" + ss + " UTC";
}

std::string formatRmcDate(const std::string& raw) {
    if (raw.length() != 6) {
        return "not available";
    }

    std::string dd = raw.substr(0, 2);
    std::string mm = raw.substr(2, 2);
    std::string yy = raw.substr(4, 2);

    int year = 2000 + std::stoi(yy);

    return std::to_string(year) + "-" + mm + "-" + dd;
}

double nmeaToDecimal(const std::string& value, const std::string& direction) {
    if (value.empty() || direction.empty()) {
        return 0.0;
    }

    double raw = std::stod(value);
    int degrees = static_cast<int>(raw / 100);
    double minutes = raw - degrees * 100;
    double decimal = degrees + minutes / 60.0;

    if (direction == "S" || direction == "W") {
        decimal = -decimal;
    }

    return decimal;
}

std::string doubleToString(double value, int precision = 6) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision) << value;
    return oss.str();
}

// =========================
// GPS parsing
// =========================
void updateSpeedFromKnots(double speedKnots, GPSData& gps) {
    double speedKmh = speedKnots * 1.852;
    double speedMs = speedKmh / 3.6;

    gps.speedKnots = doubleToString(speedKnots, 3);
    gps.speedKmh = doubleToString(speedKmh, 3);
    gps.speedMs = doubleToString(speedMs, 6);
}

void updateSpeedFromKmh(double speedKmh, GPSData& gps) {
    double speedMs = speedKmh / 3.6;
    double speedKnots = speedKmh / 1.852;

    gps.speedKmh = doubleToString(speedKmh, 3);
    gps.speedMs = doubleToString(speedMs, 6);
    gps.speedKnots = doubleToString(speedKnots, 3);
}

void parseRMC(const std::string& line, GPSData& gps) {
    std::vector<std::string> f = splitCSV(line);

    if (f.size() < 10) {
        return;
    }

    gps.dataReceived = true;

    // RMC format:
    // $GNRMC,time,status,lat,N/S,lon,E/W,speed_knots,course,date,...
    std::string timeRaw = f[1];
    std::string status = f[2];

    gps.utcTime = formatUtcTime(timeRaw);
    gps.fixValid = (status == "A");

    if (!f[9].empty()) {
        gps.date = formatRmcDate(f[9]);
    }

    if (gps.fixValid) {
        if (!f[3].empty() && !f[4].empty()) {
            double lat = nmeaToDecimal(f[3], f[4]);
            gps.latitude = doubleToString(lat, 6);
        }

        if (!f[5].empty() && !f[6].empty()) {
            double lon = nmeaToDecimal(f[5], f[6]);
            gps.longitude = doubleToString(lon, 6);
        }

        if (!f[7].empty()) {
            double speedKnots = std::stod(f[7]);
            updateSpeedFromKnots(speedKnots, gps);
        }

        if (!f[8].empty()) {
            gps.course = doubleToString(std::stod(f[8]), 2) + " deg";
        } else {
            gps.course = "not available";
        }
    }

    if (f.size() > 12) {
        std::string modeField = f[12];
        size_t starPos = modeField.find('*');
        if (starPos != std::string::npos) {
            modeField = modeField.substr(0, starPos);
        }
        if (!modeField.empty()) {
            gps.mode = modeField;
        }
    }
}

void parseGLL(const std::string& line, GPSData& gps) {
    std::vector<std::string> f = splitCSV(line);

    if (f.size() < 7) {
        return;
    }

    gps.dataReceived = true;

    // GLL format:
    // $GNGLL,lat,N/S,lon,E/W,time,status,mode
    if (!f[1].empty() && !f[2].empty()) {
        double lat = nmeaToDecimal(f[1], f[2]);
        gps.latitude = doubleToString(lat, 6);
    }

    if (!f[3].empty() && !f[4].empty()) {
        double lon = nmeaToDecimal(f[3], f[4]);
        gps.longitude = doubleToString(lon, 6);
    }

    if (!f[5].empty()) {
        gps.utcTime = formatUtcTime(f[5]);
    }

    if (!f[6].empty()) {
        gps.fixValid = (f[6] == "A");
    }
}

void parseVTG(const std::string& line, GPSData& gps) {
    std::vector<std::string> f = splitCSV(line);

    if (f.size() < 9) {
        return;
    }

    gps.dataReceived = true;

    // VTG format:
    // $GNVTG,course,T,,M,speed_knots,N,speed_kmh,K,mode
    if (!f[1].empty()) {
        gps.course = doubleToString(std::stod(f[1]), 2) + " deg";
    }

    if (!f[7].empty()) {
        double speedKmh = std::stod(f[7]);
        updateSpeedFromKmh(speedKmh, gps);
    }
}

void parseGPSLine(const std::string& line, GPSData& gps) {
    if (line.find("RMC") != std::string::npos) {
        parseRMC(line, gps);
    } else if (line.find("GLL") != std::string::npos) {
        parseGLL(line, gps);
    } else if (line.find("VTG") != std::string::npos) {
        parseVTG(line, gps);
    }
}

// =========================
// GPS UART setup
// =========================
int setupGPS(GPSData& gps) {
    int fd = open(GPS_DEVICE, O_RDWR | O_NOCTTY | O_NONBLOCK);

    if (fd < 0) {
        printGpsError("Cannot open /dev/serial0.");
        printGpsError("Check GPS UART wiring: GPS TX2 -> Pi Pin 10 / GPIO15 / RXD.");
        printGpsError("Check GPS 3.3V, GND, and Raspberry Pi serial configuration.");
        return -1;
    }

    termios tty{};
    if (tcgetattr(fd, &tty) != 0) {
        printGpsError("Cannot get GPS serial attributes.");
        printPiError("The serial device exists, but Raspberry Pi could not read its configuration.");
        close(fd);
        return -1;
    }

    cfsetispeed(&tty, B115200);
    cfsetospeed(&tty, B115200);

    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;
    tty.c_cflag &= ~CRTSCTS;
    tty.c_cflag |= CREAD | CLOCAL;

    tty.c_lflag &= ~ICANON;
    tty.c_lflag &= ~ECHO;
    tty.c_lflag &= ~ECHOE;
    tty.c_lflag &= ~ECHONL;
    tty.c_lflag &= ~ISIG;

    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);

    tty.c_oflag &= ~OPOST;
    tty.c_oflag &= ~ONLCR;

    tty.c_cc[VTIME] = 0;
    tty.c_cc[VMIN] = 0;

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        printGpsError("Cannot set GPS serial attributes.");
        printPiError("Raspberry Pi serial configuration failed.");
        close(fd);
        return -1;
    }

    gps.deviceOpen = true;
    return fd;
}

// =========================
// IMU SPI setup and reading
// =========================
int spiTransfer(int fd, const uint8_t* tx, uint8_t* rx, size_t len, uint32_t speed) {
    spi_ioc_transfer tr{};
    tr.tx_buf = reinterpret_cast<unsigned long>(tx);
    tr.rx_buf = reinterpret_cast<unsigned long>(rx);
    tr.len = len;
    tr.speed_hz = speed;
    tr.bits_per_word = 8;
    tr.delay_usecs = 0;

    return ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
}

int16_t makeInt16(uint8_t high, uint8_t low) {
    return static_cast<int16_t>((static_cast<uint16_t>(high) << 8) | low);
}

double normalizeSignedAngle(double angle) {
    while (angle > 180.0) {
        angle -= 360.0;
    }

    while (angle < -180.0) {
        angle += 360.0;
    }

    return angle;
}

double normalizeYaw360(double angle) {
    while (angle < 0.0) {
        angle += 360.0;
    }

    while (angle >= 360.0) {
        angle -= 360.0;
    }

    return angle;
}

double rawToSignedDegrees(int16_t raw) {
    double angle = raw / IMU_SCALE;
    return normalizeSignedAngle(angle);
}

double rawToYawDegrees(int16_t raw) {
    double angle = raw / IMU_SCALE;
    return normalizeYaw360(angle);
}

int setupIMU(IMUData& imu, uint32_t speed) {
    int fd = open(SPI_DEVICE, O_RDWR);

    if (fd < 0) {
        printImuError("Cannot open /dev/spidev0.0.");
        printImuError("Check that SPI is enabled: sudo raspi-config -> Interface Options -> SPI.");
        printImuError("Check IMU SPI wiring: MOSI, MISO, SCLK, CE0, 3.3V, GND.");
        return -1;
    }

    uint8_t mode = SPI_MODE_0;
    uint8_t bits = 8;

    if (ioctl(fd, SPI_IOC_WR_MODE, &mode) < 0) {
        printImuError("Cannot set SPI mode.");
        printPiError("Raspberry Pi SPI configuration failed.");
        close(fd);
        return -1;
    }

    if (ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0) {
        printImuError("Cannot set SPI bits per word.");
        printPiError("Raspberry Pi SPI configuration failed.");
        close(fd);
        return -1;
    }

    if (ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0) {
        printImuError("Cannot set SPI speed.");
        printPiError("Raspberry Pi SPI speed configuration failed.");
        close(fd);
        return -1;
    }

    imu.deviceOpen = true;
    return fd;
}

bool readIMU(int spiFd, IMUData& imu, uint32_t speed) {
    uint8_t tx70[8] = {0x70, 0, 0, 0, 0, 0, 0, 0};
    uint8_t tx71[8] = {0x71, 0, 0, 0, 0, 0, 0, 0};

    uint8_t rx70[8];
    uint8_t rx71[8];

    std::memset(rx70, 0, sizeof(rx70));
    std::memset(rx71, 0, sizeof(rx71));

    if (spiTransfer(spiFd, tx70, rx70, sizeof(rx70), speed) < 0) {
        imu.transferOk = false;
        imu.active = false;
        return false;
    }

    usleep(20000);

    if (spiTransfer(spiFd, tx71, rx71, sizeof(rx71), speed) < 0) {
        imu.transferOk = false;
        imu.active = false;
        return false;
    }

    // Confirmed byte mapping from testing:
    // Pitch = RX70[1], RX70[2]
    // Roll  = RX70[3], RX70[4]
    // Yaw   = RX71[1], RX71[2]
    int16_t pitchRaw = makeInt16(rx70[1], rx70[2]);
    int16_t rollRaw  = makeInt16(rx70[3], rx70[4]);
    int16_t yawRaw   = makeInt16(rx71[1], rx71[2]);

    imu.pitch = rawToSignedDegrees(pitchRaw);
    imu.roll  = rawToSignedDegrees(rollRaw);
    imu.yaw   = rawToYawDegrees(yawRaw);

    imu.transferOk = true;
    imu.active = true;

    return true;
}

// =========================
// Status text
// =========================
std::string getSystemStatus(const GPSData& gps, const IMUData& imu) {
    if (!gps.deviceOpen || !imu.deviceOpen) {
        return "Device Error";
    }

    if (!imu.active) {
        return "IMU Error";
    }

    if (!gps.dataReceived) {
        return "GPS No Data";
    }

    return "OK";
}

std::string getGpsStatus(const GPSData& gps) {
    if (!gps.deviceOpen) {
        return "UART Device Error";
    }

    if (!gps.dataReceived) {
        return "No UART Data";
    }

    if (!gps.fixValid) {
        return "Receiving Data, No Valid Fix";
    }

    return "Valid Fix";
}

std::string getImuStatus(const IMUData& imu) {
    if (!imu.deviceOpen) {
        return "SPI Device Error";
    }

    if (!imu.transferOk || !imu.active) {
        return "SPI Transfer Error";
    }

    return "Active";
}

// =========================
// Display formatting
// =========================
std::string makeLeftField(const std::string& label, const std::string& value) {
    return label + ": " + value;
}

void printCombinedData(const GPSData& gps, const IMUData& imu) {
    std::string fixText = gps.fixValid ? "Valid" : "Invalid";
    std::string imuText = imu.active ? "Active" : "Inactive";

    std::ostringstream pitchText;
    std::ostringstream rollText;
    std::ostringstream yawText;

    pitchText << std::fixed << std::setprecision(2) << imu.pitch << " deg";
    rollText  << std::fixed << std::setprecision(2) << imu.roll  << " deg";
    yawText   << std::fixed << std::setprecision(2) << imu.yaw   << " deg";

    std::cout << "\nCombined Navigation Data V2\n";
    std::cout << "---------------------------------------------------------------\n";

    std::cout << "System Status: " << getSystemStatus(gps, imu) << std::endl;
    std::cout << "GPS Status: " << getGpsStatus(gps) << std::endl;
    std::cout << "IMU Status: " << getImuStatus(imu) << std::endl;

    std::cout << "---------------------------------------------------------------\n";

    std::cout << std::left << std::setw(32)
              << makeLeftField("GPS Fix", fixText)
              << "| IMU Status: " << imuText << std::endl;

    std::cout << std::left << std::setw(32)
              << makeLeftField("Date", gps.date)
              << "|" << std::endl;

    std::cout << std::left << std::setw(32)
              << makeLeftField("UTC Time", gps.utcTime)
              << "|" << std::endl;

    std::cout << std::left << std::setw(32)
              << makeLeftField("Latitude", gps.latitude)
              << "|" << std::endl;

    std::cout << std::left << std::setw(32)
              << makeLeftField("Longitude", gps.longitude)
              << "| Pitch: " << pitchText.str() << std::endl;

    std::cout << std::left << std::setw(32)
              << makeLeftField("Speed m/s", gps.speedMs)
              << "| Roll: " << rollText.str() << std::endl;

    std::cout << std::left << std::setw(32)
              << makeLeftField("Speed km/h", gps.speedKmh)
              << "| Yaw: " << yawText.str() << std::endl;

    std::cout << std::left << std::setw(32)
              << makeLeftField("Speed knots", gps.speedKnots)
              << "|" << std::endl;

    std::cout << std::left << std::setw(32)
              << makeLeftField("Course", gps.course)
              << "|" << std::endl;

    std::cout << "---------------------------------------------------------------\n";
}

// =========================
// Main program
// =========================
int main() {
    uint32_t imuSpeed = 500000;

    GPSData gps;
    IMUData imu;

    std::cout << "Starting Combined GPS + IMU Navigation Test V2" << std::endl;
    std::cout << "GPS Device: " << GPS_DEVICE << std::endl;
    std::cout << "GPS Baud Rate: 115200" << std::endl;
    std::cout << "IMU Device: " << SPI_DEVICE << std::endl;
    std::cout << "IMU SPI Speed: " << imuSpeed << " Hz" << std::endl;
    std::cout << "Speed output: m/s, km/h, knots" << std::endl;
    std::cout << "Press Ctrl+C to stop." << std::endl;

    int gpsFd = setupGPS(gps);
    if (gpsFd < 0) {
        printPiError("Program stopped because GPS UART could not be initialized.");
        return 1;
    }

    int imuFd = setupIMU(imu, imuSpeed);
    if (imuFd < 0) {
        close(gpsFd);
        printPiError("Program stopped because IMU SPI could not be initialized.");
        return 1;
    }

    std::string gpsBuffer;
    char readBuffer[256];

    int loopCounter = 0;
    int gpsNoDataCounter = 0;
    int imuFailCounter = 0;

    while (true) {
        // Read available GPS serial data
        ssize_t bytesRead = read(gpsFd, readBuffer, sizeof(readBuffer) - 1);

        if (bytesRead > 0) {
            readBuffer[bytesRead] = '\0';
            gpsBuffer += readBuffer;
            gpsNoDataCounter = 0;

            size_t pos;
            while ((pos = gpsBuffer.find('\n')) != std::string::npos) {
                std::string line = gpsBuffer.substr(0, pos);
                gpsBuffer.erase(0, pos + 1);

                if (!line.empty() && line.back() == '\r') {
                    line.pop_back();
                }

                if (!line.empty() && line[0] == '$') {
                    parseGPSLine(line, gps);
                }
            }
        } else {
            gpsNoDataCounter++;
        }

        // Read IMU data
        if (!readIMU(imuFd, imu, imuSpeed)) {
            imuFailCounter++;
            if (imuFailCounter % 10 == 1) {
                printImuError("SPI transfer failed. Check UM7 power, SPI wiring, or /dev/spidev0.0.");
            }
        } else {
            imuFailCounter = 0;
        }

        // Warnings
        if (gpsNoDataCounter == 25) {
            printGpsWarning("No GPS UART data received recently. Check GPS TX2 -> Pi Pin 10 and common GND.");
        }

        if (gps.dataReceived && !gps.fixValid && loopCounter % 25 == 0) {
            printGpsWarning("GPS data is being received, but fix is invalid. Move antenna near window or outside.");
        }

        // Print every about 1 second
        if (loopCounter % 5 == 0) {
            printCombinedData(gps, imu);
        }

        loopCounter++;
        usleep(200000);
    }

    close(gpsFd);
    close(imuFd);

    return 0;
}
