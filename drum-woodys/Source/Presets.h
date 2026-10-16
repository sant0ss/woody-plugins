#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <vector>
#include "Slots.h"

namespace woody
{
// A built-in kit: pieces from one main kit (some slots may borrow from another)
// plus mixer/effect settings on top of the defaults.
struct FactoryPreset
{
    const char* name;
    const char* mainKit;
    std::vector<std::pair<int, const char*>> slotKits;   // slot -> kit it borrows from
    std::vector<std::pair<const char*, float>> params;   // parameter id -> value
};

const std::vector<FactoryPreset>& factoryPresets();

// Kits the user saved: a "PRESET" tree with the name, a "KIT" child (slot -> instrument id)
// and a "PARAMS" child (parameter id -> value). Shared by every instance of the plugin.
class UserPresets
{
public:
    static std::vector<juce::ValueTree> load();
    static void save (const std::vector<juce::ValueTree>&);

private:
    static juce::File file();
};
} // namespace woody
