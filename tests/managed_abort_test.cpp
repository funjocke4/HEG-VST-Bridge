#include "../vst3wrapper/managed_async_bridge.h"

#include <chrono>
#include <iostream>
#include <thread>

using namespace std::chrono_literals;
using Clock = std::chrono::steady_clock;

int wmain(int argc, wchar_t** argv) {
    if (argc != 2)
        return 2;

    heg::ManagedAsyncBridge bridge;

    if (!bridge.start(
            argv[1],
            L"Z:\\HEG\\Dummy.dll",
            44100.0,
            512)) {
        std::cerr << "FEHLER: Worker-Start\n";
        return 3;
    }

    for (int i = 0; i < 100; ++i) {
        if (bridge.ready() ||
            bridge.error() != heg::BridgeError::None)
            break;

        std::this_thread::sleep_for(50ms);
    }

    if (!bridge.ready()) {
        std::cerr << "FEHLER: Bridge nicht bereit\n";
        bridge.abort();
        return 4;
    }

    heg::AudioBlock input{};
    input.frames = 512;
    input.channels = 2;

    if (!bridge.submit(input)) {
        std::cerr << "FEHLER: Audioblock abgelehnt\n";
        bridge.abort();
        return 5;
    }

    // Dem Test-Worker Zeit geben, den Block zu empfangen.
    std::this_thread::sleep_for(500ms);

    auto start = Clock::now();

    bridge.abort();

    auto elapsed = std::chrono::duration_cast<
        std::chrono::milliseconds>(
            Clock::now() - start).count();

    std::cout << "abort() Dauer: "
              << elapsed << " ms\n";

    if (bridge.ready()) {
        std::cerr << "FEHLER: Bridge noch bereit\n";
        return 6;
    }

    if (elapsed > 2000) {
        std::cerr << "FEHLER: Notabschaltung zu langsam\n";
        return 7;
    }

    std::cout << "ERFOLG: Worker-Notabschaltung\n";
    return 0;
}
