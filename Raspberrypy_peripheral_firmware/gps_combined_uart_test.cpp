#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>

struct GPSData {
    bool hasFix = false;

    std::string date = "not available";
    std::string utcTime = "not available";

    double latitude = 0.0;
    double longitude = 0.0;

    double speedKnots = 0.0;
    double speedKmh = 0.0;
    double speedMs = 0.0;

    bool hasCourse = false;
    double course = 0.0;

    std::string mode = "not available";
};

std::vector<std::string> split(const std::string& line, char delimiter) {
    std::vector<std::string> result;
    std::stringstream ss(line);
    std::string item;

    while (std::getline(ss, item, delimiter)) {
        result.push_back(item);
    }

    return result;
}

std::string removeChecksum(const std::string& field) {
    std::size_t pos = field.find('*');
    if (pos != std::string::npos) {
        return field.substr(0, pos);
    }
    return field;
}

double convertToDecimalDegrees(const std::string& nmeaCoord, const std::string& direction) {
    if (nmeaCoord.empty() || direction.empty()) {
        return 0.0;
    }

    double raw = std::stod(nmeaCoord);
    int degrees = static_cast<int>(raw / 100);
    double minutes = raw - degrees * 100;
    double decimal = degrees + minutes / 60.0;

    if (direction == "S" || direction == "W") {
        decimal = -decimal;
    }

    return decimal;
}

std::string formatUtcTime(const std::string& utcRaw) {
    if (utcRaw.length() < 6) {
        return "not available";
    }

    std::string hour = utcRaw.substr(0, 2);
    std::string minute = utcRaw.substr(2, 2);
    std::string second = utcRaw.substr(4, 2);

    return hour + ":" + minute + ":" + second + " UTC";
}

std::string formatRmcDate(const std::string& dateRaw) {
    /*
        RMC date format:
        DDMMYY

        Example:
        120626 -> 2026-06-12
    */
    if (dateRaw.length() != 6) {
        return "not available";
    }

    std::string day = dateRaw.substr(0, 2);
    std::string month = dateRaw.substr(2, 2);
    std::string year = dateRaw.substr(4, 2);

    return "20" + year + "-" + month + "-" + day;
}

int openSerialPort(const char* device) {
    int fd = open(device, O_RDWR | O_NOCTTY | O_SYNC);

    if (fd < 0) {
        std::cerr << "Error: Cannot open serial port " << device << std::endl;
        return -1;
    }

    termios tty{};

    if (tcgetattr(fd, &tty) != 0) {
        std::cerr << "Error: Cannot get serial port settings" << std::endl;
        close(fd);
        return -1;
    }

    cfsetispeed(&tty, B115200);
    cfsetospeed(&tty, B115200);

    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
    tty.c_iflag &= ~IGNBRK;
    tty.c_lflag = 0;
    tty.c_oflag = 0;

    tty.c_cc[VMIN] = 1;
    tty.c_cc[VTIME] = 1;

    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~(PARENB | PARODD);
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CRTSCTS;

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        std::cerr << "Error: Cannot set serial port settings" << std::endl;
        close(fd);
        return -1;
    }

    return fd;
}

bool parseGLL(const std::string& line, GPSData& data) {
    std::vector<std::string> fields = split(line, ',');

    /*
        GLL example:
        $GNGLL,3401.80980,N,11805.36473,W,223937.00,A,A*xx

        fields[1] = latitude
        fields[2] = N/S
        fields[3] = longitude
        fields[4] = E/W
        fields[5] = UTC time
        fields[6] = status A/V
    */
    if (fields.size() < 7) {
        return false;
    }

    std::string status = fields[6];

    if (status == "A") {
        data.hasFix = true;
        data.latitude = convertToDecimalDegrees(fields[1], fields[2]);
        data.longitude = convertToDecimalDegrees(fields[3], fields[4]);
        data.utcTime = formatUtcTime(fields[5]);
        return true;
    }

    return false;
}

bool parseVTG(const std::string& line, GPSData& data) {
    std::vector<std::string> fields = split(line, ',');

    /*
        VTG example:
        $GNVTG,,T,,M,0.032,N,0.060,K,D*xx

        fields[1] = true course
        fields[5] = speed in knots
        fields[7] = speed in km/h
        fields[9] = mode
    */
    if (fields.size() < 9) {
        return false;
    }

    if (!fields[1].empty()) {
        data.course = std::stod(fields[1]);
        data.hasCourse = true;
    } else {
        data.hasCourse = false;
    }

    if (!fields[5].empty()) {
        data.speedKnots = std::stod(fields[5]);
    }

    if (!fields[7].empty()) {
        data.speedKmh = std::stod(fields[7]);
        data.speedMs = data.speedKmh / 3.6;
    }

    if (fields.size() > 9) {
        data.mode = removeChecksum(fields[9]);
    }

    return true;
}

bool parseRMC(const std::string& line, GPSData& data) {
    std::vector<std::string> fields = split(line, ',');

    /*
        RMC example:
        $GNRMC,223937.00,A,3401.80980,N,11805.36473,W,0.009,,120626,,,D,V*02

        fields[1] = UTC time
        fields[2] = status A/V
        fields[3] = latitude
        fields[4] = N/S
        fields[5] = longitude
        fields[6] = E/W
        fields[7] = speed in knots
        fields[8] = course
        fields[9] = date DDMMYY
        fields[12] = mode, may contain checksum depending on output
    */
    if (fields.size() < 10) {
        return false;
    }

    std::string status = fields[2];

    if (status == "A") {
        data.hasFix = true;

        data.utcTime = formatUtcTime(fields[1]);
        data.latitude = convertToDecimalDegrees(fields[3], fields[4]);
        data.longitude = convertToDecimalDegrees(fields[5], fields[6]);

        if (!fields[7].empty()) {
            data.speedKnots = std::stod(fields[7]);
            data.speedKmh = data.speedKnots * 1.852;
            data.speedMs = data.speedKmh / 3.6;
        }

        if (!fields[8].empty()) {
            data.course = std::stod(fields[8]);
            data.hasCourse = true;
        }

        if (!fields[9].empty()) {
            data.date = formatRmcDate(fields[9]);
        }

        if (fields.size() > 12) {
            data.mode = removeChecksum(fields[12]);
        }

        return true;
    }

    return false;
}

void printGPSData(const GPSData& data) {
    std::cout << "\nGPS Combined Data" << std::endl;

    if (!data.hasFix) {
        std::cout << "GPS Fix: Not valid yet" << std::endl;
        return;
    }

    std::cout << "GPS Fix: Valid" << std::endl;
    std::cout << "Date: " << data.date << " (GPS RMC date)" << std::endl;
    std::cout << "UTC Time: " << data.utcTime << std::endl;

    std::cout << std::fixed << std::setprecision(6);
    std::cout << "Latitude: " << data.latitude << std::endl;
    std::cout << "Longitude: " << data.longitude << std::endl;

    std::cout << "Speed: " << data.speedMs << " m/s" << std::endl;
    std::cout << "Speed: " << data.speedKmh << " km/h" << std::endl;
    std::cout << "Speed: " << data.speedKnots << " knots" << std::endl;

    if (data.hasCourse) {
        std::cout << "Direction/Course: " << data.course << " degrees" << std::endl;
    } else {
        std::cout << "Direction/Course: not available" << std::endl;
    }

    std::cout << "Mode: " << data.mode << std::endl;
}

int main() {
    const char* device = "/dev/serial0";

    int fd = openSerialPort(device);
    if (fd < 0) {
        return 1;
    }

    std::cout << "Reading combined GPS data from " << device << "..." << std::endl;
    std::cout << "Baud rate: 115200" << std::endl;
    std::cout << "Date source: GPS RMC sentence" << std::endl;
    std::cout << "Press Ctrl+C to stop." << std::endl;

    GPSData gpsData;
    std::string line;
    char ch;

    while (true) {
        int n = read(fd, &ch, 1);

        if (n > 0) {
            if (ch == '\n') {
                if (!line.empty()) {
                    if (line.find("$GNRMC") == 0 || line.find("$GPRMC") == 0) {
                        parseRMC(line, gpsData);
                    }

                    if (line.find("$GNGLL") == 0 || line.find("$GPGLL") == 0) {
                        parseGLL(line, gpsData);
                    }

                    if (line.find("$GNVTG") == 0 || line.find("$GPVTG") == 0) {
                        parseVTG(line, gpsData);
                    }

                    if (gpsData.hasFix) {
                        printGPSData(gpsData);
                    }
                }

                line.clear();
            } else if (ch != '\r') {
                line += ch;
            }
        }
    }

    close(fd);
    return 0;
}
