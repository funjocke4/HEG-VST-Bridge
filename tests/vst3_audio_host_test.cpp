#include <windows.h>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <thread>

#include "pluginterfaces/base/ipluginbase.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivsthostapplication.h"

using namespace Steinberg;
using namespace Steinberg::Vst;

class TestHost final : public IHostApplication {
public:
    tresult PLUGIN_API queryInterface(
        const TUID uid, void** obj) override {
        if (!obj) return kInvalidArgument;
        *obj = nullptr;

        if (std::memcmp(uid, IHostApplication_iid, 16) == 0 ||
            std::memcmp(uid, FUnknown_iid, 16) == 0) {
            *obj = static_cast<IHostApplication*>(this);
            addRef();
            return kResultOk;
        }
        return kNoInterface;
    }

    uint32 PLUGIN_API addRef() override { return ++refs_; }
    uint32 PLUGIN_API release() override { return --refs_; }

    tresult PLUGIN_API getName(String128 name) override {
        name[0] = 'H';
        name[1] = 'E';
        name[2] = 'G';
        name[3] = 0;
        return kResultOk;
    }

    tresult PLUGIN_API createInstance(
        TUID, TUID, void** obj) override {
        if (obj) *obj = nullptr;
        return kNoInterface;
    }

private:
    std::atomic<uint32> refs_{1};
};

using FactoryFn = IPluginFactory* (PLUGIN_API*)();

int wmain(int argc, wchar_t** argv) {
    if (argc != 4) {
        std::puts("FEHLER: VST3 Worker DLL erwartet");
        return 2;
    }

    if (!SetEnvironmentVariableW(L"HEG_VST_WORKER", argv[2]) ||
        !SetEnvironmentVariableW(L"HEG_VST_PLUGIN", argv[3]))
        return 3;

    HMODULE module = LoadLibraryW(argv[1]);
    if (!module) {
        std::printf("FEHLER: VST3 laden: %lu\n",
                    GetLastError());
        return 4;
    }

    auto getFactory = reinterpret_cast<FactoryFn>(
        GetProcAddress(module, "GetPluginFactory"));

    if (!getFactory) {
        FreeLibrary(module);
        return 5;
    }

    IPluginFactory* factory = getFactory();
    if (!factory) {
        FreeLibrary(module);
        return 6;
    }

    IComponent* component = nullptr;
    IAudioProcessor* processor = nullptr;
    TestHost host;
    bool initialized = false;
    bool active = false;
    bool processing = false;
    int result = 1;

    do {
        PClassInfo info{};

        if (factory->getClassInfo(0, &info) != kResultOk)
            break;

        if (factory->createInstance(
                info.cid, IComponent_iid,
                reinterpret_cast<void**>(&component)) != kResultOk)
            break;

        std::puts("ERFOLG: Prozessorinstanz erstellt");

        {
            auto rc = component->initialize(&host);
            std::printf("DIAG Initialisierung: %d\n",
                        static_cast<int>(rc));
            if (rc != kResultOk)
                break;
        }
        initialized = true;

        if (component->queryInterface(
                IAudioProcessor_iid,
                reinterpret_cast<void**>(&processor)) != kResultOk)
            break;

        ProcessSetup setup{};
        setup.processMode = kRealtime;
        setup.symbolicSampleSize = kSample32;
        setup.maxSamplesPerBlock = 512;
        setup.sampleRate = 44100.0;

        {
            auto rc = processor->setupProcessing(setup);
            std::printf("DIAG Audio-Setup: %d\n",
                        static_cast<int>(rc));
            if (rc != kResultOk)
                break;
        }

        std::printf("Gemeldete Latenz: %u Samples\n",
                    processor->getLatencySamples());

        {
            auto rc = component->setActive(true);
            std::printf("DIAG Worker-Aktivierung: %d\n",
                        static_cast<int>(rc));
            if (rc != kResultOk)
                break;
        }
        active = true;

        {
            auto rc = processor->setProcessing(true);
            std::printf("DIAG Audioverarbeitung starten: %d\n",
                        static_cast<int>(rc));
            if (rc != kResultOk)
                break;
        }
        processing = true;

        std::this_thread::sleep_for(
            std::chrono::milliseconds(600));

        float inL[512]{}, inR[512]{};
        float outL[512]{}, outR[512]{};

        float* inputs[2] = {inL, inR};
        float* outputs[2] = {outL, outR};

        AudioBusBuffers inputBus{};
        inputBus.numChannels = 2;
        inputBus.channelBuffers32 = inputs;

        AudioBusBuffers outputBus{};
        outputBus.numChannels = 2;
        outputBus.channelBuffers32 = outputs;

        ProcessData data{};
        data.processMode = kRealtime;
        data.symbolicSampleSize = kSample32;
        data.numSamples = 512;
        data.numInputs = 1;
        data.numOutputs = 1;
        data.inputs = &inputBus;
        data.outputs = &outputBus;

        int audio = 0;
        int silent = 0;
        float maxPeak = 0.0f;
        bool processError = false;

        using Clock = std::chrono::steady_clock;
        auto next = Clock::now();
        const auto interval = std::chrono::duration_cast<
            Clock::duration>(
                std::chrono::duration<double>(512.0 / 44100.0));

        for (int block = 0; block < 100; ++block) {
            if (processor->process(data) != kResultOk) {
                processError = true;
                break;
            }

            float peak = 0.0f;

            for (int i = 0; i < 512; ++i) {
                peak = std::fmax(peak, std::fabs(outL[i]));
                peak = std::fmax(peak, std::fabs(outR[i]));
            }

            maxPeak = std::fmax(maxPeak, peak);

            if (peak > 0.05f)
                ++audio;
            else
                ++silent;

            next += interval;
            std::this_thread::sleep_until(next);
        }

        std::printf("Audio geliefert: %d\n", audio);
        std::printf("Stille ausgegeben: %d\n", silent);
        std::printf("Maximaler Audio-Peak: %.4f\n", maxPeak);

        result = (!processError &&
                  audio > 0 &&
                  maxPeak > 0.1f &&
                  maxPeak < 0.21f) ? 0 : 7;

    } while (false);

    if (processing) processor->setProcessing(false);
    if (active) component->setActive(false);
    if (processor) processor->release();
    if (initialized) component->terminate();
    if (component) component->release();

    factory->release();
    FreeLibrary(module);

    std::puts(result == 0
        ? "ERFOLG: VST3-WORKER-AUDIOTEST"
        : "FEHLER: VST3-WORKER-AUDIOTEST");

    return result;
}
