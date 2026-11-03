#include "../vst3wrapper/async_bridge.h"
#include "../vst3wrapper/worker_process.h"

#include <chrono>
#include <cmath>
#include <iostream>
#include <thread>

using namespace std::chrono_literals;

bool testBlock(heg::AsyncBridge& bridge,
               uint64_t expectedId) {
    heg::AudioBlock input{};
    input.frames = 512;
    input.channels = 2;

    if (!bridge.submitTagged(input, expectedId)) {
        std::cerr << "FEHLER: Block senden\n";
        return false;
    }

    heg::AudioBlock output{};
    uint64_t receivedId = 0;

    for (int i = 0; i < 100; ++i) {
        if (bridge.receiveTagged(output, receivedId)) {
            float peak = 0.0f;

            for (int c = 0; c < 2; ++c)
                for (int j = 0; j < 512; ++j)
                    peak = std::fmax(
                        peak, std::fabs(output.data[c][j]));

            std::cout << "Gesendet: " << expectedId
                      << " Empfangen: " << receivedId
                      << " Peak: " << peak << "\n";

            return receivedId == expectedId &&
                   output.frames == 512 &&
                   output.channels == 2 &&
                   peak > 0.1f && peak < 0.21f;
        }

        if (bridge.error() != heg::BridgeError::None)
            return false;

        std::this_thread::sleep_for(20ms);
    }

    return false;
}

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

    if (ok)
        ok = testBlock(bridge, 101);

    if (ok)
        ok = testBlock(bridge, 102);

    worker.stop();
    bridge.stop();

    if (!ok) {
        std::cerr << "FEHLER: BLOCKNUMMERN-TEST\n";
        return 1;
    }

    std::cout << "ERFOLG: BLOCKNUMMERN BESTANDEN\n";
    return 0;
}
