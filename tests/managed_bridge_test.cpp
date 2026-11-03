#include "../vst3wrapper/managed_async_bridge.h"

#include <chrono>
#include <cmath>
#include <iostream>
#include <thread>

using namespace std::chrono_literals;

int wmain(int argc, wchar_t** argv) {
    if (argc != 3) {
        std::cerr << "Worker und Plugin angeben\n";
        return 2;
    }

    heg::ManagedAsyncBridge bridge;

    if (!bridge.start(argv[1], argv[2], 44100.0, 512)) {
        std::cerr << "FEHLER: Automatischer Start\n";
        return 3;
    }

    for (int i = 0; i < 100; ++i) {
        if (bridge.ready() ||
            bridge.error() != heg::BridgeError::None)
            break;

        std::this_thread::sleep_for(50ms);
    }

    if (!bridge.ready()) {
        std::cerr << "FEHLER: Bridge nicht bereit. Code: "
                  << static_cast<int>(bridge.error())
                  << "\n";
        bridge.stop();
        return 4;
    }

    std::cout << "ERFOLG: Worker und Bridge bereit\n";

    heg::AudioBlock input{};
    input.frames = 512;
    input.channels = 2;

    if (!bridge.submit(input)) {
        std::cerr << "FEHLER: Audio senden\n";
        bridge.stop();
        return 5;
    }

    heg::AudioBlock output{};
    bool received = false;

    for (int i = 0; i < 100; ++i) {
        if (bridge.receive(output)) {
            received = true;
            break;
        }

        if (bridge.error() != heg::BridgeError::None)
            break;

        std::this_thread::sleep_for(20ms);
    }

    float peak = 0.0f;

    if (received &&
        output.frames == 512 &&
        output.channels == 2) {

        for (uint32_t c = 0; c < 2; ++c)
            for (uint32_t i = 0; i < 512; ++i)
                peak = std::fmax(
                    peak, std::fabs(output.data[c][i])
                );
    }

    std::cout << "Audio-Peak: " << peak << "\n";

    bridge.stop();

    if (!received || peak < 0.1f || peak > 0.21f) {
        std::cerr << "FEHLER: Audio-Test\n";
        return 1;
    }

    std::cout << "ERFOLG: MANAGED BRIDGE BESTANDEN\n";
    return 0;
}
