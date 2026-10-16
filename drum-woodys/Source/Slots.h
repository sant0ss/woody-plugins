#pragma once

#include <array>

namespace woody
{
// Kinds of kit piece. A slot only accepts instruments of its own type.
enum SlotType
{
    kickType,
    snareType,
    hihatType,
    tomType,
    crashType,
    rideType,
    fxType,
    percType
};

// The fixed positions of the kit, like the pieces of a real drum set.
enum Slot
{
    kick = 0,
    snare,
    hihat,
    tom1,
    tom2,
    floorTom,
    crash1,
    crash2,
    ride,
    fxCymbal,
    perc1,
    perc2,
    perc3,
    perc4,
    numSlots
};

constexpr int maxArticulations = 4;

struct ArticulationDef
{
    const char* name = nullptr;
    int note = -1;
    bool choke = false; // stops the ringing of its own slot
};

struct SlotDef
{
    const char* id;
    const char* name;
    SlotType type;
    int numArticulations;
    std::array<ArticulationDef, maxArticulations> articulations;
    float defaultPan;
    float defaultRoom;
};

// MIDI map: General MIDI for the main hits, spare low notes for chokes and extras.
inline const std::array<SlotDef, numSlots>& slotDefs()
{
    static const std::array<SlotDef, numSlots> defs {{
        { "kick",   "Kick",      kickType,  1, {{ { "Hit", 36 } }},                                                        0.00f, 0.10f },
        { "snare",  "Snare",     snareType, 3, {{ { "Hit", 38 }, { "Rim", 40 }, { "Side Stick", 37 } }},                  -0.10f, 0.30f },
        { "hihat",  "Hi-Hat",    hihatType, 4, {{ { "Closed", 42 }, { "Pedal", 44 }, { "Open", 46 }, { "Swish", 26 } }},  -0.35f, 0.15f },
        { "tom1",   "Tom 1",     tomType,   2, {{ { "Hit", 48 }, { "Rim", 50 } }},                                        -0.15f, 0.30f },
        { "tom2",   "Tom 2",     tomType,   2, {{ { "Hit", 45 }, { "Rim", 47 } }},                                         0.10f, 0.30f },
        { "floor",  "Floor Tom", tomType,   2, {{ { "Hit", 41 }, { "Rim", 43 } }},                                         0.40f, 0.30f },
        { "crash1", "Crash 1",   crashType, 2, {{ { "Hit", 49 }, { "Choke", 28, true } }},                                -0.45f, 0.20f },
        { "crash2", "Crash 2",   crashType, 2, {{ { "Hit", 57 }, { "Choke", 29, true } }},                                 0.45f, 0.20f },
        { "ride",   "Ride",      rideType,  4, {{ { "Tip", 51 }, { "Bell", 53 }, { "Edge", 59 }, { "Choke", 30, true } }}, 0.35f, 0.15f },
        { "fx",     "FX Cymbal", fxType,    2, {{ { "Hit", 52 }, { "Choke", 31, true } }},                                 0.55f, 0.20f },
        { "perc1",  "Perc 1",    percType,  1, {{ { "Hit", 39 } }},                                                        0.00f, 0.25f },
        { "perc2",  "Perc 2",    percType,  1, {{ { "Hit", 54 } }},                                                       -0.50f, 0.25f },
        { "perc3",  "Perc 3",    percType,  1, {{ { "Hit", 56 } }},                                                        0.30f, 0.25f },
        { "perc4",  "Perc 4",    percType,  1, {{ { "Hit", 70 } }},                                                        0.50f, 0.25f },
    }};
    return defs;
}

struct NoteTarget
{
    int slot = -1;
    int articulation = -1;
};

inline const std::array<NoteTarget, 128>& noteMap()
{
    static const auto map = []
    {
        std::array<NoteTarget, 128> m {};
        for (int s = 0; s < numSlots; ++s)
            for (int a = 0; a < slotDefs()[(size_t) s].numArticulations; ++a)
                m[(size_t) slotDefs()[(size_t) s].articulations[(size_t) a].note] = { s, a };
        return m;
    }();
    return map;
}
} // namespace woody
