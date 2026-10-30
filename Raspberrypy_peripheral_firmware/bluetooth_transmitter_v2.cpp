/*
 * bluetooth_transmitter_v2.cpp
 *
 * Bluetooth transmitter for Senior Design Navigation V4.
 *
 * Function:
 *   1. Runs ./combined_navigation_test_v4.
 *   2. Reads the V4 program output.
 *   3. Prints the same output on the Raspberry Pi terminal.
 *   4. Sends the output to /dev/rfcomm0.
 *
 * Compile:
 *   g++ src/bluetooth_transmitter_v2.cpp -o bluetooth_transmitter_v2
 *
 * Run after Bluetooth RFCOMM is connected:
 *   sudo ./bluetooth_transmitter_v2
 */

#include <iostream>
#include <fstream>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <fcntl.h>

static const char *RFCOMM_DEVICE = "/dev/rfcomm0";
static const char *SOURCE_PROGRAM = "./combined_navigation_test_v4";

int main() {
    std::cout << "Bluetooth Transmitter V2\n";
    std::cout << "Source Program: " << SOURCE_PROGRAM << std::endl;
    std::cout << "Bluetooth Device: " << RFCOMM_DEVICE << std::endl;
    std::cout << "--------------------------------------------------" << std::endl;

    int btFd = open(RFCOMM_DEVICE, O_WRONLY | O_NOCTTY);

    if (btFd < 0) {
        std::cerr << "[Bluetooth Error] Cannot open " << RFCOMM_DEVICE
                  << ": " << std::strerror(errno) << std::endl;
        std::cerr << "Make sure rfcomm listen is connected before running this program." << std::endl;
        return 1;
    }

    FILE *pipe = popen(SOURCE_PROGRAM, "r");

    if (!pipe) {
        std::cerr << "[Program Error] Cannot run " << SOURCE_PROGRAM << std::endl;
        close(btFd);
        return 1;
    }

    char buffer[512];

    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        std::cout << buffer;
        std::cout.flush();

        ssize_t written = write(btFd, buffer, std::strlen(buffer));

        if (written < 0) {
            std::cerr << "[Bluetooth Error] Write failed: "
                      << std::strerror(errno) << std::endl;
            break;
        }
    }

    pclose(pipe);
    close(btFd);

    return 0;
}
