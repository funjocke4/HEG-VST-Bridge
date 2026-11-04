#include <windows.h>
#include <cstdio>
#include <cstring>

#include "pluginterfaces/base/ipluginbase.h"

using namespace Steinberg;
using FactoryFn = IPluginFactory* (PLUGIN_API*)();

int wmain(int argc, wchar_t** argv) {
    if (argc != 2)
        return 2;

    HMODULE dll = LoadLibraryW(argv[1]);
    if (!dll) {
        std::printf("FEHLER: DLL laden: %lu\n",
                    GetLastError());
        return 3;
    }

    auto fn = reinterpret_cast<FactoryFn>(
        GetProcAddress(dll, "GetPluginFactory"));

    if (!fn) {
        FreeLibrary(dll);
        return 4;
    }

    IPluginFactory* factory = fn();
    if (!factory) {
        FreeLibrary(dll);
        return 5;
    }

    IPluginFactory2* factory2 = nullptr;
    bool instrumentFound = false;

    if (factory->queryInterface(
            IPluginFactory2_iid,
            reinterpret_cast<void**>(&factory2)) == kResultOk) {

        for (int32 i = 0;
             i < factory2->countClasses(); ++i) {
            PClassInfo2 info{};

            if (factory2->getClassInfo2(i, &info)
                    != kResultOk)
                continue;

            std::printf("Klasse: %s\n", info.name);
            std::printf("Kategorie: %s\n", info.category);
            std::printf("Unterkategorie: %s\n",
                        info.subCategories);

            if (std::strcmp(
                    info.name, "HEG VST Bridge") == 0 &&
                std::strcmp(
                    info.subCategories,
                    "Instrument|Synth") == 0) {
                instrumentFound = true;
            }
        }

        factory2->release();
    } else {
        std::puts("FEHLER: IPluginFactory2 fehlt");
    }

    factory->release();
    FreeLibrary(dll);

    std::puts(instrumentFound
        ? "ERFOLG: VST3-INSTRUMENT ERKANNT"
        : "FEHLER: INSTRUMENT NICHT ERKANNT");

    return instrumentFound ? 0 : 6;
}
