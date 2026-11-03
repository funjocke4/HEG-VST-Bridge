#include "../vst3wrapper/async_bridge.h"

#include <chrono>
#include <iostream>
#include <thread>

int main() {
    heg::AsyncBridge bridge;

    // Diese DLL existiert absichtlich nicht.
    const std::wstring invalidPath =
        L"Z:\\HEG\\NichtVorhanden.dll";

    if (!bridge.start(invalidPath, 44100.0, 512)) {
        std::cerr << "FEHLER: Threadstart abgelehnt\n";
        return 1;
    }

    std::this_thread::sleep_for(
        std::chrono::milliseconds(300)
    );

    if (bridge.ready()) {
        std::cerr << "FEHLER: Ungueltiges Plugin akzeptiert\n";
        bridge.stop();
        return 2;
    }

    std::cout << "ERFOLG: Ungueltiges Plugin nicht geladen\n";

    bridge.stop();

    std::cout << "ERFOLG: Thread sauber beendet\n";
    return 0;
}
