#pragma once

#include "worker_process.h"
#include "async_bridge.h"
#include "realtime_audio_adapter.h"

#include <string>
#include <atomic>
#include <cstdint>

namespace heg {

class ManagedAsyncBridge {
public:
    ManagedAsyncBridge() = default;
    ~ManagedAsyncBridge() { stop(); }

    ManagedAsyncBridge(const ManagedAsyncBridge&) = delete;
    ManagedAsyncBridge& operator=(const ManagedAsyncBridge&) = delete;

    bool start(const std::wstring& workerExe,
               const std::wstring& pluginDll,
               double sampleRate,
               uint32_t blockSize) {
        if (started_)
            return false;

        adapter_ = RealtimeAudioAdapter{};

        static std::atomic<uint32_t> nextId{1};

        const std::wstring pipeId =
            std::to_wstring(GetCurrentProcessId()) +
            std::to_wstring(nextId.fetch_add(1));

        const std::wstring pipeName =
            std::wstring(kPipeName) + L"_" + pipeId;

        if (!process_.start(workerExe, pipeId))
            return false;

        if (!bridge_.start(
                pluginDll, sampleRate, blockSize, pipeName)) {
            process_.stop();
            return false;
        }

        started_ = true;
        return true;
    }

    bool ready() const {
        return started_ &&
               process_.running() &&
               bridge_.ready();
    }

    BridgeError error() const {
        return bridge_.error();
    }

    bool submit(const AudioBlock& input) {
        return ready() && bridge_.submit(input);
    }

    bool submitMidi(const AudioBlock& input,
                    const MidiMessage& midi) {
        return ready() && bridge_.submitMidi(input, midi);
    }

    bool receive(AudioBlock& output) {
        return bridge_.receive(output);
    }

    bool queueMidi(const MidiMessage& midi) {
        return started_ && bridge_.enqueueMidi(midi);
    }

    bool render(const AudioBlock& input, AudioBlock& output) {
        output = {};
        if (!started_ || !bridge_.ready())
            return false;

        return adapter_.render(bridge_, input, output);
    }

    // Notabschaltung bei blockiertem Worker.
    // Nicht aus dem Echtzeit-Audiothread aufrufen.
    void abort() {
        if (!started_)
            return;

        // Zuerst den separaten Prozess beenden.
        // Dadurch werden dessen Pipe-Handles geschlossen.
        process_.stop();

        // Anschliessend den Kommunikationsthread beenden.
        bridge_.stop();

        started_ = false;
    }

    void stop() {
        if (!started_)
            return;

        // Erst die Kommunikation beenden.
        bridge_.stop();

        // Danach den eigenen Worker-Prozess beenden.
        process_.stop();

        started_ = false;
    }

private:
    WorkerProcess process_;
    AsyncBridge bridge_;
    RealtimeAudioAdapter adapter_;
    bool started_ = false;
};

} // namespace heg
