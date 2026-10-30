#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <sstream>
#include <vector>
#include <string>
#include <iomanip>
#include <ctime>

struct GPSData {
    bool locationValid = false;
    bool speedValid = false;
    bool courseAvailable = false;

    std::string utcTime;
    std::string systemDate;

    double latitude = 0.0;
    double longitude = 0.0;

    double speedKnots = 0.0;
    double speedKmh = 0.0;
    double speedMps = 0.0;

    double courseDegrees = 0.0;
    std::string mode;
};

std::vector<std::string> split(const std::string& line, char delimiter) {
    std::vector<std::string> fields;
    std::stringstream ss(line);
    std::string item;

    while (std::getline(ss, item, delimiter)) {
        fields.push_back(item);
    }

    return fields;
}

std::string removeChecksum(const std::string& field) {
    size_t pos = field.find('*');
    if (pos != std::string::npos) {
        return field.substr(0, pos);
    }
    return field;
}

double convertToDecimalDegrees(const std::string& nmeaCoord, const std::string& direction) {
    if (nmeaCoord.empty()) {
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

std::string formatUtcTime(const std::string& utcTime) {
    if (utcTime.length() < 6) {
        return utcTime;
    }

    std::string hour = utcTime.substr(0, 2);
    std::string minute = utcTime.substr(2, 2);
    std::string second = utcTime.substr(4, 2);

    return hour + ":" + minute + ":" + second + " UTC";
}

std::string getSystemDate() {
    std::time_t now = std::time(nullptr);
    std::tm* localTime = std::localtime(&now);

    char buffer[20];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d", localTime);

    return std::string(buffer);
}

int openSerialPort(const char* device) {
    int fd = open(device, O_RDONLY | O_NOCTTY);

    if (fd < 0) {
        std::cerr << "Failed to open device: " << device << std::endl;
        return -1;
    }

    termios tty{};
    if (tcgetattr(fd, &tty) != 0) {
        std::cerr << "Failed to get serial port attributes." << std::endl;
        close(fd);
        return -1;
    }

    cfsetispeed(&tty, B9600);
    cfsetospeed(&tty, B9600);

    tty.c_cflag |= CLOCAL | CREAD;
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;
    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CRTSCTS;

    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_oflag &= ~OPOST;

    tty.c_cc[VMIN] = 1;
    tty.c_cc[VTIME] = 1;

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        std::cerr << "Failed to set serial port attributes." << std::endl;
        close(fd);
        return -1;
    }

    return fd;
}

bool parseGLL(const std::string& line, GPSData& gps) {
    std::vector<std::string> fields = split(line, ',');

    /*
      GLL format:
      $GNGLL,latitude,N/S,longitude,E/W,utc_time,status,mode*checksum

      fields[1] = latitude
      fields[2] = N/S
      fields[3] = longitude
      fields[4] = E/W
      fields[5] = UTC time
      fields[6] = status, A=valid, V=invalid
    */

    if (fields.size() < 7) {
        return false;
    }

    std::string status = fields[6];

    if (status != "A") {
        gps.locationValid = false;
        return false;
    }

    gps.latitude = convertToDecimalDegrees(fields[1], fields[2]);
    gps.longitude = convertToDecimalDegrees(fields[3], fields[4]);
    gps.utcTime = fields[5];
    gps.systemDate = getSystemDate();
    gps.locationValid = true;

    return true;
}

bool parseVTG(const std::string& line, GPSData& gps) {
    std::vector<std::string> fields = split(line, ',');

    /*
      VTG format:
      $GNVTG,true_course,T,magnetic_course,M,speed_knots,N,speed_kmh,K,mode*checksum

      fields[1] = true course in degrees
      fields[5] = speed in knots
      fields[7] = speed in km/h
      fields[9] = mode + checksum
    */

    if (fields.size() < 9) {
        return false;
    }

    if (!fields[1].empty()) {
        gps.courseDegrees = std::stod(fields[1]);
        gps.courseAvailable = true;
    } else {
        gps.courseAvailable = false;
    }

    if (!fields[5].empty()) {
        gps.speedKnots = std::stod(fields[5]);
    }

    if (!fields[7].empty()) {
        gps.speedKmh = std::stod(fields[7]);
    }

    gps.speedMps = gps.speedKmh / 3.6;

    if (fields.size() >= 10) {
        gps.mode = removeChecksum(fields[9]);
    }

    gps.speedValid = true;
    return true;
}

void printGPSData(const GPSData& gps) {
    std::cout << std::fixed << std::setprecision(6);

    std::cout << "GPS Combined Data" << std::endl;

    if (gps.locationValid) {
        std::cout << "Date: " << gps.systemDate << " (Raspberry Pi system date)" << std::endl;
        std::cout << "UTC Time: " << formatUtcTime(gps.utcTime) << std::endl;
        std::cout << "Latitude: " << gps.latitude << std::endl;
        std::cout << "Longitude: " << gps.longitude << std::endl;
    } else {
        std::cout << "Location: not valid yet" << std::endl;
    }

    if (gps.speedValid) {
        std::cout << "Speed: " << gps.speedMps << " m/s" << std::endl;
        std::cout << "Speed: " << gps.speedKmh << " km/h" << std::endl;
        std::cout << "Speed: " << gps.speedKnots << " knots" << std::endl;

        if (gps.courseAvailable) {
            std::cout << "Direction/Course: " << gps.courseDegrees << " degrees" << std::endl;
        } else {
            std::cout << "Direction/Course: not available" << std::endl;
        }

        std::cout << "Mode: " << gps.mode << std::endl;
    } else {
        std::cout << "Speed: not available yet" << std::endl;
    }

    std::cout << "------------------------" << std::endl;
}

int main() {
    const char* device = "/dev/ttyACM0";

    int fd = openSerialPort(device);
    if (fd < 0) {
        return 1;
    }

    std::cout << "Reading combined GPS data from " << device << "..." << std::endl;

    std::string line;
    char ch;
    GPSData gps;

    while (true) {
        int n = read(fd, &ch, 1);

        if (n > 0) {
            if (ch == '\n') {
                bool updated = false;

                if (line.find("$GNGLL") == 0 || line.find("$GPGLL") == 0) {
                    updated = parseGLL(line, gps);
                }

                if (line.find("$GNVTG") == 0 || line.find("$GPVTG") == 0) {
                    updated = parseVTG(line, gps);
                }

                if (updated && gps.locationValid) {
                    printGPSData(gps);
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
