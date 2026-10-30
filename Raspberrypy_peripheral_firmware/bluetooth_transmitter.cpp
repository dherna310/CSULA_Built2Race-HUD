#include <iostream>
#include <cstdio>
#include <cstdlib>

int main() {
    const char* bluetoothDevice = "/dev/rfcomm0";
    const char* combinedProgram = "./combined_navigation_test_v2";

    std::cout << "Bluetooth Transmitter for Combined Navigation Data" << std::endl;
    std::cout << "Bluetooth device: " << bluetoothDevice << std::endl;
    std::cout << "Source program: " << combinedProgram << std::endl;
    std::cout << "---------------------------------------------------------------" << std::endl;

    FILE* bt = fopen(bluetoothDevice, "w");
    if (bt == nullptr) {
        std::cerr << "[BLUETOOTH ERROR] Cannot open /dev/rfcomm0." << std::endl;
        std::cerr << "Please connect Bluetooth first." << std::endl;
        std::cerr << "Pi terminal 1:" << std::endl;
        std::cerr << "  sudo sdptool add SP" << std::endl;
        std::cerr << "  sudo pkill rfcomm" << std::endl;
        std::cerr << "  sudo rfcomm listen hci0 1" << std::endl;
        std::cerr << "Windows:" << std::endl;
        std::cerr << "  Open PuTTY COM5, Speed 9600, Serial" << std::endl;
        return 1;
    }

    FILE* source = popen(combinedProgram, "r");
    if (source == nullptr) {
        std::cerr << "[PROGRAM ERROR] Cannot run ./combined_navigation_test_v2." << std::endl;
        fclose(bt);
        return 1;
    }

    char line[1024];

    while (fgets(line, sizeof(line), source) != nullptr) {
        std::cout << line;
        std::cout.flush();

        fprintf(bt, "%s", line);
        fflush(bt);
    }

    pclose(source);
    fclose(bt);

    return 0;
}

