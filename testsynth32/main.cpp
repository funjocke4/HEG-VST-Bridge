#include <cmath>
#include <cstdint>
#include "../worker32/vst2_abi.h"

namespace {
AEffect synth{};
double phase = 0.0;
double sampleRate = 44100.0;
bool active = false;

VstIntPtr __cdecl dispatch(
    AEffect*, int32_t opcode, int32_t,
    VstIntPtr value, void*, float opt)
{
    switch (opcode) {
        case effOpen:
            phase = 0.0;
            return 1;
        case effClose:
            active = false;
            return 1;
        case effSetSampleRate:
            if (opt > 0.0f) sampleRate = opt;
            return 1;
        case effSetBlockSize:
            return value > 0 ? 1 : 0;
        case effMainsChanged:
            active = value != 0;
            return 1;
        default:
            return 0;
    }
}

void __cdecl process(
    AEffect*, float**, float** outputs, int32_t frames)
{
    if (!outputs || !outputs[0] || !outputs[1]) return;
    if (frames <= 0 || frames > 512) return;

    const double step =
        6.283185307179586 * 440.0 / sampleRate;

    for (int32_t i = 0; i < frames; ++i) {
        float value = active
            ? static_cast<float>(0.2 * std::sin(phase))
            : 0.0f;

        outputs[0][i] = value;
        outputs[1][i] = value;

        phase += step;
        if (phase >= 6.283185307179586)
            phase -= 6.283185307179586;
    }
}
}

extern "C" __declspec(dllexport)
AEffect* __cdecl VSTPluginMain(AudioMasterCallback)
{
    synth = {};
    synth.magic = kVstMagic;
    synth.dispatcher = dispatch;
    synth.numInputs = 0;
    synth.numOutputs = 2;
    synth.flags = kEffectCanReplacing | (1 << 8);
    synth.uniqueID = 0x48454754;
    synth.version = 1;
    synth.processReplacing = process;
    return &synth;
}
