#pragma once

#include "bridge_client.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <string>
#include <utility>

namespace heg {

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
               uint32_t blockSize) {
        if (worker_.joinable() ||
            pluginPath.empty() ||
            blockSize == 0 ||
            blockSize > kMaxFrames)
            return false;

        stopping_.store(false);
        ready_.store(false);

        worker_ = std::thread(
            [this, pluginPath, sampleRate, blockSize]() {
                workerLoop(pluginPath, sampleRate, blockSize);
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

    // Ein MIDI-Ereignis zusammen mit einem Audioblock.
    bool submitMidi(const AudioBlock& input,
                    const MidiMessage& midi) {
        return submitInternal(input, &midi);
    }

    bool submit(const AudioBlock& input) {
        return submitInternal(input, nullptr);
    }

private:
    bool submitInternal(const AudioBlock& input,
                        const MidiMessage* midi) {
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
        std::unique_lock<std::mutex> lock(
            mutex_, std::try_to_lock
        );

        if (!lock.owns_lock() || !outputPending_)
            return false;

        output = output_;
        outputPending_ = false;
        inFlight_ = false;
        return true;
    }

private:
    void workerLoop(const std::wstring& pluginPath,
                    double sampleRate,
                    uint32_t blockSize) {
        BridgeClient client;

        if (!client.connect())
            return;

        if (stopping_.load() ||
            !client.loadPlugin(pluginPath))
            return;

        if (stopping_.load() ||
            !client.configure(sampleRate, blockSize))
            return;

        ready_.store(true);

        while (!stopping_.load()) {
            AudioBlock input{};
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
                midi = midi_;
                sendMidi = midiPending_;
                midiPending_ = false;
                inputPending_ = false;
            }

            AudioBlock output{};

            if (sendMidi &&
                !client.midi(midi.status, midi.data1, midi.data2)) {
                ready_.store(false);
                break;
            }

            if (!client.process(input, output)) {
                ready_.store(false);
                break;
            }

            {
                std::lock_guard<std::mutex> lock(mutex_);
                output_ = output;
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

    AudioBlock input_{};
    AudioBlock output_{};

    MidiMessage midi_{};
    bool midiPending_ = false;

    bool inputPending_ = false;
    bool outputPending_ = false;
    bool inFlight_ = false;
};

}
