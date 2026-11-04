#include "processor.h"
#include "ids.h"
#include "pluginterfaces/vst/ivstevents.h"

#include <algorithm>
#include <windows.h>
#include <string>

namespace {
std::wstring readWindowsPath(const wchar_t* name) {
    wchar_t buffer[4096]{};
    DWORD length = GetEnvironmentVariableW(name, buffer, 4096);

    if (length == 0 || length >= 4096)
        return {};

    return std::wstring(buffer, length);
}
}

namespace HEG {

using namespace Steinberg;
using namespace Steinberg::Vst;

Processor::Processor() {
    setControllerClass(FUID::fromTUID(ControllerUID));
}

tresult PLUGIN_API Processor::initialize(FUnknown* context) {
    auto result = AudioEffect::initialize(context);

    if (result != kResultOk)
        return result;

    addAudioInput(
        STR16("Stereo Input"),
        SpeakerArr::kStereo
    );

    addAudioOutput(
        STR16("Stereo Output"),
        SpeakerArr::kStereo
    );

    // MIDI-Eingang mit 16 Kanälen.
    addEventInput(
        STR16("MIDI Input"),
        16
    );

    return kResultOk;
}

tresult PLUGIN_API Processor::canProcessSampleSize(
    int32 symbolicSampleSize
) {
    return symbolicSampleSize == kSample32
        ? kResultTrue
        : kResultFalse;
}

tresult PLUGIN_API Processor::setupProcessing(
    ProcessSetup& setup
) {
    if (setup.sampleRate <= 0.0 ||
        setup.maxSamplesPerBlock <= 0)
        return kInvalidArgument;

    auto result = AudioEffect::setupProcessing(setup);
    if (result != kResultOk)
        return result;

    sampleRate_ = setup.sampleRate;
    maxBlockSize_ = setup.maxSamplesPerBlock;

    // Noch keine Bridge im VST3-Audioweg:
    // deshalb momentan keine zusaetzliche Latenz.
    workerPath_ = readWindowsPath(L"HEG_VST_WORKER");
    pluginPath_ = readWindowsPath(L"HEG_VST_PLUGIN");

    if (workerPath_.empty() != pluginPath_.empty())
        return kInvalidArgument;

    bridgeEnabled_ = !workerPath_.empty();

    if (bridgeEnabled_ &&
        maxBlockSize_ > static_cast<int32>(heg::kMaxFrames))
        return kResultFalse;

    // Testbetrieb: ein Audioblock Verzögerung.
    // Nur bei konstanter Blockgröße korrekt.
    latencySamples_ = bridgeEnabled_
        ? static_cast<uint32>(maxBlockSize_)
        : 0;

    return kResultOk;
}

tresult PLUGIN_API Processor::setActive(TBool state) {
    if (!state) {
        bridge_.abort();
        return AudioEffect::setActive(state);
    }

    auto result = AudioEffect::setActive(state);
    if (result != kResultOk)
        return result;

    if (bridgeEnabled_) {
        if (!bridge_.start(
                workerPath_,
                pluginPath_,
                sampleRate_,
                static_cast<uint32_t>(maxBlockSize_))) {
            AudioEffect::setActive(false);
            return kResultFalse;
        }
    }

    return kResultOk;
}

tresult PLUGIN_API Processor::setProcessing(TBool /*state*/) {
    // Keine blockierenden Arbeiten im Audiothread.
    return kResultOk;
}

uint32 PLUGIN_API Processor::getLatencySamples() {
    return latencySamples_;
}

tresult PLUGIN_API Processor::process(ProcessData& data) {
    if (data.numSamples < 0)
        return kInvalidArgument;

    if (data.numSamples == 0)
        return kResultOk;

    if (data.symbolicSampleSize != kSample32)
        return kResultFalse;

    if (!data.inputs || !data.outputs ||
        data.numInputs < 1 || data.numOutputs < 1)
        return kResultFalse;

    auto& input = data.inputs[0];
    auto& output = data.outputs[0];

    if (!input.channelBuffers32 ||
        !output.channelBuffers32 ||
        input.numChannels != 2 ||
        output.numChannels != 2)
        return kResultFalse;

    if (bridgeEnabled_) {
        // MIDI-Ereignisse ohne blockierende Pipe-Aufrufe.
        if (data.inputEvents) {
            const int32 count =
                data.inputEvents->getEventCount();

            for (int32 i = 0; i < count; ++i) {
                Event event{};
                if (data.inputEvents->getEvent(i, event)
                        != kResultOk ||
                    event.busIndex != 0)
                    continue;

                const bool on =
                    event.type == Event::kNoteOnEvent;
                const bool off =
                    event.type == Event::kNoteOffEvent;

                if (!on && !off)
                    continue;

                const int channel = on
                    ? event.noteOn.channel
                    : event.noteOff.channel;
                const int pitch = on
                    ? event.noteOn.pitch
                    : event.noteOff.pitch;
                const float velocity = on
                    ? event.noteOn.velocity
                    : event.noteOff.velocity;

                if (channel < 0 || channel > 15 ||
                    pitch < 0 || pitch > 127)
                    continue;

                heg::MidiMessage midi{};
                midi.status = static_cast<uint8_t>(
                    (on ? 0x90 : 0x80) | channel);
                midi.data1 = static_cast<uint8_t>(pitch);
                midi.data2 = static_cast<uint8_t>(
                    std::clamp(
                        static_cast<int>(velocity * 127.0f + 0.5f),
                        0, 127));

                bridge_.queueMidi(midi);
            }
        }

        // Vorläufig nur konstante Blockgrößen zulassen.
        const bool supported =
            data.numSamples == maxBlockSize_ &&
            data.numSamples <= static_cast<int32>(heg::kMaxFrames);

        heg::AudioBlock inputBlock{};
        heg::AudioBlock outputBlock{};

        if (supported) {
            inputBlock.frames =
                static_cast<uint32>(data.numSamples);
            inputBlock.channels = 2;
        }

        for (int channel = 0; channel < 2; ++channel) {
            auto* src = input.channelBuffers32[channel];
            auto* dst = output.channelBuffers32[channel];

            if (!src || !dst)
                return kResultFalse;

            if (supported) {
                std::copy_n(src, data.numSamples,
                            inputBlock.data[channel]);
            }
        }

        bool received = supported &&
            bridge_.render(inputBlock, outputBlock);

        for (int channel = 0; channel < 2; ++channel) {
            float* dst = output.channelBuffers32[channel];

            if (received) {
                std::copy_n(outputBlock.data[channel],
                            data.numSamples, dst);
            } else {
                std::fill_n(dst, data.numSamples, 0.0f);
            }
        }

        output.silenceFlags = received ? 0 : 3;
        return kResultOk;
    }

    for (int channel = 0; channel < 2; ++channel) {
        const float* src = input.channelBuffers32[channel];
        float* dst = output.channelBuffers32[channel];

        if (!src || !dst)
            return kResultFalse;

        if (src != dst) {
            std::copy_n(src, data.numSamples, dst);
        }
    }

    output.silenceFlags = input.silenceFlags;

    return kResultOk;
}

}
