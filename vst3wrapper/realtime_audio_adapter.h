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

        // Zuerst das vorherige Ergebnis abholen.
        AudioBlock completed{};
        bool received = bridge.receive(completed);

        if (received &&
            completed.frames == input.frames &&
            completed.channels == 2) {
            output = completed;
        } else {
            received = false;
            // output bleibt Stille.
        }

        // Neuen Block uebergeben, ohne auf das
        // Ergebnis des Workers zu warten.
        bridge.submit(input);

        return received;
    }
};

} // namespace heg
