#include <windows.h>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <thread>

#include "pluginterfaces/base/ipluginbase.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivstevents.h"
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


class TestMidiEvents final : public IEventList {
public:
    tresult PLUGIN_API queryInterface(
        const TUID uid, void** obj) override {
        if (!obj) return kInvalidArgument;
        *obj = nullptr;
        if (std::memcmp(uid, IEventList_iid, 16) == 0 ||
            std::memcmp(uid, FUnknown_iid, 16) == 0) {
            *obj = static_cast<IEventList*>(this);
            addRef();
            return kResultOk;
        }
        return kNoInterface;
    }

    uint32 PLUGIN_API addRef() override {
        return ++refs_;
    }

    uint32 PLUGIN_API release() override {
        return --refs_;
    }

    int32 PLUGIN_API getEventCount() override {
        return hasEvent_ ? 1 : 0;
    }

    tresult PLUGIN_API getEvent(
        int32 index, Event& event) override {
        if (index != 0 || !hasEvent_)
            return kInvalidArgument;
        event = current_;
        return kResultOk;
    }

    tresult PLUGIN_API addEvent(Event& event) override {
        current_ = event;
        hasEvent_ = true;
        return kResultOk;
    }

    void clear() {
        hasEvent_ = false;
    }

    void setNote(bool on) {
        current_ = {};
        current_.busIndex = 0;
        current_.sampleOffset = 0;
        current_.type = on
            ? Event::kNoteOnEvent
            : Event::kNoteOffEvent;

        if (on) {
            current_.noteOn.channel = 0;
            current_.noteOn.pitch = 69;
            current_.noteOn.velocity = 0.8f;
            current_.noteOn.noteId = -1;
        } else {
            current_.noteOff.channel = 0;
            current_.noteOff.pitch = 69;
            current_.noteOff.velocity = 0.0f;
            current_.noteOff.noteId = -1;
        }
        hasEvent_ = true;
    }

private:
    std::atomic<uint32> refs_{1};
    Event current_{};
    bool hasEvent_ = false;
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
    TestMidiEvents midiEvents;
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

        auto midiBuses = component->getBusCount(
            kEvent, kInput);

        std::printf("MIDI-Eingaenge: %d\n",
                    static_cast<int>(midiBuses));

        if (midiBuses != 1)
            break;

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
        data.inputEvents = &midiEvents;


        float beforePeak = 0.0f;
        float playingPeak = 0.0f;
        float afterPeak = 0.0f;
        int playingBlocks = 0;
        bool processError = false;

        using Clock = std::chrono::steady_clock;
        auto next = Clock::now();
        const auto interval = std::chrono::duration_cast<
            Clock::duration>(
                std::chrono::duration<double>(
                    512.0 / 44100.0));

        for (int block = 0; block < 140; ++block) {
            midiEvents.clear();

            if (block == 20)
                midiEvents.setNote(true);

            if (block == 90)
                midiEvents.setNote(false);

            if (processor->process(data) != kResultOk) {
                processError = true;
                break;
            }

            float peak = 0.0f;

            for (int i = 0; i < 512; ++i) {
                peak = std::fmax(peak, std::fabs(outL[i]));
                peak = std::fmax(peak, std::fabs(outR[i]));
            }

            if (block < 20)
                beforePeak = std::fmax(beforePeak, peak);

            if (block >= 30 && block < 90) {
                playingPeak = std::fmax(playingPeak, peak);
                if (peak > 0.05f)
                    ++playingBlocks;
            }

            if (block >= 105)
                afterPeak = std::fmax(afterPeak, peak);

            next += interval;
            std::this_thread::sleep_until(next);
        }

        std::printf("Vor Note-On Peak: %.4f\n", beforePeak);
        std::printf("Bei Note-On Peak: %.4f\n", playingPeak);
        std::printf("Nach Note-Off Peak: %.4f\n", afterPeak);
        std::printf("Aktive Audiobloecke: %d\n",
                    playingBlocks);

        result = (!processError &&
                  beforePeak < 0.001f &&
                  playingPeak > 0.1f &&
                  playingBlocks > 20 &&
                  afterPeak < 0.001f) ? 0 : 7;

    } while (false);

    if (processing) processor->setProcessing(false);
    if (active) component->setActive(false);
    if (processor) processor->release();
    if (initialized) component->terminate();
    if (component) component->release();

    factory->release();
    FreeLibrary(module);

    std::puts(result == 0
        ? "ERFOLG: VST3-MIDI-AUDIOTEST"
        : "FEHLER: VST3-MIDI-AUDIOTEST");

    return result;
}
