#include "../vst3wrapper/realtime_audio_adapter.h"
#include "../vst3wrapper/worker_process.h"

#include <chrono>
#include <iostream>
#include <thread>

using Clock = std::chrono::steady_clock;

int wmain(int argc, wchar_t** argv) {
    if (argc != 3) return 2;

    const std::wstring id =
        std::to_wstring(GetCurrentProcessId());

    const std::wstring pipeName =
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
        std::this_thread::sleep_for(
            std::chrono::milliseconds(50));
    }

    if (!bridge.ready()) {
        worker.stop();
        bridge.stop();
        return 5;
    }

    heg::AudioBlock input{};
    input.frames = 512;
    input.channels = 2;

    heg::AudioBlock output{};

    constexpr int totalBlocks = 100;
    const auto blockDuration =
        std::chrono::duration<double>(512.0 / 44100.0);

    int audioBlocks = 0;
    int silentBlocks = 0;
    int firstErrorBlock = -1;
    bool workerAliveAtError = false;
    auto testStart = Clock::now();

    auto next = Clock::now();

    for (int i = 0; i < totalBlocks; ++i) {
        if (adapter.render(bridge, input, output))
            ++audioBlocks;
        else
            ++silentBlocks;

        if (firstErrorBlock < 0 &&
            bridge.error() != heg::BridgeError::None) {
            firstErrorBlock = i;
            workerAliveAtError = worker.running();
        }

        next += std::chrono::duration_cast<
            Clock::duration>(blockDuration);

        std::this_thread::sleep_until(next);
    }

    bool workerAliveAtEnd = worker.running();

    auto elapsed = std::chrono::duration_cast<
        std::chrono::milliseconds>(
            Clock::now() - testStart).count();

    std::cout << "Erster Fehler bei Block: "
              << firstErrorBlock << "\n";
    std::cout << "Worker lebte beim Fehler: "
              << workerAliveAtError << "\n";
    std::cout << "Worker lebte am Ende: "
              << workerAliveAtEnd << "\n";
    std::cout << "Testdauer: "
              << elapsed << " ms\n";

    worker.stop();
    bridge.stop();

    std::cout << "Gesamte Bloecke: "
              << totalBlocks << "\n";

    std::cout << "Audio geliefert: "
              << audioBlocks << "\n";

    std::cout << "Stille ausgegeben: "
              << silentBlocks << "\n";

    std::cout << "Bridge-Fehlercode: "
              << static_cast<int>(bridge.error()) << "\n";

    std::cout << "TEST ABGESCHLOSSEN\n";
    return 0;
}
