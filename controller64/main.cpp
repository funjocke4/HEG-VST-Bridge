#include <fstream>
#include <vector>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>
#include "../common/protocol.h"
namespace {
bool transfer(HANDLE pipe, heg::Command cmd, const void* data, uint32_t bytes, heg::AudioBlock* audio = nullptr) {
    heg::Header request{heg::kMagic, heg::kVersion, static_cast<uint32_t>(cmd), bytes};
    DWORD done = 0;
    if (!WriteFile(pipe, &request, sizeof(request), &done, nullptr) || done != sizeof(request)) return false;
    if (bytes && (!WriteFile(pipe, data, bytes, &done, nullptr) || done != bytes)) return false;
    heg::Reply reply{};
    if (!ReadFile(pipe, &reply, sizeof(reply), &done, nullptr) || done != sizeof(reply)) return false;
    if (reply.magic != heg::kMagic || reply.version != heg::kVersion || reply.payloadBytes > sizeof(heg::AudioBlock)) return false;
    if (reply.payloadBytes) {
        if (!audio || reply.payloadBytes != sizeof(*audio)) return false;
        if (!ReadFile(pipe, audio, sizeof(*audio), &done, nullptr) || done != sizeof(*audio)) return false;
    }
    std::wcout << L"Befehl " << static_cast<uint32_t>(cmd) << L": Status " << reply.status << L"\n";
    return reply.status == static_cast<uint32_t>(heg::Status::Ok);
}
}
int wmain(int argc, wchar_t** argv) {
    if (argc < 2 || argc > 3) {
        std::wcout << L"Verwendung: HEGController64.exe C:\\Pfad\\Synth.dll [--midi-test|--audio-test]\n";
        return 1;
    }
    std::wstring mode = argc == 3 ? argv[2] : L"";
    if (mode != L"" && mode != L"--midi-test" && mode != L"--audio-test") {
        std::wcerr << L"Unbekannter Modus.\n"; return 1;
    }
    std::wstring path = argv[1];
    DWORD fileAttr = GetFileAttributesW(path.c_str());
    if (fileAttr == INVALID_FILE_ATTRIBUTES || (fileAttr & FILE_ATTRIBUTE_DIRECTORY)) {
        std::wcerr << L"DLL nicht gefunden.\n"; return 2;
    }
    if (!WaitNamedPipeW(heg::kPipeName, 10000)) { std::wcerr << L"Worker32 starten, dann erneut versuchen.\n"; return 3; }
    HANDLE pipe = CreateFileW(heg::kPipeName, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (pipe == INVALID_HANDLE_VALUE) return 4;
    bool ok = transfer(pipe, heg::Command::Hello, nullptr, 0);
    if (ok) ok = transfer(pipe, heg::Command::Load, path.c_str(), static_cast<uint32_t>((path.size() + 1) * sizeof(wchar_t)));
    heg::Configure cfg{44100.0, 512};
    if (ok) ok = transfer(pipe, heg::Command::Configure, &cfg, sizeof(cfg));
    if (ok && mode == L"--midi-test") {
        heg::MidiMessage on{0x90, 60, 100, 0};
        heg::MidiMessage off{0x80, 60, 0, 0};
        ok = transfer(pipe, heg::Command::Midi, &on, sizeof(on));
        if (ok) ok = transfer(pipe, heg::Command::Midi, &off, sizeof(off));
        std::wcout << L"MIDI-Test abgeschlossen (ohne Audioausgabe).\n";
    }

    if (ok && mode == L"--audio-test") {
        heg::MidiMessage on{0x90, 60, 100, 0};
        heg::MidiMessage off{0x80, 60, 0, 0};

        ok = transfer(pipe, heg::Command::Midi,
                      &on, sizeof(on));

        std::vector<int16_t> samples;
        samples.reserve(86 * 512 * 2);
        double peak = 0.0;
        double sumSquares = 0.0;
        uint64_t count = 0;

        for (int block = 0; ok && block < 86; ++block) {
            heg::AudioBlock input{};
            input.frames = 512;
            input.channels = 2;

            for (uint32_t i = 0; i < 512; ++i) {
                double t = (block * 512 + i) / 44100.0;
                float tone = static_cast<float>(
                    0.2 * std::sin(6.283185307179586 * 440.0 * t)
                );
                input.data[0][i] = tone;
                input.data[1][i] = tone;
            }

            heg::AudioBlock output{};

            ok = transfer(pipe, heg::Command::Process,
                          &input, sizeof(input), &output);

            if (!ok) break;

            if (output.frames != 512 || output.channels != 2) {
                std::wcout << L"Ungueltige Audioantwort.\n";
                ok = false;
                break;
            }

            for (uint32_t i = 0; i < output.frames; ++i) {
                for (int c = 0; c < 2; ++c) {
                    double v = output.data[c][i];

                    if (!std::isfinite(v)) {
                        ok = false;
                        break;
                    }

                    double a = std::fabs(v);
                    if (a > peak) peak = a;

                    sumSquares += v * v;
                    ++count;

                    if (v > 1.0) v = 1.0;
                    if (v < -1.0) v = -1.0;

                    samples.push_back(
                        static_cast<int16_t>(v * 32767.0));
                }
                if (!ok) break;
            }
        }

        if (ok) {
            ok = transfer(pipe, heg::Command::Midi,
                          &off, sizeof(off));
        }

        if (ok && count > 0) {
            double rms = std::sqrt(sumSquares / count);

            std::wcout << L"HEG Audio Peak: " << peak << L"\n";
            std::wcout << L"HEG Audio RMS: " << rms << L"\n";
            std::wcout << L"Samples: " << count << L"\n";

            std::ofstream wav("HEG-Audiotest.wav",
                              std::ios::binary);

            auto w16 = [&](uint16_t v) {
                char b[2] = {
                    char(v & 255),
                    char((v >> 8) & 255)
                };
                wav.write(b, 2);
            };

            auto w32 = [&](uint32_t v) {
                w16(uint16_t(v & 65535));
                w16(uint16_t(v >> 16));
            };

            uint32_t bytes =
                static_cast<uint32_t>(samples.size() * 2);

            wav.write("RIFF", 4);
            w32(36 + bytes);
            wav.write("WAVEfmt ", 8);
            w32(16);
            w16(1);
            w16(2);
            w32(44100);
            w32(44100 * 4);
            w16(4);
            w16(16);
            wav.write("data", 4);
            w32(bytes);

            for (int16_t sample : samples)
                w16(static_cast<uint16_t>(sample));

            wav.close();
            ok = wav.good();

            if (ok)
                std::wcout << L"HEG-Audiotest.wav erstellt.\n";
        }
    }
    if (mode.empty()) std::wcout << L"Sicherer Lade-/Konfigurationstest: Audio und MIDI uebersprungen.\n";
    bool quitOk = transfer(pipe, heg::Command::Quit, nullptr, 0);
    CloseHandle(pipe);
    ok = ok && quitOk;
    std::wcout << (ok ? L"IPC/Plugin-Test erfolgreich.\n" : L"Test fehlgeschlagen. Details oben.\n");
    return ok ? 0 : 5;
}
