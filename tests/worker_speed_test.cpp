#include "../vst3wrapper/async_bridge.h"
#include "../vst3wrapper/worker_process.h"

#include <chrono>
#include <iostream>
#include <thread>

using Clock = std::chrono::steady_clock;
using namespace std::chrono_literals;

int wmain(int argc, wchar_t** argv) {
    if (argc != 3) return 2;

    std::wstring id =
        std::to_wstring(GetCurrentProcessId());

    std::wstring pipeName =
        std::wstring(heg::kPipeName) + L"_" + id;

    heg::WorkerProcess worker;
    heg::AsyncBridge bridge;

    if (!worker.start(argv[1], id))
        return 3;

    if (!bridge.start(argv[2], 44100, 512, pipeName)) {
        worker.stop();
        return 4;
    }

    for (int i = 0; i < 100 && !bridge.ready(); ++i) {
        if (bridge.error() != heg::BridgeError::None)
            break;
        std::this_thread::sleep_for(50ms);
    }

    bool ok = bridge.ready();
    double sum = 0;
    double maximum = 0;
    int completed = 0;

    heg::AudioBlock input{};
    input.frames = 512;
    input.channels = 2;

    for (int n = 0; ok && n < 30; ++n) {
        auto start = Clock::now();

        if (!bridge.submitTagged(input, n + 1)) {
            ok = false;
            break;
        }

        bool received = false;

        for (int j = 0; j < 1000; ++j) {
            heg::AudioBlock output{};
            uint64_t sequence = 0;

            if (bridge.receiveTagged(output, sequence)) {
                received = sequence == uint64_t(n + 1);
                break;
            }

            if (bridge.error() != heg::BridgeError::None)
                break;

            std::this_thread::sleep_for(1ms);
        }

        if (!received) {
            ok = false;
            break;
        }

        double ms = std::chrono::duration<double,
            std::milli>(Clock::now() - start).count();

        sum += ms;
        if (ms > maximum) maximum = ms;
        ++completed;
    }

    std::cout << "Bloecke verarbeitet: "
              << completed << "\n";

    if (completed) {
        std::cout << "Durchschnitt: "
                  << sum / completed << " ms\n";

        std::cout << "Laengster Block: "
                  << maximum << " ms\n";
    }

    std::cout << "Bridge-Fehlercode: "
              << static_cast<int>(bridge.error()) << "\n";

    worker.stop();
    bridge.stop();

    return ok && completed == 30 ? 0 : 1;
}
