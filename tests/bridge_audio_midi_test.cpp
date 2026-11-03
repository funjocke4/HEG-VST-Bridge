#include "../vst3wrapper/bridge_client.h"
#include <iostream>
#include <algorithm>
#include <cmath>

int wmain(int argc, wchar_t** argv) {
    if (argc != 2) {
        std::cerr << "VST2-DLL als Argument erforderlich\n";
        return 2;
    }

    heg::BridgeClient bridge;

    if (!bridge.connect()) {
        std::cerr << "FEHLER: Worker nicht erreichbar\n";
        return 1;
    }

    if (!bridge.loadPlugin(argv[1])) {
        std::cerr << "FEHLER: VST2 konnte nicht geladen werden\n";
        return 3;
    }

    if (!bridge.configure(44100.0, 512)) {
        std::cerr << "FEHLER: Konfiguration fehlgeschlagen\n";
        return 4;
    }

    std::cout << "ERFOLG: VST2 geladen und konfiguriert\n";

    // MIDI: Note On, Kanal 1, Note 69 (A4), Anschlag 100
    if (!bridge.midi(0x90, 69, 100)) {
        std::cerr << "FEHLER: MIDI Note On\n";
        return 5;
    }

    std::cout << "ERFOLG: MIDI Note On uebertragen\n";

    heg::AudioBlock input{};
    heg::AudioBlock output{};

    input.frames = 512;
    input.channels = 2;

    if (!bridge.process(input, output)) {
        std::cerr << "FEHLER: Audioverarbeitung\n";
        return 6;
    }

    float peak = 0.0f;

    for (uint32_t c = 0; c < 2; ++c) {
        for (uint32_t i = 0; i < output.frames; ++i) {
            peak = std::max(
                peak, std::abs(output.data[c][i])
            );
        }
    }

    std::cout << "ERFOLG: Audioblock empfangen\n";
    std::cout << "Frames: " << output.frames << "\n";
    std::cout << "Kanaele: " << output.channels << "\n";
    std::cout << "Audio-Pegel (Peak): " << peak << "\n";

    if (peak < 0.001f) {
        std::cerr << "FEHLER: Kein Testton erkannt\n";
        return 7;
    }

    if (!bridge.midi(0x80, 69, 0)) {
        std::cerr << "FEHLER: MIDI Note Off\n";
        return 8;
    }

    std::cout << "ERFOLG: MIDI Note Off uebertragen\n";

    if (!bridge.unloadPlugin()) {
        std::cerr << "FEHLER: Entladen fehlgeschlagen\n";
        return 9;
    }

    std::cout << "ERFOLG: AUDIO- UND MIDI-TEST BESTANDEN\n";

    return 0;
}
