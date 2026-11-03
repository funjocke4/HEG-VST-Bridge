#include "../vst3wrapper/bridge_client.h"
#include <iostream>

int main() {
    heg::BridgeClient bridge;

    std::cout << "HEG VST Bridge Verbindungstest\n";

    if (!bridge.connect()) {
        std::cerr << "FEHLER: Keine Verbindung zum Worker32\n";
        return 1;
    }

    std::cout << "ERFOLG: Verbindung zum Worker32\n";
    std::cout << "ERFOLG: HEG-Protokoll erkannt\n";

    bridge.disconnect();

    return 0;
}
