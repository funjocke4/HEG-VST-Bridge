#pragma once
#include <cstdint>

namespace heg {
constexpr uint32_t kMagic = 0x31474548; // HEG1
constexpr uint32_t kVersion = 1;
constexpr uint32_t kMaxFrames = 512;
constexpr uint32_t kMaxChannels = 2;
constexpr uint32_t kMaxPathChars = 1024;
constexpr wchar_t kPipeName[] = LR"(\\.\pipe\HEGVSTBridge_v1)";

enum class Command : uint32_t { Hello=1, Load=2, Configure=3, Process=4, Unload=5, Quit=6, Midi=7 };
enum class Status : uint32_t { Ok=0, BadRequest=1, LoadFailed=2, NotLoaded=3, Unsupported=4, InternalError=5 };
#pragma pack(push, 1)
struct Header { uint32_t magic; uint32_t version; uint32_t command; uint32_t payloadBytes; };
struct Reply { uint32_t magic; uint32_t version; uint32_t status; uint32_t payloadBytes; };
struct Configure { double sampleRate; uint32_t maxBlockSize; };
struct MidiMessage { uint8_t status; uint8_t data1; uint8_t data2; uint8_t reserved; };
struct AudioBlock { uint32_t frames; uint32_t channels; float data[kMaxChannels][kMaxFrames]; };
#pragma pack(pop)
}
