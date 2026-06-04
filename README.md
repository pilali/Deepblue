# Deepblue

An "underwater" effect for guitar — what a sound would feel like heard through
water. The design is grounded in the physics of underwater sound rather than a
generic "muffled reverb": frequency-dependent absorption, the watery pitch
wavering of light/current movement, and the dispersive smear of a shallow-water
waveguide.

Cross-format, host-agnostic DSP core shared by every wrapper:

- **LV2** — primary target: MOD Dwarf, Raspberry Pi 5 (MODEP), desktop Linux.
- **VST3 / AU / Standalone** via JUCE — macOS universal (arm64 + x86_64) & Windows.

Mono and stereo are both supported. On the stereo path a mono source is
decorrelated into a wide image (the eventual "loss of localisation" feature).

## Signal chain

```
in → Absorption → Wobble → Dispersion → [StereoField] → ⊕ bubbles → ⊕ reverb → Dry/Wet → ×Level → out
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

A generative **bubble layer** is summed on top: a Poisson stream of damped,
upward-chirping sinusoids at the Minnaert resonance of a gas bubble in water
(`f₀ = (1/2πa)·√(3γP/ρ)` ≈ 3.28/a Hz at the surface). Depth raises the ambient
pressure, so deeper water shifts every bubble's pitch up (`f₀ ∝ √P`) while the
bed is rolled off to stay coherent with the absorption. Each bubble is panned at
random for a naturally wide stream.

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
| Mix         | 0 – 1      | Dry/wet                                           |
| Level       | 0 – 2      | Output gain                                       |
| Bubbles     | 0 – 1      | Minnaert bubble-stream density / presence         |
| Bubble Size | 0 – 1      | Bubble register: small/fizzy → large/gloopy       |
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

## Tuning knob: dispersion chain length

`DEEPBLUE_DISP_STAGES` sets the number of first-order allpass sections (the most
CPU-hungry part). `DEEPBLUE_BUBBLE_VOICES` sets the bubble-stream polyphony.
Both default to **6** (MOD Dwarf), **12** (native), **16** (Pi 5 / desktop JUCE).

## Status

Steps 1–5 (DSP) implemented: Absorption, Wobble, Depth macro, Dry/Wet,
Dispersion, the Minnaert bubble layer, the StereoField loss-of-localisation
stage, and the dark diffuse reverb (a 4-line FDN). Remaining for step 5:
presets, MOD modgui and a custom JUCE UI. Step 6 finalises the JUCE VST3/AU/
Windows builds.

## License

GPL-3.0-or-later.
