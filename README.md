# Deepblue

[![build](https://github.com/pilali/Deepblue/actions/workflows/build.yml/badge.svg)](https://github.com/pilali/Deepblue/actions/workflows/build.yml)

An "underwater" effect for guitar — what a sound would feel like heard through
water. The design is grounded in the physics of underwater sound rather than a
generic "muffled reverb": frequency-dependent absorption, the watery pitch
wavering of light/current movement, and the dispersive smear of a shallow-water
waveguide.

Cross-format, host-agnostic DSP core shared by every wrapper:

- **LV2** — primary target: MOD Dwarf, Raspberry Pi 5 (MODEP), desktop Linux.
- **VST3 / AU / Standalone** via JUCE — macOS universal (arm64 + x86_64) & Windows.

The plugin is **true stereo** (two inputs, two outputs — every LV2 port is
mandatory and contiguous, the layout hosts are most robust with), so the stereo
features — the Immersion "loss of localisation" field and the stereo spread of
the bubbles and reverb — are always live. A mono guitar patched to both inputs
still widens, because the core decorrelates the channels into a wide image.

## Signal chain

```
in → Absorption → Wobble → Dispersion → [StereoField] → ⊕ bubbles → ⊕ reverb → Dry/Wet → ×Output → out
```

Everything after the per-channel chain — the StereoField, the Minnaert bubble
stream and the dark diffuse reverb — is part of the **wet** signal, so the
single **Mix** knob balances the dry guitar against the whole underwater
treatment. The reverb is fed by the wet signal (bubbles included), so the
bubbles reverberate inside the same body of water.

On the stereo path a **StereoField** stage recreates the underwater *loss of
directional hearing*: because sound travels ~4.4× faster in water (so interaural
time differences shrink below the ear's resolution) and the head barely shadows
it, a submerged listener can't tell where a sound comes from. As **Immersion**
rises it collapses the coherent (directional) Side component toward mono while
substituting a decorrelated diffuse field — the sound becomes non-localisable
yet stays wide and enveloping. (Mono path: no-op — there's no image to lose.)

A generative **bubble layer** rides the wet signal — not as an added sound but
as a stream of *filters*. Each Poisson-emitted bubble is a resonant band-pass
ringing off the full-bandwidth input at the Minnaert resonance of a gas bubble
in water (`f₀ = (1/2πa)·√(3γP/ρ)` ≈ 3.28/a Hz at the surface), with the bubble's
own physical damping as the filter Q, chirping upward as it decays — the watery
"bloop" — so the water bubbles around whatever you play (silence in, silence
out). The resonances are deliberately voiced to **stand out**: lighter damping
(higher Q + a longer ring), more makeup gain, a denser stream and a brighter bed
so each bloop reads as a clear pitched resonance instead of a faint click.
Bubble Size spans 0.4 mm fizz (~8 kHz) down to a very big 40 mm gloop
(~80 Hz). Depth raises the ambient pressure, so deeper water shifts every bubble
up (`f₀ ∝ √P`) while the bed is rolled off to stay coherent with the absorption.
Each bubble is panned at random for a naturally wide stream.

The single **Depth** macro is the physical through-line — deeper water means
more high-frequency loss, more pitch wavering and more dispersion — so it biases
all three at once, with per-control trims on top.

| Control     | Range      | Role                                              |
|-------------|------------|---------------------------------------------------|
| Depth       | 0 – 1      | Surface → deep. Biases cutoff, wobble, dispersion |
| Tone        | 0 – 1      | Bright/dark trim around the depth-derived cutoff  |
| Wobble      | 0 – 1      | Pitch-wavering depth                              |
| Wobble Rate | 0.05 – 2 Hz| Wavering speed                                    |
| Dispersion  | 0 – 1      | Allpass frequency smear                           |
| Mix         | 0 – 1      | Dry/wet balance                                   |
| Output      | 0 – 2      | Global output gain, applied after the mix         |
| Bubbles     | 0 – 1      | Minnaert bubble-stream density / presence         |
| Bubble Size | 0 – 1      | Register: fizzy ~8 kHz → very big gloop ~80 Hz    |
| Immersion   | 0 – 1      | Loss of localisation: collapse + diffuse (stereo) |
| Reverb      | 0 – 1      | Dark diffuse reverb amount (surface/bottom field) |
| Reverb Size | 0 – 1      | Reverb decay / size of the body of water          |

## Building

### LV2 — native (development / desktop Linux)

```sh
make TARGET=native          # → deepblue.lv2/deepblue.so
# or with CMake:
cmake --preset native && cmake --build build/native
```

### LV2 — Raspberry Pi 5 (cross-compile from x86-64)

```sh
make TARGET=rpi5            # requires aarch64-linux-gnu-g++
```

### LV2 — MOD Dwarf

Built via [`mod-plugin-builder`](https://github.com/moddevices/mod-plugin-builder).
Copy `plugins/package/deepblue/` into the builder's package tree and build the
`deepblue` package. A short dispersion chain (`DEEPBLUE_DISP_STAGES=6`) keeps it
within the Cortex-A35 CPU budget; the Pi 5 build uses 16.

### VST3 / AU / Standalone (JUCE)

```sh
cmake -S juce -B juce/build -DCMAKE_BUILD_TYPE=Release
cmake --build juce/build
```

macOS builds are universal (arm64 + x86_64) by default. Pass
`-DLOCAL_JUCE_DIR=/path/to/JUCE` for offline builds.

### Prebuilt binaries (CI)

Every push is built by [GitHub Actions](.github/workflows/build.yml): the LV2
bundle (Linux x86-64 and Raspberry Pi 5 / aarch64) and the JUCE VST3 / AU /
Standalone for Linux, macOS (universal) and Windows. Grab them from the run's
artefacts. The MOD Dwarf package is built separately via `mod-plugin-builder`.

## Tuning knob: dispersion chain length

`DEEPBLUE_DISP_STAGES` sets the number of first-order allpass sections (the most
CPU-hungry part). `DEEPBLUE_BUBBLE_VOICES` sets the bubble-stream polyphony.
Both default to **6** (MOD Dwarf), **12** (native), **16** (Pi 5 / desktop JUCE).
`DEEPBLUE_REVERB_LINES` sets the reverb FDN size (an even count in [2,8]): **4**
on the MOD Dwarf, **8** elsewhere for a denser, deeper tail.

## Status

Steps 1–5 implemented: the full DSP (Absorption, Wobble, Depth macro, Dry/Wet,
Dispersion, the Minnaert bubble layer, the StereoField loss-of-localisation
stage, and the deep dark diffuse modulated-FDN reverb), five LV2 presets (Shallows,
Deep Current, Abyss, Champagne, Submerged Cathedral), a custom "abyssal" JUCE
editor and a matching MOD modgui. Step 6 finalises the JUCE VST3/AU/Windows
builds (run on the target machine — JUCE is fetched at configure time).

The desktop editor and the MOD modgui share one visual language: a deep
navy→black gradient with drifting caustic light, a large central Depth macro
and glassy knobs lit by a luminous cyan value arc.

## License

GPL-3.0-or-later.
