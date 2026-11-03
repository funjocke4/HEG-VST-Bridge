#pragma once

#include <windows.h>
#include <string>
#include <vector>

namespace heg {

class WorkerProcess {
public:
    WorkerProcess() = default;
    ~WorkerProcess() { stop(); }

    WorkerProcess(const WorkerProcess&) = delete;
    WorkerProcess& operator=(const WorkerProcess&) = delete;

    bool start(const std::wstring& executable,
               const std::wstring& pipeId = L"") {
        if (process_ || executable.empty())
            return false;

        std::wstring command = L"\"" + executable + L"\"";

        if (!pipeId.empty()) {
            if (pipeId.size() > 32)
                return false;

            for (wchar_t c : pipeId) {
                if (c < L'0' || c > L'9')
                    return false;
            }

            command += L" --pipe-id " + pipeId;
        }
        std::vector<wchar_t> args(
            command.begin(), command.end()
        );
        args.push_back(L'\0');

        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);

        PROCESS_INFORMATION info{};

        if (!CreateProcessW(
                executable.c_str(),
                args.data(),
                nullptr, nullptr,
                FALSE,
                CREATE_NO_WINDOW,
                nullptr, nullptr,
                &startup, &info))
            return false;

        process_ = info.hProcess;
        pid_ = info.dwProcessId;
        CloseHandle(info.hThread);
        return true;
    }

    bool running() const {
        if (!process_)
            return false;

        return WaitForSingleObject(
            process_, 0
        ) == WAIT_TIMEOUT;
    }

    DWORD processId() const {
        return pid_;
    }

    void stop() {
        if (!process_)
            return;

        if (running()) {
            TerminateProcess(process_, 1);
            WaitForSingleObject(process_, 2000);
        }

        CloseHandle(process_);
        process_ = nullptr;
        pid_ = 0;
    }

private:
    HANDLE process_ = nullptr;
    DWORD pid_ = 0;
};

} // namespace heg
