#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <string>
#include "../common/protocol.h"
#include "vst2_abi.h"

namespace {
HMODULE module = nullptr;
AEffect* effect = nullptr;
double sampleRate = 44100.0;
int32_t blockSize = 512;
bool mainsOn = false;

VstIntPtr __cdecl hostCallback(AEffect*, int32_t opcode, int32_t, VstIntPtr, void*, float) {
    switch (opcode) {
        case audioMasterVersion: return 2400;
        case audioMasterGetSampleRate: return static_cast<VstIntPtr>(sampleRate);
        case audioMasterGetBlockSize: return blockSize;
        case audioMasterGetCurrentProcessLevel: return 4; // offline
        case audioMasterGetAutomationState: return 1; // off
        default: return 0;
    }
}

heg::Status sendMidi(const heg::MidiMessage& midi) {
    if (!effect || !mainsOn) return heg::Status::NotLoaded;
    const uint8_t kind = midi.status & 0xF0;
    if (kind != 0x80 && kind != 0x90 && kind != 0xB0) return heg::Status::Unsupported;
    if (midi.data1 > 127 || midi.data2 > 127) return heg::Status::BadRequest;
    VstMidiEvent event{};
    event.type = kVstMidiType;
    event.byteSize = sizeof(event);
    event.deltaFrames = 0;
    event.midiData[0] = static_cast<char>(midi.status);
    event.midiData[1] = static_cast<char>(midi.data1);
    event.midiData[2] = static_cast<char>(midi.data2);
    VstEventsOne events{};
    events.numEvents = 1;
    events.events[0] = &event;
    std::fprintf(stderr, "[HEG] MIDI dispatch begin: %02X %u %u\n",
                 midi.status, midi.data1, midi.data2); std::fflush(stderr);
    effect->dispatcher(effect, effProcessEvents, 0, 0, &events, 0.0f);
    std::fprintf(stderr, "[HEG] MIDI dispatch returned\n"); std::fflush(stderr);
    return heg::Status::Ok;
}

bool readAll(HANDLE handle, void* dest, DWORD bytes) {
    auto* ptr = static_cast<uint8_t*>(dest);
    while (bytes) { DWORD got = 0; if (!ReadFile(handle, ptr, bytes, &got, nullptr) || !got) return false; ptr += got; bytes -= got; }
    return true;
}
bool writeAll(HANDLE handle, const void* src, DWORD bytes) {
    auto* ptr = static_cast<const uint8_t*>(src);
    while (bytes) { DWORD wrote = 0; if (!WriteFile(handle, ptr, bytes, &wrote, nullptr) || !wrote) return false; ptr += wrote; bytes -= wrote; }
    return true;
}
void unload() {
    std::fprintf(stderr, "[HEG] unload: begin\n"); std::fflush(stderr);
    if (effect) {
        if (mainsOn) { std::fprintf(stderr, "[HEG] unload: mains off\n"); std::fflush(stderr); effect->dispatcher(effect, effMainsChanged, 0, 0, nullptr, 0); }
        std::fprintf(stderr, "[HEG] unload: effClose\n"); std::fflush(stderr);
        effect->dispatcher(effect, effClose, 0, 0, nullptr, 0);
        effect = nullptr;
    }
    mainsOn = false;
    if (module) { std::fprintf(stderr, "[HEG] unload: FreeLibrary\n"); std::fflush(stderr); FreeLibrary(module); module = nullptr; }
    std::fprintf(stderr, "[HEG] unload: done\n"); std::fflush(stderr);
}
heg::Status loadPlugin(const std::wstring& path) {
    unload();
    std::fprintf(stderr, "[HEG] load: LoadLibraryExW\n"); std::fflush(stderr);
    module = LoadLibraryExW(path.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if (!module) return heg::Status::LoadFailed;
    using Entry = AEffect* (__cdecl *)(AudioMasterCallback);
    auto entry = reinterpret_cast<Entry>(GetProcAddress(module, "VSTPluginMain"));
    if (!entry) entry = reinterpret_cast<Entry>(GetProcAddress(module, "main"));
    if (!entry) { unload(); return heg::Status::Unsupported; }
    std::fprintf(stderr, "[HEG] load: plugin entry begin\n"); std::fflush(stderr);
    effect = entry(hostCallback);
    std::fprintf(stderr, "[HEG] load: plugin entry returned\n"); std::fflush(stderr);
    if (!effect || effect->magic != kVstMagic || !effect->dispatcher || !effect->processReplacing ||
        !(effect->flags & kEffectCanReplacing)) {
        // If an invalid effect is returned, do not invoke its callbacks.
        effect = nullptr; unload(); return heg::Status::Unsupported;
    }
    std::fprintf(stderr, "[HEG] load: effOpen begin\n"); std::fflush(stderr);
    effect->dispatcher(effect, effOpen, 0, 0, nullptr, 0);
    std::fprintf(stderr, "[HEG] load: effOpen done, inputs=%d outputs=%d flags=0x%x\n", effect->numInputs, effect->numOutputs, effect->flags); std::fflush(stderr);
    return heg::Status::Ok;
}
heg::Status configure(const heg::Configure& cfg) {
    if (!effect) return heg::Status::NotLoaded;
    if (cfg.sampleRate < 8000 || cfg.sampleRate > 384000 || cfg.maxBlockSize == 0 || cfg.maxBlockSize > heg::kMaxFrames) return heg::Status::BadRequest;
    if (mainsOn) { effect->dispatcher(effect, effMainsChanged, 0, 0, nullptr, 0); mainsOn = false; }
    sampleRate = cfg.sampleRate;
    blockSize = static_cast<int32_t>(cfg.maxBlockSize);
    std::fprintf(stderr, "[HEG] configure: samplerate begin\n"); std::fflush(stderr);
    effect->dispatcher(effect, effSetSampleRate, 0, 0, nullptr, static_cast<float>(sampleRate));
    std::fprintf(stderr, "[HEG] configure: blocksize begin\n"); std::fflush(stderr);
    effect->dispatcher(effect, effSetBlockSize, 0, blockSize, nullptr, 0);
    std::fprintf(stderr, "[HEG] configure: mains on begin\n"); std::fflush(stderr);
    effect->dispatcher(effect, effMainsChanged, 0, 1, nullptr, 0);
    std::fprintf(stderr, "[HEG] configure: done\n"); std::fflush(stderr);
    mainsOn = true;
    return heg::Status::Ok;
}
heg::Status process(const heg::AudioBlock& input, heg::AudioBlock& output) {
    if (!effect || !mainsOn) return heg::Status::NotLoaded;
    if (!input.frames || input.frames > heg::kMaxFrames || input.frames > static_cast<uint32_t>(blockSize) || input.channels != 2) return heg::Status::BadRequest;
    if (effect->numInputs > 2 || effect->numOutputs > 2 || effect->numOutputs < 1) return heg::Status::Unsupported;
    output = {};
    output.frames = input.frames;
    output.channels = 2;
    std::array<std::array<float, heg::kMaxFrames>, 2> in{};
    std::array<std::array<float, heg::kMaxFrames>, 2> out{};
    for (uint32_t c = 0; c < 2; ++c) std::copy_n(input.data[c], input.frames, in[c].data());
    float* inputPtrs[2] = {in[0].data(), in[1].data()};
    float* outputPtrs[2] = {out[0].data(), out[1].data()};
#ifdef HEG_TRACE_AUDIO
    std::fprintf(stderr, "[HEG] process: processReplacing begin, frames=%u, inputs=%d, outputs=%d\n", input.frames, effect->numInputs, effect->numOutputs); std::fflush(stderr);
#endif
    effect->processReplacing(effect, inputPtrs, outputPtrs, static_cast<int32_t>(input.frames));
#ifdef HEG_TRACE_AUDIO
    std::fprintf(stderr, "[HEG] process: processReplacing returned\n"); std::fflush(stderr);
#endif
    for (uint32_t c = 0; c < 2; ++c) std::copy_n(out[c].data(), input.frames, output.data[c]);
#ifdef HEG_TRACE_AUDIO
    std::fprintf(stderr, "[HEG] process: output copied\n"); std::fflush(stderr);
#endif
    return heg::Status::Ok;
}
}

int wmain(int argc, wchar_t** argv) {
    std::wstring pipeName = heg::kPipeName;

    // Optional: --pipe-id <nummer>
    // Ohne Argument bleibt der bisherige Pipe-Name erhalten.
    if (argc != 1) {
        if (argc != 3 || std::wstring(argv[1]) != L"--pipe-id") {
            std::wcerr << L"Usage: HEGWorker32.exe [--pipe-id NUMBER]\\n";
            return 1;
        }

        const std::wstring id = argv[2];

        if (id.empty() || id.size() > 32 ||
            !std::all_of(id.begin(), id.end(),
                [](wchar_t c) { return c >= L'0' && c <= L'9'; })) {
            std::wcerr << L"Invalid pipe ID\\n";
            return 1;
        }

        pipeName += L"_" + id;
    }

    std::wcout << L"HEG Worker32: " << pipeName << L"\\n";
    std::wcout.flush();

    HANDLE pipe = CreateNamedPipeW(pipeName.c_str(), PIPE_ACCESS_DUPLEX,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, 1,
        65536, 65536, 0, nullptr);
    if (pipe == INVALID_HANDLE_VALUE) { std::wcerr << L"CreateNamedPipe failed: " << GetLastError() << L"\n"; return 2; }
    bool connected = ConnectNamedPipe(pipe, nullptr) || GetLastError() == ERROR_PIPE_CONNECTED;
    if (!connected) { CloseHandle(pipe); return 3; }
    bool running = true;
    while (running) {
        heg::Header header{};
        if (!readAll(pipe, &header, sizeof(header))) break;
        heg::Status status = heg::Status::BadRequest;
        heg::AudioBlock output{};
        uint32_t outputBytes = 0;
        if (header.magic != heg::kMagic || header.version != heg::kVersion || header.payloadBytes > sizeof(heg::AudioBlock)) break;
        std::array<uint8_t, sizeof(heg::AudioBlock)> payload{};
        if (header.payloadBytes && !readAll(pipe, payload.data(), header.payloadBytes)) break;
        std::fprintf(stderr, "[HEG] command: %u received\n", header.command); std::fflush(stderr);
        switch (static_cast<heg::Command>(header.command)) {
            case heg::Command::Hello:
                if (header.payloadBytes == 0) status = heg::Status::Ok;
                break;
            case heg::Command::Load:
                if (header.payloadBytes >= sizeof(wchar_t) && header.payloadBytes % sizeof(wchar_t) == 0 &&
                    header.payloadBytes <= heg::kMaxPathChars * sizeof(wchar_t)) {
                    std::wstring path(reinterpret_cast<const wchar_t*>(payload.data()), header.payloadBytes / sizeof(wchar_t));
                    if (!path.empty() && path.back() == L'\0') { path.pop_back(); status = loadPlugin(path); }
                }
                break;
            case heg::Command::Configure:
                if (header.payloadBytes == sizeof(heg::Configure)) {
                    heg::Configure cfg{}; memcpy(&cfg, payload.data(), sizeof(cfg)); status = configure(cfg);
                }
                break;
            case heg::Command::Process:
                if (header.payloadBytes == sizeof(heg::AudioBlock)) {
                    heg::AudioBlock in{}; memcpy(&in, payload.data(), sizeof(in)); status = process(in, output);
                    if (status == heg::Status::Ok) outputBytes = sizeof(output);
                }
                break;
            case heg::Command::Midi:
                if (header.payloadBytes == sizeof(heg::MidiMessage)) {
                    heg::MidiMessage midi{};
                    memcpy(&midi, payload.data(), sizeof(midi));
                    status = sendMidi(midi);
                }
                break;
            case heg::Command::Unload:
                if (header.payloadBytes == 0) { unload(); status = heg::Status::Ok; }
                break;
            case heg::Command::Quit:
                if (header.payloadBytes == 0) { unload(); status = heg::Status::Ok; running = false; }
                break;
        }
        std::fprintf(stderr, "[HEG] command: %u status=%u\n", header.command, static_cast<unsigned>(status)); std::fflush(stderr);
        heg::Reply reply{heg::kMagic, heg::kVersion, static_cast<uint32_t>(status), outputBytes};
        if (!writeAll(pipe, &reply, sizeof(reply))) break;
        if (outputBytes && !writeAll(pipe, &output, outputBytes)) break;
    }
    unload(); DisconnectNamedPipe(pipe); CloseHandle(pipe);
    return 0;
}
