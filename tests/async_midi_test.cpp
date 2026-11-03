#include "../vst3wrapper/async_bridge.h"

#include <chrono>
#include <cmath>
#include <iostream>
#include <thread>

using namespace std::chrono_literals;

bool testMidiBlock(
    heg::AsyncBridge& bridge,
    const heg::MidiMessage& midi,
    const char* name
) {
    heg::AudioBlock input{};
    input.frames = 512;
    input.channels = 2;

    if (!bridge.submitMidi(input, midi)) {
        std::cerr << "FEHLER: " << name
                  << " konnte nicht gesendet werden\n";
        return false;
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
        std::cerr << "FEHLER: Audioantwort fehlt\n";
        return false;
    }

    float peak = 0.0f;

    for (uint32_t c = 0; c < 2; ++c) {
        for (uint32_t i = 0; i < output.frames; ++i) {
            peak = std::fmax(
                peak,
                std::fabs(output.data[c][i])
            );
        }
    }

    std::cout << name << " OK, Peak: "
              << peak << "\n";

    return peak > 0.001f;
}

int wmain(int argc, wchar_t** argv) {
    if (argc != 2)
        return 2;

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

    heg::MidiMessage noteOn{0x90, 69, 100, 0};
    heg::MidiMessage noteOff{0x80, 69, 0, 0};

    bool success =
        testMidiBlock(bridge, noteOn, "MIDI Note On");

    if (success)
        success = testMidiBlock(
            bridge, noteOff, "MIDI Note Off"
        );

    bridge.stop();

    if (!success)
        return 1;

    std::cout << "ERFOLG: ASYNC-MIDI-TEST BESTANDEN\n";

    return 0;
}
