#pragma once

#include "public.sdk/source/vst/vstaudioeffect.h"

namespace HEG {

class Processor : public Steinberg::Vst::AudioEffect {
public:
    Processor();

    static Steinberg::FUnknown* createInstance(void*) {
        return static_cast<Steinberg::Vst::IAudioProcessor*>(
            new Processor()
        );
    }

    Steinberg::tresult PLUGIN_API initialize(
        Steinberg::FUnknown* context
    ) override;

    Steinberg::tresult PLUGIN_API setupProcessing(
        Steinberg::Vst::ProcessSetup& setup
    ) override;

    Steinberg::uint32 PLUGIN_API getLatencySamples()
        override;

    Steinberg::tresult PLUGIN_API process(
        Steinberg::Vst::ProcessData& data
    ) override;

    Steinberg::tresult PLUGIN_API canProcessSampleSize(
        Steinberg::int32 symbolicSampleSize
    ) override;
private:
    double sampleRate_ = 0.0;
    Steinberg::int32 maxBlockSize_ = 0;
    Steinberg::uint32 latencySamples_ = 0;
};

}
