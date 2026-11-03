#include "../vst3wrapper/async_bridge.h"

#include <chrono>
#include <iostream>
#include <thread>

int main() {
    heg::AsyncBridge bridge;

    if (!bridge.start(
        L"Z:\\HEG\\NichtVorhanden.dll",
        44100.0,
        512
    )) {
        std::cerr << "FEHLER: Threadstart\n";
        return 1;
    }

    // Maximal 5 Sekunden auf den Fehlerstatus warten.
    for (int i = 0; i < 100; ++i) {
        if (bridge.error() != heg::BridgeError::None)
            break;

        std::this_thread::sleep_for(
            std::chrono::milliseconds(50)
        );
    }

    auto error = bridge.error();

    bridge.stop();

    if (error != heg::BridgeError::LoadFailed) {
        std::cerr << "FEHLER: Falscher Fehlerstatus: "
                  << static_cast<int>(error) << "\n";
        return 2;
    }

    std::cout << "ERFOLG: LoadFailed korrekt erkannt\n";
    std::cout << "ERFOLG: Thread beendet\n";
    std::cout << "ERFOLG: FEHLERDIAGNOSE BESTANDEN\n";

    return 0;
}
