#include "../vst3wrapper/realtime_audio_adapter.h"
#include "../vst3wrapper/worker_process.h"

#include <chrono>
#include <cmath>
#include <iostream>
#include <thread>

using namespace std::chrono_literals;

int wmain(int argc, wchar_t** argv) {
    if (argc != 3) return 2;

    const std::wstring id =
        std::to_wstring(GetCurrentProcessId());

    const std::wstring pipeName =
        std::wstring(heg::kPipeName) + L"_" + id;

    heg::WorkerProcess worker;
    heg::AsyncBridge bridge;
    heg::RealtimeAudioAdapter adapter;

    if (!worker.start(argv[1], id)) {
        std::cerr << "FEHLER: Worker-Start\n";
        return 3;
    }

    if (!bridge.start(argv[2], 44100.0, 512, pipeName)) {
        std::cerr << "FEHLER: Bridge-Start\n";
        worker.stop();
        return 4;
    }

    for (int i = 0; i < 100 && !bridge.ready(); ++i) {
        if (bridge.error() != heg::BridgeError::None)
            break;
        std::this_thread::sleep_for(50ms);
    }

    bool ok = bridge.ready();

    if (ok) {
        heg::AudioBlock input{};
        input.frames = 512;
        input.channels = 2;

        heg::AudioBlock output{};

        // Der erste Block darf noch kein Ergebnis haben.
        bool first = adapter.render(bridge, input, output);

        std::cout << "Erster Block bereits fertig: "
                  << first << "\n";

        float firstPeak = 0.0f;
        for (int c = 0; c < 2; ++c)
            for (int j = 0; j < 512; ++j)
                firstPeak = std::fmax(
                    firstPeak, std::fabs(output.data[c][j]));

        std::cout << "Erster Block Peak: "
                  << firstPeak << "\n";

        ok = !first && firstPeak == 0.0f;

        bool received = false;
        float peak = 0.0f;

        for (int i = 0; ok && i < 100; ++i) {
            std::this_thread::sleep_for(20ms);

            if (adapter.render(bridge, input, output)) {
                received = true;

                for (int c = 0; c < 2; ++c)
                    for (int j = 0; j < 512; ++j)
                        peak = std::fmax(
                            peak,
                            std::fabs(output.data[c][j]));

                break;
            }

            if (bridge.error() != heg::BridgeError::None)
                break;
        }

        std::cout << "Audio empfangen: "
                  << received << "\n";
        std::cout << "Audio-Peak: "
                  << peak << "\n";

        ok = ok && received &&
             peak > 0.1f && peak < 0.21f;
    }

    if (!ok) {
        std::cerr << "Bridge-Fehlercode: "
                  << static_cast<int>(bridge.error()) << "\n";
    }

    // Test-Aufraeumung ausserhalb des Audiothreads.
    worker.stop();
    bridge.stop();

    if (!ok) {
        std::cerr << "FEHLER: AUDIO-ADAPTER-TEST\n";
        return 1;
    }

    std::cout << "ERFOLG: AUDIO-ADAPTER BESTANDEN\n";
    return 0;
}
