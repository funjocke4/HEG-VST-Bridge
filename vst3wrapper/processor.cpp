#include "processor.h"
#include "ids.h"

#include <algorithm>

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

    return kResultOk;
}

tresult PLUGIN_API Processor::canProcessSampleSize(
    int32 symbolicSampleSize
) {
    return symbolicSampleSize == kSample32
        ? kResultTrue
        : kResultFalse;
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
