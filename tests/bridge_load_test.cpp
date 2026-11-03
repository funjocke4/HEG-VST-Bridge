#include "../vst3wrapper/bridge_client.h"
#include <iostream>
#include <string>

int wmain(int argc, wchar_t** argv) {
    if (argc != 2) {
        std::wcerr << L"Aufruf: HEGBridgeLoadTest64.exe <VST2-DLL>\n";
        return 2;
    }

    heg::BridgeClient bridge;

    if (!bridge.connect()) {
        std::cerr << "FEHLER: Worker nicht erreichbar\n";
        return 1;
    }

    std::wcout << L"Lade VST2: " << argv[1] << L"\n";

    if (!bridge.loadPlugin(argv[1])) {
        std::cerr << "FEHLER: VST2 konnte nicht geladen werden\n";
        return 3;
    }

    std::cout << "ERFOLG: VST2-DLL geladen\n";

    if (!bridge.configure(44100.0, 512)) {
        std::cerr << "FEHLER: Konfiguration fehlgeschlagen\n";
        return 4;
    }

    std::cout << "ERFOLG: 44100 Hz / 512 Samples\n";

    bridge.unloadPlugin();
    return 0;
}
