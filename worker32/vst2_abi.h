#pragma once
// Minimal VST2 binary ABI declaration for interoperability with user-owned DLLs.
// No Steinberg VST2 SDK source or headers are copied into this project.
#include <cstdint>
#if !defined(_WIN32) || !defined(_M_IX86)
#error This legacy host requires 32-bit Windows (MSVC x86).
#endif

struct AEffect;
using VstIntPtr = intptr_t;
using AudioMasterCallback = VstIntPtr (__cdecl *)(AEffect*, int32_t, int32_t, VstIntPtr, void*, float);
using Dispatcher = VstIntPtr (__cdecl *)(AEffect*, int32_t, int32_t, VstIntPtr, void*, float);
using Process = void (__cdecl *)(AEffect*, float**, float**, int32_t);
using SetParameter = void (__cdecl *)(AEffect*, int32_t, float);
using GetParameter = float (__cdecl *)(AEffect*, int32_t);
struct AEffect {
    int32_t magic;
    Dispatcher dispatcher;
    Process process;
    SetParameter setParameter;
    GetParameter getParameter;
    int32_t numPrograms, numParams, numInputs, numOutputs, flags;
    intptr_t reserved1, reserved2;
    int32_t initialDelay, realQualities, offQualities;
    float ioRatio;
    void* object;
    void* user;
    int32_t uniqueID, version;
    Process processReplacing;
    Process processDoubleReplacing; // intentionally pointer-sized; unused
    char future[56];
};
static_assert(sizeof(AEffect) == 144, "Unexpected VST2 effect ABI layout");
constexpr int32_t kVstMagic = 0x56737450; // 'VstP'
constexpr int32_t kEffectCanReplacing = 1 << 4;
// Common VST2 dispatcher opcodes required by this minimal bridge.
constexpr int32_t effOpen = 0;
constexpr int32_t effClose = 1;
constexpr int32_t effSetSampleRate = 10;
constexpr int32_t effSetBlockSize = 11;
constexpr int32_t effMainsChanged = 12;
constexpr int32_t audioMasterVersion = 1;
constexpr int32_t audioMasterGetSampleRate = 16;
constexpr int32_t audioMasterGetBlockSize = 17;
constexpr int32_t audioMasterGetCurrentProcessLevel = 23;
constexpr int32_t audioMasterGetAutomationState = 24;

// Minimal VST2 MIDI event layouts (32-bit ABI).
constexpr int32_t effProcessEvents = 25;
constexpr int32_t kVstMidiType = 1;
struct VstMidiEvent {
    int32_t type;
    int32_t byteSize;
    int32_t deltaFrames;
    int32_t flags;
    int32_t noteLength;
    int32_t noteOffset;
    char midiData[4];
    char detune;
    char noteOffVelocity;
    char reserved1;
    char reserved2;
};
struct VstEventsOne {
    int32_t numEvents;
    VstIntPtr reserved;
    VstMidiEvent* events[2];
};
static_assert(sizeof(VstMidiEvent) == 32, "Unexpected VST2 MIDI event size");
static_assert(sizeof(VstEventsOne) == 16, "Unexpected VST2 event container size");
