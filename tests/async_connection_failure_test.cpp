#include "../vst3wrapper/async_bridge.h"

#include <chrono>
#include <iostream>
#include <thread>

int main() {
    heg::AsyncBridge bridge;

    if (!bridge.start(
        L"Z:\\HEG\\Test.dll", 44100.0, 512
    )) {
        std::cerr << "FEHLER: Thread konnte nicht starten\n";
        return 1;
    }

    for (int i = 0; i < 100; ++i) {
        if (bridge.error() != heg::BridgeError::None)
            break;

        std::this_thread::sleep_for(
            std::chrono::milliseconds(50)
        );
    }

    auto error = bridge.error();
    bridge.stop();

    if (error != heg::BridgeError::ConnectionFailed) {
        std::cerr << "FEHLER: Unerwarteter Fehlerstatus: "
                  << static_cast<int>(error) << "\n";
        return 2;
    }

    std::cout << "ERFOLG: ConnectionFailed erkannt\n";
    std::cout << "ERFOLG: Thread beendet\n";
    std::cout << "ERFOLG: VERBINDUNGSFEHLER-TEST BESTANDEN\n";

    return 0;
}
