#include "../common/protocol.h"
#include <windows.h>
#include <iostream>

int main() {
    HANDLE pipe = CreateNamedPipeW(
        heg::kPipeName,
        PIPE_ACCESS_DUPLEX,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
        1, 65536, 65536, 0, nullptr
    );

    if (pipe == INVALID_HANDLE_VALUE) {
        std::cerr << "FEHLER: Pipe konnte nicht erstellt werden\n";
        return 1;
    }

    std::cout << "Test-Worker bereit" << std::endl;

    BOOL connected = ConnectNamedPipe(pipe, nullptr);
    if (!connected && GetLastError() != ERROR_PIPE_CONNECTED) {
        CloseHandle(pipe);
        return 2;
    }

    std::cout << "Verbindung angenommen, antworte nicht"
              << std::endl;

    Sleep(8000);

    DisconnectNamedPipe(pipe);
    CloseHandle(pipe);
    return 0;
}
