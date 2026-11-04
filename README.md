# Woody Plugins

Audio plugins (VST3) built with [JUCE](https://juce.com).

| Plugin | What it is |
| --- | --- |
| [Drum Woodys](drum-woodys/) | Acoustic drum sampler in the spirit of Addictive Drums: pick each kit piece from any kit, built-in groove library, step editor, song arranger and MIDI drag-and-drop into the DAW. |

| [Synth Woodys](synth-woodys/) | Polyphonic synthesizer that blends an analog-style oscillator with a wavetable oscillator, ladder filter, modulation matrix and effects. |

| [EQ Woodys](eq-woodys/) | 16-band parametric equalizer with zero-latency and linear-phase modes, per-band stereo placement (stereo, L, R, mid, side), spectrum analyzer and per-band saturation. |
| [Sat Woodys](sat-woodys/) | Saturator with five anti-aliased curves, tone, focus band, mid/side, oversampling and auto-gain. |
| [Comp Woodys](comp-woodys/) | Compressor with a single-band mode and a 4-band Linkwitz-Riley multiband mode. |

### EQ Woodys

- **16 bands**, each a bell, low/high shelf, low/high cut (6 to 96 dB/oct), notch, band pass or tilt shelf. Drag nodes on the
  curve, scroll to change Q, double-click to add or remove a band.
- **Zero Latency** mode: cascaded biquads with sliding coefficients (no clicks while moving a band).
- **Linear Phase** mode: the combined response is turned into symmetric FIR filters (inverse FFT + window) and applied by
  partitioned convolution. No phase distortion; latency is half the filter length (Low to Max = 2048 to 16384 taps).
  Mid/side bands are handled exactly by combining the bands as 2x2 matrices of zero-phase responses.
- **Per-band placement**: stereo, left, right, mid or side.
- **Per-band saturation**: only the signal around the band is saturated, with anti-aliased curves (soft, tube, tape, hard, fold).
- Pre/post spectrum analyzer, output gain, bypass.

### Sat Woodys

- Five curves (soft, tube, tape, hard, fold) using first-order antiderivative anti-aliasing (no latency), plus 2x/4x/8x oversampling.
- Input, drive, tone, bias, mix, output, auto-gain; stereo, mid-only or side-only; focus low/high cut so only part of the
  spectrum is saturated while the rest passes untouched. Live transfer curve and waveform view.

### Comp Woodys

- **Single band**: threshold, ratio (up to limit), attack, release (or auto), soft knee, makeup (or auto), parallel mix,
  sidechain high-pass, stereo link, peak/RMS detector, lookahead, stereo or mid/side.
- **Multiband**: 4 bands split by 4th-order Linkwitz-Riley crossovers (adjustable, sum is flat), with threshold, ratio, attack,
  release, makeup, solo and bypass per band.

The DSP of the EQ, saturator and compressor is checked by console test programs (`run-tests.bat`, `run-test-one.bat`).

### Synth Woodys

- **Osc A (analog)** – saw, square/pulse (with pulse width), triangle and sine, band-limited with polyBLEP.
- **Osc B (wavetable)** – 6 generated wavetables (Basic, Digital, Vocal, Organ, Glass, Growl) with 8 key frames each;
  the position control morphs between frames. Tables are band-limited per octave to avoid aliasing.
- **Unison** – up to 7 stacked voices per oscillator with detune and stereo spread, plus sub oscillator and noise.
- **Filter** – ladder filter with drive (low pass 24/12, band pass, high pass), envelope amount and keytracking.
- **Modulation** – two ADSR envelopes (volume, filter), two LFOs (sine, triangle, saw, square, random) and a 4-slot
  matrix (LFO 1/2, filter envelope, velocity, mod wheel to cutoff, resonance, pitch, wavetable position, pulse width,
  osc levels, volume).
- **Effects** – drive, chorus, ping-pong delay, reverb, master level with soft limiter.
- **Presets** – factory sounds plus your own saved sounds. 16 voices, sustain pedal, pitch bend (±2 st), mod wheel.
- Resizable window with on-screen keyboard.

### Drum Woodys

- **Kit** – abstract kit map with 14 slots (kick, snare, hi-hat, 3 toms, 2 crashes, ride, FX cymbal, 4 percussion).
  Each slot takes a piece from any kit in the library, with articulations (snare rim/side stick, hi-hat
  closed/pedal/open/swish, ride bell/edge, cymbal chokes), velocity layers and tuning. Per-slot panel with 3-band EQ,
  compressor, transient shaper (attack/sustain), drive, overhead/room sends and an optional separate output.
- **Kit presets** – factory kits plus your own saved kits (pieces, mixer and effects).
- **Mixer** – level, pan, mute, solo and peak meters per slot; Overhead and Room buses; master glue compressor and
  soft limiter; humanize and variation (keeps repeated hits from sounding identical).
- **Multi-out** – 14 extra stereo outputs, one per slot, for mixing pieces on separate DAW tracks.
- **Grooves** – 50 patterns in 13 styles (rock, pop, funk, shuffle, jazz, bossa nova, samba, baião, reggae, hip hop,
  metal, ballad, disco, funk carioca), filter by complexity, suggested tempo, one-click variations and fills, swing and
  human timing. Preview in sync with the host and drag the MIDI into a track.
- **Editor** – step grid with undo/redo to build or tweak grooves (1/2/4 bars, 16ths, 8ths or triplets), saved to
  "Meus padrões".
- **Música** – chain parts with repeats into a song, play it and drag the whole song, or a single part, as MIDI.
- Resizable window; the layout scales with it.

MIDI map: General MIDI for the main hits (36 kick, 38 snare, 42/44/46 hi-hat, 48/45/41 toms, 49/57 crashes, 51/53/59 ride).

## Build (Windows)

Requirements: Visual Studio 2022 Build Tools (C++ workload) and CMake.

```bash
git submodule update --init
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Output: `build/drum-woodys/DrumWoodys_artefacts/Release/VST3/Drum Woodys.vst3` and
`build/synth-woodys/SynthWoodys_artefacts/Release/VST3/Synth Woodys.vst3`
Each plugin also builds a `Standalone` app for testing without a DAW.

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

## License

Woody Plugins is free software, licensed under the **GNU Affero General Public License v3.0** (see [LICENSE](LICENSE)),
the same license JUCE offers for open-source projects. You can use, study, change and share it under those terms.

- [JUCE](https://juce.com) is included as a submodule and has its own license (AGPLv3 or a commercial JUCE licence).
- The drum kits are **not** part of this repository; each kit keeps its own license (the AVL Drumkits are CC-BY-SA 3.0).
- Compiled plugins distributed to others must come with a link to this source code.
