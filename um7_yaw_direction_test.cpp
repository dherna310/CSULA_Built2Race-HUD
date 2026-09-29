/*
 * um7_yaw_direction_test.cpp
 *
 * Simple UM7 Yaw Direction Test
 *
 * Purpose:
 *   Test whether positive yaw change means left turn or right turn
 *   for the actual UM7 mounting direction.
 *
 * Device:
 *   UM7 IMU through SPI
 *   /dev/spidev0.0
 *   SPI mode 0
 *   500000 Hz
 *
 * Compile:
 *   g++ src/um7_yaw_direction_test.cpp -o um7_yaw_direction_test
 *
 * Run:
 *   ./um7_yaw_direction_test
 *
 * Test:
 *   Keep the UM7 flat.
 *   Rotate it clockwise and counterclockwise.
 *   Watch whether yaw change is positive or negative.
 */

#include <iostream>
#include <iomanip>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cerrno>

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>

static const char *SPI_DEVICE = "/dev/spidev0.0";
static const uint8_t SPI_MODE_VALUE = SPI_MODE_0;
static const uint8_t SPI_BITS_PER_WORD = 8;
static const uint32_t SPI_SPEED_HZ = 500000;

// UM7 Euler yaw register
static const uint8_t DREG_EULER_PSI = 0x71;

// UM7 Euler angle scale
static const double UM7_EULER_SCALE = 91.02222;

// Test threshold
static const double YAW_CHANGE_THRESHOLD_DEG = 3.0;

uint32_t bytesToU32BE(uint8_t b3, uint8_t b2, uint8_t b1, uint8_t b0) {
    return (static_cast<uint32_t>(b3) << 24) |
           (static_cast<uint32_t>(b2) << 16) |
           (static_cast<uint32_t>(b1) << 8)  |
           static_cast<uint32_t>(b0);
}

int16_t highWordSigned(uint32_t word) {
    return static_cast<int16_t>((word >> 16) & 0xFFFF);
}

double angleDifference(double current, double previous) {
    if (!std::isfinite(current) || !std::isfinite(previous)) {
        return 0.0;
    }

    double diff = current - previous;

    while (diff > 180.0) {
        diff -= 360.0;
    }

    while (diff < -180.0) {
        diff += 360.0;
    }

    return diff;
}

int openSPI() {
    int fd = open(SPI_DEVICE, O_RDWR);

    if (fd < 0) {
        std::cerr << "[ERROR] Cannot open " << SPI_DEVICE
                  << ": " << std::strerror(errno) << std::endl;
        return -1;
    }

    uint8_t mode = SPI_MODE_VALUE;
    uint8_t bits = SPI_BITS_PER_WORD;
    uint32_t speed = SPI_SPEED_HZ;

    if (ioctl(fd, SPI_IOC_WR_MODE, &mode) < 0) {
        std::cerr << "[ERROR] Cannot set SPI mode: "
                  << std::strerror(errno) << std::endl;
        close(fd);
        return -1;
    }

    if (ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0) {
        std::cerr << "[ERROR] Cannot set SPI bits per word: "
                  << std::strerror(errno) << std::endl;
        close(fd);
        return -1;
    }

    if (ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0) {
        std::cerr << "[ERROR] Cannot set SPI speed: "
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

bool readYaw(int spiFd, double &yawDeg) {
    uint32_t psi = 0;

    if (!readUM7Register32(spiFd, DREG_EULER_PSI, psi)) {
        return false;
    }

    int16_t yawRaw = highWordSigned(psi);
    yawDeg = static_cast<double>(yawRaw) / UM7_EULER_SCALE;

    return true;
}

std::string classifyDirection(double yawChange) {
    if (yawChange > YAW_CHANGE_THRESHOLD_DEG) {
        return "Positive yaw change";
    }

    if (yawChange < -YAW_CHANGE_THRESHOLD_DEG) {
        return "Negative yaw change";
    }

    return "No clear rotation";
}

int main() {
    std::cout << "UM7 Yaw Direction Test\n";
    std::cout << "Device: " << SPI_DEVICE << std::endl;
    std::cout << "SPI Mode: 0" << std::endl;
    std::cout << "SPI Speed: " << SPI_SPEED_HZ << " Hz" << std::endl;
    std::cout << "--------------------------------------------------\n";
    std::cout << "Instruction:\n";
    std::cout << "1. Keep the UM7 flat.\n";
    std::cout << "2. Rotate it clockwise.\n";
    std::cout << "3. Rotate it counterclockwise.\n";
    std::cout << "4. Check whether yaw change is positive or negative.\n";
    std::cout << "--------------------------------------------------\n";

    int spiFd = openSPI();

    if (spiFd < 0) {
        return 1;
    }

    double previousYaw = NAN;
    int sample = 0;

    while (true) {
        double currentYaw = NAN;

        bool ok = readYaw(spiFd, currentYaw);

        if (!ok) {
            std::cerr << "[ERROR] Failed to read UM7 yaw register." << std::endl;
            usleep(500000);
            continue;
        }

        double yawChange = 0.0;

        if (std::isfinite(previousYaw)) {
            yawChange = angleDifference(currentYaw, previousYaw);
        }

        previousYaw = currentYaw;
        sample++;

        std::string direction = classifyDirection(yawChange);

        std::cout << "\033[2J\033[H";
        std::cout << "UM7 Yaw Direction Test\n";
        std::cout << "--------------------------------------------------\n";
        std::cout << "Sample: " << sample << std::endl;
        std::cout << "Yaw: " << std::fixed << std::setprecision(2)
                  << currentYaw << " deg" << std::endl;
        std::cout << "Yaw Change: " << std::fixed << std::setprecision(2)
                  << yawChange << " deg" << std::endl;
        std::cout << "Direction Result: " << direction << std::endl;
        std::cout << "--------------------------------------------------\n";
        std::cout << "How to use this result:\n";
        std::cout << "- Rotate the UM7 clockwise.\n";
        std::cout << "- If Yaw Change is positive, then positive yaw = clockwise.\n";
        std::cout << "- If Yaw Change is negative, then negative yaw = clockwise.\n";
        std::cout << "\n";
        std::cout << "Then decide V3 labels:\n";
        std::cout << "- If clockwise means vehicle right turn, map that sign to Right Turn.\n";
        std::cout << "- If counterclockwise means vehicle left turn, map that sign to Left Turn.\n";
        std::cout << "--------------------------------------------------\n";
        std::cout << "Press Ctrl + C to stop.\n";

        usleep(500000);
    }

    close(spiFd);
    return 0;
}
