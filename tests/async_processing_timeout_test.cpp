#include "../vst3wrapper/async_bridge.h"
#include <chrono>
#include <iostream>
#include <thread>

using Clock = std::chrono::steady_clock;

int main() {
    heg::AsyncBridge bridge;

    if (!bridge.start(L"Z:\\HEG\\Dummy.dll", 44100.0, 512))
        return 1;

    for (int i = 0; i < 100 && !bridge.ready() &&
         bridge.error() == heg::BridgeError::None; ++i)
        std::this_thread::sleep_for(
            std::chrono::milliseconds(50));

    if (!bridge.ready()) {
        std::cerr << "FEHLER: Worker nicht bereit\n";
        bridge.stop();
        return 2;
    }

    heg::AudioBlock input{};
    input.frames = 512;
    input.channels = 2;

    if (!bridge.submit(input)) {
        bridge.stop();
        return 3;
    }

    auto start = Clock::now();

    for (int i = 0; i < 140; ++i) {
        if (bridge.error() != heg::BridgeError::None)
            break;
        std::this_thread::sleep_for(
            std::chrono::milliseconds(50));
    }

    auto elapsed = std::chrono::duration_cast<
        std::chrono::milliseconds>(
            Clock::now() - start).count();

    bool errorOK =
        bridge.error() == heg::BridgeError::ProcessingFailed;

    auto stopStart = Clock::now();
    bridge.stop();

    auto stopMs = std::chrono::duration_cast<
        std::chrono::milliseconds>(
            Clock::now() - stopStart).count();

    std::cout << "Fehler nach: " << elapsed << " ms\n";
    std::cout << "stop() Dauer: " << stopMs << " ms\n";

    if (!errorOK || elapsed < 2000 ||
        elapsed > 6500 || stopMs > 2000) {
        std::cerr << "FEHLER: Timeout-Test gescheitert\n";
        return 4;
    }

    std::cout << "ERFOLG: ProcessingFailed erkannt\n";
    std::cout << "ERFOLG: stop() zurueckgekehrt\n";
    return 0;
}
