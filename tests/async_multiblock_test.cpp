#include "../vst3wrapper/async_bridge.h"

#include <chrono>
#include <cmath>
#include <iostream>
#include <thread>

using namespace std::chrono_literals;

int wmain(int argc, wchar_t** argv) {
    if (argc != 2) return 2;

    heg::AsyncBridge bridge;

    if (!bridge.start(argv[1], 44100.0, 512))
        return 3;

    for (int i = 0; i < 100 && !bridge.ready(); ++i)
        std::this_thread::sleep_for(50ms);

    if (!bridge.ready()) {
        std::cerr << "FEHLER: Worker nicht bereit\n";
        bridge.stop();
        return 4;
    }

    heg::AudioBlock input{};
    input.frames = 512;
    input.channels = 2;

    bool success = true;

    for (int block = 1; block <= 10; ++block) {
        if (!bridge.submit(input)) {
            std::cerr << "FEHLER: Block " << block
                      << " abgelehnt\n";
            success = false;
            break;
        }

        // Zweites Submit muss bis zum Receive scheitern.
        if (bridge.submit(input)) {
            std::cerr << "FEHLER: Ueberschreibschutz\n";
            success = false;
            break;
        }

        heg::AudioBlock output{};
        bool received = false;

        for (int i = 0; i < 100; ++i) {
            if (bridge.receive(output)) {
                received = true;
                break;
            }

            std::this_thread::sleep_for(20ms);
        }

        if (!received ||
            output.frames != 512 ||
            output.channels != 2) {
            std::cerr << "FEHLER: Block " << block
                      << " nicht empfangen\n";
            success = false;
            break;
        }

        float peak = 0.0f;

        for (uint32_t c = 0; c < 2; ++c)
            for (uint32_t i = 0; i < 512; ++i)
                peak = std::fmax(
                    peak, std::fabs(output.data[c][i])
                );

        if (peak < 0.001f) {
            std::cerr << "FEHLER: Kein Audio\n";
            success = false;
            break;
        }

        std::cout << "Block " << block
                  << "/10 OK, Peak: " << peak << "\n";
    }

    bridge.stop();

    if (!success) return 1;

    std::cout << "ERFOLG: 10 AUDIOBLOCKE BESTANDEN\n";
    return 0;
}
