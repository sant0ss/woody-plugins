#include "Presets.h"

namespace woody
{
const std::vector<FactoryPreset>& factoryPresets()
{
    static const std::vector<FactoryPreset> presets {
        { "Black Pearl - Rock", "Black Pearl", {}, {} },

        { "Red Zeppelin - Classic Rock", "Red Zeppelin", {},
          { { "roomLevel", -4.0f }, { "roomSize", 0.6f }, { "roomComp", 0.5f }, { "glue", 0.3f },
            { "kick_eqlow", 3.0f }, { "snare_comp", 0.3f } } },

        { "Blonde Bop - Jazz Sala", "Blonde Bop", {},
          { { "ohLevel", -2.0f }, { "roomLevel", -6.0f }, { "roomSize", 0.5f }, { "roomComp", 0.1f },
            { "glue", 0.1f }, { "kick_level", -4.0f }, { "humanize", 0.5f } } },

        { "Blonde Bop HR - Jazz Seco", "Blonde Bop HR", {},
          { { "ohLevel", -6.0f }, { "roomLevel", -18.0f }, { "glue", 0.1f } } },

        { "Funk Seco", "Black Pearl", {},
          { { "roomLevel", -20.0f }, { "roomSize", 0.2f }, { "glue", 0.35f },
            { "kick_comp", 0.4f }, { "kick_attack", 0.3f },
            { "snare_comp", 0.45f }, { "snare_attack", 0.4f }, { "hihat_eqhigh", 2.0f } } },

        { "Metal Moderno", "Red Zeppelin", { { snare, "Black Pearl" } },
          { { "kick_eqlow", 4.0f }, { "kick_eqmid", -5.0f }, { "kick_eqfreq", 400.0f }, { "kick_eqhigh", 6.0f },
            { "kick_attack", 0.6f }, { "kick_comp", 0.5f },
            { "snare_comp", 0.5f }, { "snare_drive", 0.3f }, { "snare_eqlow", 2.0f },
            { "tom1_comp", 0.3f }, { "tom2_comp", 0.3f }, { "floor_comp", 0.3f },
            { "glue", 0.45f }, { "roomLevel", -10.0f } } },

        { "Lo-fi", "Black Pearl", {},
          { { "kick_drive", 0.5f }, { "snare_drive", 0.5f }, { "hihat_eqhigh", -6.0f }, { "ohLevel", -10.0f },
            { "roomComp", 0.8f }, { "roomLevel", -6.0f }, { "glue", 0.5f } } },
    };
    return presets;
}

juce::File UserPresets::file()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("Woody Plugins")
        .getChildFile ("Drum Woodys Kits.xml");
}

std::vector<juce::ValueTree> UserPresets::load()
{
    std::vector<juce::ValueTree> presets;
    if (auto xml = juce::XmlDocument::parse (file()))
        for (const auto& child : juce::ValueTree::fromXml (*xml))
            presets.push_back (child.createCopy());
    return presets;
}

void UserPresets::save (const std::vector<juce::ValueTree>& presets)
{
    juce::ValueTree tree ("USER_KITS");
    for (const auto& p : presets)
        tree.appendChild (p.createCopy(), nullptr);

    auto f = file();
    f.getParentDirectory().createDirectory();
    if (auto xml = tree.createXml())
        xml->writeTo (f);
}
} // namespace woody
