#include "../vst3wrapper/bridge_client.h"
#include <chrono>
#include <iostream>

int main() {
    heg::BridgeClient client;

    const auto start = std::chrono::steady_clock::now();
    bool connected = client.connect();
    const auto end = std::chrono::steady_clock::now();

    auto elapsed = std::chrono::duration_cast<
        std::chrono::milliseconds
    >(end - start).count();

    std::cout << "Wartezeit: " << elapsed << " ms\n";

    if (connected) {
        std::cerr << "FEHLER: Stummer Worker akzeptiert\n";
        return 1;
    }

    if (elapsed < 2000 || elapsed > 6500) {
        std::cerr << "FEHLER: Timeout nicht wie erwartet\n";
        return 2;
    }

    std::cout << "ERFOLG: Pipe-Timeout erkannt\n";
    return 0;
}
