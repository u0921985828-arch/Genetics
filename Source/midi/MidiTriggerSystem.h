#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "../params/Parameters.h"

namespace chrona::midi
{
    // ========================================================================
    //  MidiTriggerSystem — decides, per sample, whether the temporal effect is
    //  "engaged". Three behaviours mirror hardware performance pads:
    //
    //    Hold       — engaged only while the trigger note is physically held.
    //    Latch      — first press engages, next press disengages (toggle).
    //    Momentary  — like Hold but with a minimum engage time so a quick tap
    //                  still produces an audible gesture.
    //
    //  When no trigger note is configured / no MIDI arrives, the effect is
    //  considered always-engaged so the plugin works with zero MIDI setup
    //  (the <5s UX goal).
    // ========================================================================
    class MidiTriggerSystem
    {
    public:
        void prepare (double sampleRate) { sr = sampleRate; setMinMomentaryMs (100.0); reset(); }

        void reset()
        {
            noteHeld = false;
            latched  = false;
            momentaryCounter = 0;
            sawAnyNote = false;
        }

        void setTriggerNote (int n) { triggerNote = n; }
        void setMode (params::TriggerMode m) { mode = m; }
        void setMinMomentaryMs (double ms) { minMomentary = std::max (0, (int) (ms * 0.001 * sr)); }

        // Process a single MIDI message (called in event order alongside audio).
        void handleMessage (const juce::MidiMessage& m)
        {
            if (m.isNoteOn() && m.getNoteNumber() == triggerNote)
            {
                sawAnyNote = true;
                noteHeld = true;
                if (mode == params::TriggerMode::Latch)
                    latched = ! latched;
                if (mode == params::TriggerMode::Momentary)
                    momentaryCounter = minMomentary;
            }
            else if (m.isNoteOff() && m.getNoteNumber() == triggerNote)
            {
                noteHeld = false;
            }
            else if (m.isAllNotesOff() || m.isAllSoundOff())
            {
                // Hosts often send CC 123/120 on stop/panic instead of an
                // explicit note-off — clear the hold so the effect can't stick.
                noteHeld = false;
            }
        }

        // Advance internal timers (call once per sample).
        inline void tick()
        {
            if (momentaryCounter > 0) --momentaryCounter;
        }

        // Is the effect engaged right now?
        inline bool engaged() const
        {
            if (! sawAnyNote)
                return true; // no MIDI setup → always on

            switch (mode)
            {
                case params::TriggerMode::Hold:      return noteHeld;
                case params::TriggerMode::Latch:     return latched;
                case params::TriggerMode::Momentary: return noteHeld || momentaryCounter > 0;
            }
            return true;
        }

        bool hasReceivedMidi() const { return sawAnyNote; }

    private:
        double sr = 44100.0;
        int    triggerNote = 60;
        params::TriggerMode mode = params::TriggerMode::Hold;

        bool noteHeld = false;
        bool latched  = false;
        bool sawAnyNote = false;
        int  momentaryCounter = 0;
        int  minMomentary = 4410; // ~100ms
    };
}
