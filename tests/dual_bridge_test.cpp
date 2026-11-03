#include "../vst3wrapper/managed_async_bridge.h"
#include <chrono>
#include <cmath>
#include <iostream>
#include <thread>

using namespace std::chrono_literals;

bool waitReady(heg::ManagedAsyncBridge& bridge) {
    for (int i = 0; i < 100; ++i) {
        if (bridge.ready()) return true;
        if (bridge.error() != heg::BridgeError::None)
            return false;
        std::this_thread::sleep_for(50ms);
    }
    return false;
}

bool checkAudio(heg::ManagedAsyncBridge& bridge,
                const char* name) {
    heg::AudioBlock input{};
    input.frames = 512;
    input.channels = 2;

    if (!bridge.submit(input))
        return false;

    heg::AudioBlock output{};
    for (int i = 0; i < 100; ++i) {
        if (bridge.receive(output)) {
            if (output.frames != 512 ||
                output.channels != 2)
                return false;

            float peak = 0.0f;
            for (uint32_t c = 0; c < 2; ++c)
                for (uint32_t j = 0; j < 512; ++j)
                    peak = std::fmax(
                        peak, std::fabs(output.data[c][j]));

            std::cout << name << " Peak: "
                      << peak << "\n";
            return peak > 0.1f && peak < 0.21f;
        }

        if (bridge.error() != heg::BridgeError::None)
            return false;

        std::this_thread::sleep_for(20ms);
    }
    return false;
}

int wmain(int argc, wchar_t** argv) {
    if (argc != 3) return 2;

    heg::ManagedAsyncBridge synth1;
    heg::ManagedAsyncBridge synth2;

    if (!synth1.start(argv[1], argv[2], 44100, 512)) {
        std::cerr << "FEHLER: Synth 1 Start\n";
        return 3;
    }

    if (!synth2.start(argv[1], argv[2], 44100, 512)) {
        std::cerr << "FEHLER: Synth 2 Start\n";
        synth1.abort();
        return 4;
    }

    bool ready1 = waitReady(synth1);
    bool ready2 = waitReady(synth2);

    bool ok = ready1 && ready2;

    if (!ok) {
        std::cerr << "FEHLER: Bridge nicht bereit: "
                  << static_cast<int>(synth1.error()) << ", "
                  << static_cast<int>(synth2.error()) << "\n";
    }

    if (ok) {
        bool audio1 = checkAudio(synth1, "Synth 1");
        bool audio2 = checkAudio(synth2, "Synth 2");
        ok = audio1 && audio2;
    }

    synth1.stop();
    synth2.stop();

    if (!ok) {
        std::cerr << "FEHLER: Doppeltest\n";
        return 1;
    }

    std::cout << "ERFOLG: ZWEI BRIDGES BESTANDEN\n";
    return 0;
}
