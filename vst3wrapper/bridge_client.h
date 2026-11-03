#pragma once

#include "../common/protocol.h"
#include <windows.h>
#include <string>
#include <utility>
#include <cstdint>

namespace heg {

class BridgeClient {
public:
    explicit BridgeClient(
        std::wstring pipeName = kPipeName)
        : pipeName_(std::move(pipeName)) {}
    ~BridgeClient() { disconnect(); }

    BridgeClient(const BridgeClient&) = delete;
    BridgeClient& operator=(const BridgeClient&) = delete;

    bool connect() {
        if (connected()) return true;

        pipe_ = CreateFileW(
            pipeName_.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            0, nullptr, OPEN_EXISTING,
            FILE_FLAG_OVERLAPPED, nullptr
        );

        if (!connected()) return false;

        if (!transfer(Command::Hello, nullptr, 0)) {
            disconnect();
            return false;
        }
        return true;
    }

    void disconnect() {
        if (connected()) CloseHandle(pipe_);
        pipe_ = INVALID_HANDLE_VALUE;
    }

    bool connected() const {
        return pipe_ != INVALID_HANDLE_VALUE;
    }

    bool loadPlugin(const std::wstring& path) {
        if (path.empty() || path.size() >= kMaxPathChars)
            return false;

        const auto bytes = static_cast<uint32_t>(
            (path.size() + 1) * sizeof(wchar_t)
        );

        return transfer(Command::Load, path.c_str(), bytes);
    }

    bool unloadPlugin() {
        return transfer(Command::Unload, nullptr, 0);
    }

    bool configure(double sampleRate, uint32_t blockSize) {
        if (blockSize == 0 || blockSize > kMaxFrames)
            return false;

        Configure cfg{sampleRate, blockSize};

        return transfer(
            Command::Configure,
            &cfg, sizeof(cfg)
        );
    }

    bool midi(uint8_t status, uint8_t d1, uint8_t d2) {
        MidiMessage msg{status, d1, d2, 0};

        return transfer(
            Command::Midi,
            &msg, sizeof(msg)
        );
    }

    bool process(const AudioBlock& input, AudioBlock& output) {
        if (input.frames == 0 ||
            input.frames > kMaxFrames ||
            input.channels == 0 ||
            input.channels > kMaxChannels)
            return false;

        return transfer(
            Command::Process,
            &input, sizeof(input),
            &output
        );
    }

private:
    std::wstring pipeName_;
    HANDLE pipe_ = INVALID_HANDLE_VALUE;

    static constexpr DWORD kIoTimeoutMs = 3000;

    bool ioOnce(bool writing, void* buffer,
                DWORD size, DWORD& transferred) {
        transferred = 0;

        HANDLE event = CreateEventW(
            nullptr, TRUE, FALSE, nullptr
        );

        if (!event)
            return false;

        OVERLAPPED ov{};
        ov.hEvent = event;

        BOOL ok = writing
            ? WriteFile(pipe_, buffer, size, nullptr, &ov)
            : ReadFile(pipe_, buffer, size, nullptr, &ov);

        bool success = false;

        if (ok) {
            success = GetOverlappedResult(
                pipe_, &ov, &transferred, FALSE
            ) != FALSE;
        } else if (GetLastError() == ERROR_IO_PENDING) {
            DWORD waitResult = WaitForSingleObject(
                event, kIoTimeoutMs
            );

            if (waitResult == WAIT_OBJECT_0) {
                success = GetOverlappedResult(
                    pipe_, &ov, &transferred, FALSE
                ) != FALSE;
            } else {
                // Ausstehenden Zugriff abbrechen.
                CancelIoEx(pipe_, &ov);

                // Abschluss abwarten, bevor ov zerstoert wird.
                DWORD ignored = 0;
                GetOverlappedResult(
                    pipe_, &ov, &ignored, TRUE
                );
            }
        }

        CloseHandle(event);

        if (!success)
            disconnect();

        return success && transferred > 0;
    }

    bool writeAll(const void* data, uint32_t size) {
        auto* p = const_cast<uint8_t*>(
            static_cast<const uint8_t*>(data)
        );

        while (size > 0) {
            DWORD written = 0;

            if (!ioOnce(true, p, size, written))
                return false;

            p += written;
            size -= written;
        }
        return true;
    }

    bool readAll(void* data, uint32_t size) {
        auto* p = static_cast<uint8_t*>(data);

        while (size > 0) {
            DWORD received = 0;

            if (!ioOnce(false, p, size, received))
                return false;

            p += received;
            size -= received;
        }
        return true;
    }

    bool transfer(Command command,
                  const void* payload,
                  uint32_t bytes,
                  AudioBlock* output = nullptr) {
        if (!connected()) return false;

        Header header{
            kMagic, kVersion,
            static_cast<uint32_t>(command), bytes
        };

        if (!writeAll(&header, sizeof(header)))
            return false;

        if (bytes && (!payload || !writeAll(payload, bytes)))
            return false;

        Reply reply{};
        if (!readAll(&reply, sizeof(reply)))
            return false;

        if (reply.magic != kMagic ||
            reply.version != kVersion ||
            reply.payloadBytes > sizeof(AudioBlock))
            return false;

        if (reply.payloadBytes > 0) {
            if (!output ||
                reply.payloadBytes != sizeof(AudioBlock))
                return false;

            if (!readAll(output, sizeof(AudioBlock)))
                return false;
        }

        return reply.status == static_cast<uint32_t>(Status::Ok);
    }
};

} // namespace heg
