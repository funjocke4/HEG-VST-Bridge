#include "../vst3wrapper/worker_process.h"
#include "../vst3wrapper/async_bridge.h"

#include <chrono>
#include <cmath>
#include <iostream>
#include <thread>

using namespace std::chrono_literals;

int wmain(int argc, wchar_t** argv) {
    if (argc != 3) {
        std::cerr << "Worker-EXE und VST2-DLL angeben\n";
        return 2;
    }

    heg::WorkerProcess worker;

    if (!worker.start(argv[1])) {
        std::cerr << "FEHLER: Worker-Start\n";
        return 3;
    }

    heg::AsyncBridge bridge;

    if (!bridge.start(argv[2], 44100.0, 512)) {
        std::cerr << "FEHLER: Bridge-Start\n";
        worker.stop();
        return 4;
    }

    for (int i = 0; i < 100; ++i) {
        if (bridge.ready() ||
            bridge.error() != heg::BridgeError::None)
            break;

        std::this_thread::sleep_for(50ms);
    }

    bool success = bridge.ready();

    if (!success) {
        std::cerr << "FEHLER: Bridge nicht bereit, Code "
                  << static_cast<int>(bridge.error())
                  << "\n";
    }

    if (success) {
        heg::AudioBlock input{};
        input.frames = 512;
        input.channels = 2;

        success = bridge.submit(input);

        if (!success)
            std::cerr << "FEHLER: Audio nicht gesendet\n";

        heg::AudioBlock output{};
        bool received = false;

        if (success) {
            for (int i = 0; i < 100; ++i) {
                if (bridge.receive(output)) {
                    received = true;
                    break;
                }
                if (bridge.error() != heg::BridgeError::None)
                    break;
                std::this_thread::sleep_for(20ms);
            }
        }

        success = success && received &&
                  output.frames == 512 &&
                  output.channels == 2;

        if (success) {
            float peak = 0.0f;

            for (uint32_t channel = 0; channel < 2; ++channel)
                for (uint32_t i = 0; i < 512; ++i)
                    peak = std::fmax(
                        peak,
                        std::fabs(output.data[channel][i])
                    );

            std::cout << "Audio-Peak: " << peak << "\n";
            success = peak > 0.1f && peak < 0.21f;
        }
    }

    // Reihenfolge ist wichtig:
    // erst Kommunikation beenden, dann Worker.
    bridge.stop();
    worker.stop();

    if (!success) {
        std::cerr << "FEHLER: Integrationstest\n";
        return 1;
    }

    std::cout << "ERFOLG: WORKER + AUDIO BESTANDEN\n";
    return 0;
}
