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

    Steinberg::tresult PLUGIN_API process(
        Steinberg::Vst::ProcessData& data
    ) override;

    Steinberg::tresult PLUGIN_API canProcessSampleSize(
        Steinberg::int32 symbolicSampleSize
    ) override;
};

}
