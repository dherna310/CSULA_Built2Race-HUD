/*
 * imu_raw_register_test.cpp
 *
 * Purpose:
 *   Show raw UM7 IMU register data before conversion to degrees.
 *
 * Registers:
 *   0x70 = DREG_EULER_PHI_THETA
 *          high 16 bits = Roll raw value
 *          low 16 bits  = Pitch raw value
 *
 *   0x71 = DREG_EULER_PSI
 *          high 16 bits = Yaw raw value
 *
 * Compile:
 *   g++ src/imu_raw_register_test.cpp -o imu_raw_register_test
 *
 * Run:
 *   ./imu_raw_register_test
 */

#include <iostream>
#include <iomanip>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>

static const char *SPI_DEVICE = "/dev/spidev0.0";
static const uint8_t SPI_MODE_SETTING = SPI_MODE_0;
static const uint32_t SPI_SPEED_HZ = 500000;
static const uint8_t SPI_BITS_PER_WORD = 8;

static const uint8_t DREG_EULER_PHI_THETA = 0x70;
static const uint8_t DREG_EULER_PSI       = 0x71;

static int16_t highWordSigned(uint32_t value) {
    return static_cast<int16_t>((value >> 16) & 0xFFFF);
}

static int16_t lowWordSigned(uint32_t value) {
    return static_cast<int16_t>(value & 0xFFFF);
}

/*
 * This function may need to match the SPI read command format already used
 * in your working imu_orientation_final.cpp.
 *
 * It sends a register read command and receives 4 data bytes.
 */
bool readUM7Register32(int spiFd, uint8_t reg, uint32_t &value) {
    uint8_t tx[5] = {0};
    uint8_t rx[5] = {0};

    /*
     * UM7 SPI read command:
     * In many UM7 examples, the register address is sent first,
     * followed by dummy bytes to clock out the register data.
     *
     * If your existing working code uses a slightly different command byte,
     * copy the readUM7Register32() function from imu_orientation_final.cpp.
     */
    tx[0] = reg;

    struct spi_ioc_transfer tr;
    std::memset(&tr, 0, sizeof(tr));

    tr.tx_buf = reinterpret_cast<unsigned long>(tx);
    tr.rx_buf = reinterpret_cast<unsigned long>(rx);
    tr.len = sizeof(tx);
    tr.speed_hz = SPI_SPEED_HZ;
    tr.bits_per_word = SPI_BITS_PER_WORD;
    tr.delay_usecs = 5;

    if (ioctl(spiFd, SPI_IOC_MESSAGE(1), &tr) < 1) {
        return false;
    }

    value = (static_cast<uint32_t>(rx[1]) << 24) |
            (static_cast<uint32_t>(rx[2]) << 16) |
            (static_cast<uint32_t>(rx[3]) << 8)  |
            (static_cast<uint32_t>(rx[4]));

    return true;
}

int main() {
    int spiFd = open(SPI_DEVICE, O_RDWR);
    if (spiFd < 0) {
        std::cerr << "Error: cannot open " << SPI_DEVICE << std::endl;
        return 1;
    }

    uint8_t mode = SPI_MODE_SETTING;
    uint32_t speed = SPI_SPEED_HZ;
    uint8_t bits = SPI_BITS_PER_WORD;

    if (ioctl(spiFd, SPI_IOC_WR_MODE, &mode) < 0) {
        std::cerr << "Error: cannot set SPI mode." << std::endl;
        close(spiFd);
        return 1;
    }

    if (ioctl(spiFd, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0) {
        std::cerr << "Error: cannot set SPI bits per word." << std::endl;
        close(spiFd);
        return 1;
    }

    if (ioctl(spiFd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0) {
        std::cerr << "Error: cannot set SPI speed." << std::endl;
        close(spiFd);
        return 1;
    }

    std::cout << "UM7 IMU Raw Register Test" << std::endl;
    std::cout << "Device: " << SPI_DEVICE << std::endl;
    std::cout << "SPI Mode: 0" << std::endl;
    std::cout << "SPI Speed: " << SPI_SPEED_HZ << " Hz" << std::endl;
    std::cout << std::endl;

    for (int i = 0; i < 20; ++i) {
        uint32_t phiTheta = 0;
        uint32_t psi = 0;

        bool ok1 = readUM7Register32(spiFd, DREG_EULER_PHI_THETA, phiTheta);
        bool ok2 = readUM7Register32(spiFd, DREG_EULER_PSI, psi);

        if (!ok1 || !ok2) {
            std::cout << "SPI read failed." << std::endl;
            usleep(500000);
            continue;
        }

        int16_t rollRaw  = highWordSigned(phiTheta);
        int16_t pitchRaw = lowWordSigned(phiTheta);
        int16_t yawRaw   = highWordSigned(psi);

        std::cout << "Sample " << (i + 1) << std::endl;

        std::cout << "Register 0x70 raw 32-bit: 0x"
                  << std::hex << std::uppercase << std::setw(8) << std::setfill('0')
                  << phiTheta << std::dec << std::setfill(' ') << std::endl;

        std::cout << "  Roll raw  high 16 bits: " << rollRaw << std::endl;
        std::cout << "  Pitch raw low  16 bits: " << pitchRaw << std::endl;

        std::cout << "Register 0x71 raw 32-bit: 0x"
                  << std::hex << std::uppercase << std::setw(8) << std::setfill('0')
                  << psi << std::dec << std::setfill(' ') << std::endl;

        std::cout << "  Yaw raw   high 16 bits: " << yawRaw << std::endl;

        std::cout << "----------------------------------------" << std::endl;

        usleep(500000);
    }

    close(spiFd);
    return 0;
}
