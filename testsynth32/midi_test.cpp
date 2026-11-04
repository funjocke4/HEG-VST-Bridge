#include <windows.h>
#include <cmath>
#include <cstdint>

#include "../worker32/vst2_abi.h"

namespace {

AEffect synth{};

double phase = 0.0;
double sampleRate = 44100.0;
double frequency = 440.0;

bool active = false;
bool notePlaying = false;
int currentNote = -1;

VstIntPtr __cdecl dispatch(
    AEffect*, int32_t opcode, int32_t,
    VstIntPtr value, void* ptr, float opt)
{
    switch (opcode) {
        case effOpen:
            phase = 0.0;
            notePlaying = false;
            currentNote = -1;
            return 1;

        case effClose:
            notePlaying = false;
            active = false;
            return 1;

        case effSetSampleRate:
            if (opt > 0)
                sampleRate = opt;
            return 1;

        case effSetBlockSize:
            return value > 0 ? 1 : 0;

        case effMainsChanged:
            active = value != 0;
            if (!active)
                notePlaying = false;
            return 1;

        case effProcessEvents: {
            if (!ptr)
                return 0;

            auto* events =
                static_cast<VstEventsOne*>(ptr);

            if (events->numEvents < 1 ||
                !events->events[0])
                return 0;

            auto* event = events->events[0];

            if (event->type != kVstMidiType)
                return 0;

            uint8_t status =
                static_cast<uint8_t>(event->midiData[0]);
            uint8_t note =
                static_cast<uint8_t>(event->midiData[1]);
            uint8_t velocity =
                static_cast<uint8_t>(event->midiData[2]);

            switch (status & 0xF0) {
                case 0x90:
                    if (velocity > 0) {
                        currentNote = note;
                        frequency = 440.0 * std::pow(
                            2.0, (static_cast<int>(note) - 69) / 12.0);
                        notePlaying = true;
                    } else if (currentNote == note) {
                        notePlaying = false;
                    }
                    break;

                case 0x80:
                    if (currentNote == note)
                        notePlaying = false;
                    break;

                case 0xB0:
                    if (note == 120 || note == 123)
                        notePlaying = false;
                    break;
            }

            return 1;
        }

        default:
            return 0;
    }
}

void __cdecl process(
    AEffect*, float**, float** outputs, int32_t frames)
{
    if (!outputs || !outputs[0] || !outputs[1])
        return;

    const double step =
        6.283185307179586 * frequency / sampleRate;

    for (int32_t i = 0; i < frames; ++i) {
        float signal = (active && notePlaying)
            ? static_cast<float>(0.2 * std::sin(phase))
            : 0.0f;

        outputs[0][i] = signal;
        outputs[1][i] = signal;

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
    synth.uniqueID = 0x48474D54;
    synth.version = 1;
    synth.processReplacing = process;

    return &synth;
}
