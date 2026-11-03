#include "../vst3wrapper/realtime_audio_adapter.h"
#include "../vst3wrapper/worker_process.h"

#include <chrono>
#include <cmath>
#include <iostream>
#include <thread>

using namespace std::chrono_literals;

float peakOf(const heg::AudioBlock& block) {
    float peak = 0.0f;
    for (int c = 0; c < 2; ++c)
        for (uint32_t i = 0; i < block.frames; ++i)
            peak = std::fmax(
                peak, std::fabs(block.data[c][i]));
    return peak;
}

int wmain(int argc, wchar_t** argv) {
    if (argc != 3) return 2;

    std::wstring id = std::to_wstring(
        GetCurrentProcessId());

    std::wstring pipeName =
        std::wstring(heg::kPipeName) + L"_" + id;

    heg::WorkerProcess worker;
    heg::AsyncBridge bridge;
    heg::RealtimeAudioAdapter adapter;

    if (!worker.start(argv[1], id))
        return 3;

    if (!bridge.start(argv[2], 44100.0, 512, pipeName)) {
        worker.stop();
        return 4;
    }

    for (int i = 0; i < 100 && !bridge.ready(); ++i) {
        if (bridge.error() != heg::BridgeError::None)
            break;
        std::this_thread::sleep_for(50ms);
    }

    bool ok = bridge.ready();

    heg::AudioBlock input{};
    input.frames = 512;
    input.channels = 2;

    heg::AudioBlock output{};

    if (ok) {
        // Absichtlich falsche Blocknummer 999.
        ok = bridge.submitTagged(input, 999);
    }

    if (ok) {
        std::this_thread::sleep_for(300ms);

        // Adapter erwartet noch keinen fertigen Block.
        // Block 999 muss verworfen werden.
        bool accepted = adapter.render(
            bridge, input, output);

        float peak = peakOf(output);

        std::cout << "Alter Block akzeptiert: "
                  << accepted << "\n";
        std::cout << "Ausgang bei altem Block: "
                  << peak << "\n";

        ok = !accepted && peak == 0.0f;
    }

    if (ok) {
        // Der Adapter hat inzwischen Block 1 gesendet.
        std::this_thread::sleep_for(300ms);

        bool accepted = adapter.render(
            bridge, input, output);

        float peak = peakOf(output);

        std::cout << "Richtiger Block akzeptiert: "
                  << accepted << "\n";
        std::cout << "Audio-Peak: "
                  << peak << "\n";

        ok = accepted &&
             peak > 0.1f && peak < 0.21f;
    }

    // Worker zuerst beenden, dann Kommunikation.
    worker.stop();
    bridge.stop();

    if (!ok) {
        std::cerr << "FEHLER: VERALTETE BLOECKE\n";
        return 1;
    }

    std::cout << "ERFOLG: BLOCKPRUEFUNG BESTANDEN\n";
    return 0;
}
