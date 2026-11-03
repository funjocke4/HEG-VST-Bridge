#pragma once

#include "async_bridge.h"

namespace heg {

// Prototyp fuer asynchrones Audio.
// Keine direkte Pipe-Kommunikation im Aufrufer.
class RealtimeAudioAdapter {
public:
    bool render(AsyncBridge& bridge,
                const AudioBlock& input,
                AudioBlock& output) {
        output = {};

        if (input.frames == 0 ||
            input.frames > kMaxFrames ||
            input.channels != 2)
            return false;

        output.frames = input.frames;
        output.channels = 2;

        // Erwarteter Block: genau ein Aufruf vorher.
        const uint64_t expected =
            nextSequence_ > 1 ? nextSequence_ - 1 : 0;

        AudioBlock completed{};
        uint64_t receivedSequence = 0;

        bool received =
            bridge.receiveTagged(completed, receivedSequence);

        bool valid =
            received &&
            expected != 0 &&
            receivedSequence == expected &&
            completed.frames == input.frames &&
            completed.channels == 2;

        if (valid)
            output = completed;

        // Jeder Aufruf bekommt eine neue Kennnummer.
        // Bei belegter Mailbox wird der Block ausgelassen.
        bridge.submitTagged(input, nextSequence_++);
        return valid;
    }

private:
    uint64_t nextSequence_ = 1;
};

} // namespace heg
