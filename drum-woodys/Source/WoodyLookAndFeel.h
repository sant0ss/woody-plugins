#pragma once

#include "WoodyTheme.h"
#include "Slots.h"

namespace woody
{
// Each kind of piece has its own colour in the kit map, pads and mixer.
inline juce::Colour typeColour (SlotType type)
{
    switch (type)
    {
        case kickType:  return juce::Colour (0xffff8a3d);
        case snareType: return juce::Colour (0xffffd23d);
        case hihatType: return juce::Colour (0xff3dd6c6);
        case tomType:   return juce::Colour (0xff8a7dff);
        case crashType: return juce::Colour (0xff4da3ff);
        case rideType:  return juce::Colour (0xff5fe39a);
        case fxType:    return juce::Colour (0xffff5ea8);
        case percType:  break;
    }
    return juce::Colour (0xffc9a7ff);
}

inline juce::Colour slotColour (int slot)
{
    return typeColour (slotDefs()[(size_t) slot].type);
}
} // namespace woody
