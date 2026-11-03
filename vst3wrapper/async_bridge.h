#pragma once

#include "bridge_client.h"

#include <atomic>
#include <cstdint>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <string>
#include <utility>

namespace heg {

enum class BridgeError {
    None,
    ConnectionFailed,
    LoadFailed,
    ConfigureFailed,
    ProcessingFailed
};

class AsyncBridge {
public:
    AsyncBridge() = default;

    ~AsyncBridge() {
        stop();
    }

    AsyncBridge(const AsyncBridge&) = delete;
    AsyncBridge& operator=(const AsyncBridge&) = delete;

    bool start(const std::wstring& pluginPath,
               double sampleRate,
               uint32_t blockSize,
               std::wstring pipeName = kPipeName) {
        if (worker_.joinable() ||
            pluginPath.empty() ||
            blockSize == 0 ||
            blockSize > kMaxFrames)
            return false;

        stopping_.store(false);
        ready_.store(false);
        error_.store(BridgeError::None);

        worker_ = std::thread(
            [this, pluginPath, sampleRate, blockSize, pipeName]() {
                workerLoop(pluginPath, sampleRate, blockSize, pipeName);
            }
        );

        return true;
    }

    void stop() {
        stopping_.store(true);
        condition_.notify_all();

        if (worker_.joinable())
            worker_.join();

        ready_.store(false);
    }

    bool running() const {
        return worker_.joinable() && !stopping_.load();
    }

    bool ready() const {
        return ready_.load();
    }

    BridgeError error() const {
        return error_.load();
    }

    // Ein MIDI-Ereignis zusammen mit einem Audioblock.
    bool submitMidi(const AudioBlock& input,
                    const MidiMessage& midi) {
        return submitInternal(input, &midi, 0);
    }

    bool submit(const AudioBlock& input) {
        return submitInternal(input, nullptr, 0);
    }

    bool submitTagged(const AudioBlock& input,
                      uint64_t sequence) {
        return submitInternal(input, nullptr, sequence);
    }

private:
    bool submitInternal(const AudioBlock& input,
                        const MidiMessage* midi,
                        uint64_t sequence) {
        if (!ready() ||
            input.frames == 0 ||
            input.frames > kMaxFrames ||
            input.channels != 2)
            return false;

        std::unique_lock<std::mutex> lock(
            mutex_, std::try_to_lock
        );

        if (!lock.owns_lock() || inFlight_)
            return false;

        input_ = input;
        inputSequence_ = sequence;
        midiPending_ = midi != nullptr;
        if (midi)
            midi_ = *midi;
        inputPending_ = true;
        inFlight_ = true;

        lock.unlock();
        condition_.notify_one();

        return true;
    }

public:
    // Nicht blockierender Versuch, fertiges Audio abzuholen.
    bool receive(AudioBlock& output) {
        uint64_t sequence = 0;
        return receiveTagged(output, sequence);
    }

    bool receiveTagged(AudioBlock& output,
                       uint64_t& sequence) {
        std::unique_lock<std::mutex> lock(
            mutex_, std::try_to_lock
        );

        if (!lock.owns_lock() || !outputPending_)
            return false;

        output = output_;
        sequence = outputSequence_;
        outputPending_ = false;
        inFlight_ = false;
        return true;
    }

private:
    void workerLoop(const std::wstring& pluginPath,
                    double sampleRate,
                    uint32_t blockSize,
                    const std::wstring& pipeName) {
        BridgeClient client(pipeName);

        // Auf die Named Pipe des gestarteten Workers warten.
        // Nur im Hintergrundthread, niemals im Audiothread.
        bool connected = false;

        for (int attempt = 0;
             attempt < 60 && !stopping_.load();
             ++attempt) {

            if (client.connect()) {
                connected = true;
                break;
            }

            std::this_thread::sleep_for(
                std::chrono::milliseconds(50)
            );
        }

        if (!connected) {
            if (!stopping_.load())
                error_.store(BridgeError::ConnectionFailed);
            return;
        }

        if (stopping_.load())
            return;

        if (!client.loadPlugin(pluginPath)) {
            error_.store(BridgeError::LoadFailed);
            return;
        }

        if (stopping_.load())
            return;

        if (!client.configure(sampleRate, blockSize)) {
            error_.store(BridgeError::ConfigureFailed);
            return;
        }

        ready_.store(true);

        while (!stopping_.load()) {
            AudioBlock input{};
            uint64_t sequence = 0;
            MidiMessage midi{};
            bool sendMidi = false;

            {
                std::unique_lock<std::mutex> lock(mutex_);

                condition_.wait(lock, [this]() {
                    return stopping_.load() || inputPending_;
                });

                if (stopping_.load())
                    break;

                input = input_;
                sequence = inputSequence_;
                midi = midi_;
                sendMidi = midiPending_;
                midiPending_ = false;
                inputPending_ = false;
            }

            AudioBlock output{};

            if (sendMidi &&
                !client.midi(midi.status, midi.data1, midi.data2)) {
                error_.store(BridgeError::ProcessingFailed);
                ready_.store(false);
                break;
            }

            if (!client.process(input, output)) {
                error_.store(BridgeError::ProcessingFailed);
                ready_.store(false);
                break;
            }

            {
                std::lock_guard<std::mutex> lock(mutex_);
                output_ = output;
                outputSequence_ = sequence;
                outputPending_ = true;
            }
        }

        ready_.store(false);
        client.unloadPlugin();
    }

    std::thread worker_;
    std::mutex mutex_;
    std::condition_variable condition_;

    std::atomic<bool> stopping_{false};
    std::atomic<bool> ready_{false};
    std::atomic<BridgeError> error_{BridgeError::None};

    AudioBlock input_{};
    AudioBlock output_{};
    uint64_t inputSequence_ = 0;
    uint64_t outputSequence_ = 0;

    MidiMessage midi_{};
    bool midiPending_ = false;

    bool inputPending_ = false;
    bool outputPending_ = false;
    bool inFlight_ = false;
};

}
