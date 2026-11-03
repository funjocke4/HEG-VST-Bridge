#include "../common/protocol.h"
#include <windows.h>
#include <cstdint>
#include <iostream>

bool readAll(HANDLE pipe, void* buffer, DWORD size) {
    auto* p = static_cast<uint8_t*>(buffer);
    while (size) {
        DWORD n = 0;
        if (!ReadFile(pipe, p, size, &n, nullptr) || !n)
            return false;
        p += n;
        size -= n;
    }
    return true;
}

bool writeAll(HANDLE pipe, const void* buffer, DWORD size) {
    auto* p = static_cast<const uint8_t*>(buffer);
    while (size) {
        DWORD n = 0;
        if (!WriteFile(pipe, p, size, &n, nullptr) || !n)
            return false;
        p += n;
        size -= n;
    }
    return true;
}

int main() {
    HANDLE pipe = CreateNamedPipeW(
        heg::kPipeName,
        PIPE_ACCESS_DUPLEX,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
        1, 65536, 65536, 0, nullptr
    );

    if (pipe == INVALID_HANDLE_VALUE)
        return 1;

    std::cout << "Test-Worker bereit" << std::endl;

    if (!ConnectNamedPipe(pipe, nullptr) &&
        GetLastError() != ERROR_PIPE_CONNECTED) {
        CloseHandle(pipe);
        return 2;
    }

    for (;;) {
        heg::Header header{};

        if (!readAll(pipe, &header, sizeof(header)))
            break;

        if (header.magic != heg::kMagic ||
            header.version != heg::kVersion ||
            header.payloadBytes > 65536)
            break;

        uint8_t payload[65536]{};

        if (header.payloadBytes &&
            !readAll(pipe, payload, header.payloadBytes))
            break;

        if (header.command ==
            static_cast<uint32_t>(heg::Command::Process)) {
            std::cout << "Audio empfangen; simuliere Haenger"
                      << std::endl;
            Sleep(8000);
            break;
        }

        heg::Reply reply{
            heg::kMagic,
            heg::kVersion,
            static_cast<uint32_t>(heg::Status::Ok),
            0
        };

        if (!writeAll(pipe, &reply, sizeof(reply)))
            break;
    }

    DisconnectNamedPipe(pipe);
    CloseHandle(pipe);
    return 0;
}
