# Woody Plugins

Audio plugins (VST3) built with [JUCE](https://juce.com).

| Plugin | What it is |
| --- | --- |
| [Drum Woodys](drum-woodys/) | Acoustic drum sampler in the spirit of Addictive Drums: pick each kit piece from any kit, built-in groove library, step editor, song arranger and MIDI drag-and-drop into the DAW. |

### Drum Woodys

- **Kit** – 14 slots (kick, snare, hi-hat, 3 toms, 2 crashes, ride, FX cymbal, 4 percussion). Each slot takes a piece
  from any kit in the library, with articulations (snare rim/side stick, hi-hat closed/pedal/open/swish, ride bell/edge,
  cymbal chokes), velocity layers and tuning. Pads show their MIDI note.
- **Mixer** – level, pan, room send, mute and solo per slot; master, ambience, room size and humanize.
- **Grooves** – 50 patterns in 13 styles (rock, pop, funk, shuffle, jazz, bossa nova, samba, baião, reggae, hip hop,
  metal, ballad, disco, funk carioca). Preview in sync with the host and drag the MIDI into a track.
- **Editor** – step grid to build or tweak grooves (1/2/4 bars, 16ths, 8ths or triplets), saved to "Meus padrões".
- **Música** – chain parts with repeats into a song, play it and drag the whole song as one MIDI clip.

MIDI map: General MIDI for the main hits (36 kick, 38 snare, 42/44/46 hi-hat, 48/45/41 toms, 49/57 crashes, 51/53/59 ride).

## Build (Windows)

Requirements: Visual Studio 2022 Build Tools (C++ workload) and CMake.

```bash
git submodule update --init
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Output: `build/drum-woodys/DrumWoodys_artefacts/Release/VST3/Drum Woodys.vst3`
(and a `Standalone/Drum Woodys.exe` for testing without a DAW).

## Kits

Kits are not stored in git. Put them in `kits/` and pick the `.sfz` file with **Load Kit**.

The default kit is **AVL Drumkits – Black Pearl** by Glen MacArthur
([bandshed.net/avldrumkits](https://www.bandshed.net/avldrumkits/)), CC-BY-SA 3.0.
Music made with it can be released without attribution.

```bash
mkdir -p kits && cd kits
curl -LO https://www.bandshed.net/sounds/AVLDrumkits_SFZ/BLACK_PEARL_2023_repack.zip
unzip BLACK_PEARL_2023_repack.zip && rm BLACK_PEARL_2023_repack.zip
```
