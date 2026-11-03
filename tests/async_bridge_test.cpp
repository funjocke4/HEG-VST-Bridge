#include "../vst3wrapper/async_bridge.h"

#include <iostream>
#include <chrono>
#include <thread>
#include <cmath>

using namespace std::chrono_literals;

int wmain(int argc, wchar_t** argv) {
    if (argc != 2) {
        std::cerr << "VST2-DLL erforderlich\n";
        return 2;
    }

    heg::AsyncBridge bridge;

    std::cout << "=== HEG AsyncBridge Test ===\n";

    if (!bridge.start(argv[1], 44100.0, 512)) {
        std::cerr << "FEHLER: Threadstart\n";
        return 1;
    }

    // Maximal 5 Sekunden auf Initialisierung warten.
    for (int i = 0; i < 100 && !bridge.ready(); ++i) {
        std::this_thread::sleep_for(50ms);
    }

    if (!bridge.ready()) {
        std::cerr << "FEHLER: Worker nicht bereit\n";
        bridge.stop();
        return 3;
    }

    std::cout << "ERFOLG: Hintergrundthread bereit\n";

    heg::AudioBlock input{};
    input.frames = 512;
    input.channels = 2;

    if (!bridge.submit(input)) {
        std::cerr << "FEHLER: Audioblock abgelehnt\n";
        bridge.stop();
        return 4;
    }

    std::cout << "ERFOLG: Audioblock uebergeben\n";

    heg::AudioBlock output{};
    bool received = false;

    for (int i = 0; i < 100; ++i) {
        if (bridge.receive(output)) {
            received = true;
            break;
        }

        std::this_thread::sleep_for(20ms);
    }

    if (!received) {
        std::cerr << "FEHLER: Kein Audioblock empfangen\n";
        bridge.stop();
        return 5;
    }

    float peak = 0.0f;

    for (uint32_t c = 0; c < output.channels; ++c) {
        for (uint32_t i = 0; i < output.frames; ++i) {
            peak = std::fmax(
                peak, std::fabs(output.data[c][i])
            );
        }
    }

    std::cout << "ERFOLG: Audio asynchron empfangen\n";
    std::cout << "Frames: " << output.frames << "\n";
    std::cout << "Kanaele: " << output.channels << "\n";
    std::cout << "Peak: " << peak << "\n";

    bridge.stop();

    if (output.frames != 512 ||
        output.channels != 2 ||
        peak < 0.001f) {
        std::cerr << "FEHLER: Ungueltiges Audioergebnis\n";
        return 6;
    }

    std::cout << "ERFOLG: ASYNC-TEST BESTANDEN\n";
    return 0;
}
