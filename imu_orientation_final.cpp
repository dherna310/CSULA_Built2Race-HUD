#include <iostream>
#include <iomanip>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>

const char* SPI_DEVICE = "/dev/spidev0.0";
const double SCALE = 91.02222;

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
    double angle = raw / SCALE;
    return normalizeSignedAngle(angle);
}

double rawToYawDegrees(int16_t raw) {
    double angle = raw / SCALE;
    return normalizeYaw360(angle);
}

int main() {
    int fd = open(SPI_DEVICE, O_RDWR);

    if (fd < 0) {
        std::cerr << "Error: Cannot open " << SPI_DEVICE << std::endl;
        return 1;
    }

    uint8_t mode = SPI_MODE_0;
    uint8_t bits = 8;
    uint32_t speed = 500000;

    if (ioctl(fd, SPI_IOC_WR_MODE, &mode) < 0) {
        std::cerr << "Error: Cannot set SPI mode" << std::endl;
        close(fd);
        return 1;
    }

    if (ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0) {
        std::cerr << "Error: Cannot set SPI bits per word" << std::endl;
        close(fd);
        return 1;
    }

    if (ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0) {
        std::cerr << "Error: Cannot set SPI speed" << std::endl;
        close(fd);
        return 1;
    }

    std::cout << "UM7 IMU Orientation Final Test" << std::endl;
    std::cout << "Device: " << SPI_DEVICE << std::endl;
    std::cout << "SPI Mode: 0" << std::endl;
    std::cout << "SPI Speed: " << speed << " Hz" << std::endl;
    std::cout << "Pitch/Roll range: -180 to +180 degrees" << std::endl;
    std::cout << "Yaw range: 0 to 360 degrees" << std::endl;
    std::cout << "Press Ctrl+C to stop." << std::endl;
    std::cout << std::endl;

    uint8_t tx70[8] = {0x70, 0, 0, 0, 0, 0, 0, 0};
    uint8_t tx71[8] = {0x71, 0, 0, 0, 0, 0, 0, 0};

    while (true) {
        uint8_t rx70[8];
        uint8_t rx71[8];

        std::memset(rx70, 0, sizeof(rx70));
        std::memset(rx71, 0, sizeof(rx71));

        if (spiTransfer(fd, tx70, rx70, sizeof(rx70), speed) < 0) {
            std::cerr << "Error: SPI transfer failed for register 0x70" << std::endl;
            break;
        }

        usleep(20000);

        if (spiTransfer(fd, tx71, rx71, sizeof(rx71), speed) < 0) {
            std::cerr << "Error: SPI transfer failed for register 0x71" << std::endl;
            break;
        }

        int16_t pitchRaw = makeInt16(rx70[1], rx70[2]);
        int16_t rollRaw  = makeInt16(rx70[3], rx70[4]);
        int16_t yawRaw   = makeInt16(rx71[1], rx71[2]);

        double pitch = rawToSignedDegrees(pitchRaw);
        double roll  = rawToSignedDegrees(rollRaw);
        double yaw   = rawToYawDegrees(yawRaw);

        std::cout << std::fixed << std::setprecision(2);
        std::cout << "Pitch: " << std::setw(8) << pitch << " deg   ";
        std::cout << "Roll: "  << std::setw(8) << roll  << " deg   ";
        std::cout << "Yaw: "   << std::setw(8) << yaw   << " deg" << std::endl;

        usleep(300000);
    }

    close(fd);
    return 0;
}
