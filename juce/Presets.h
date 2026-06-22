#pragma once

#include <array>

// ── Factory presets ───────────────────────────────────────────────────────────
// Shared with the LV2 bundle: every entry here has a matching pset:Preset .ttl in
// deepblue.lv2/ with identical port values, so the factory bank is the same in
// every format. Values are the *raw* (denormalised) parameter values, in the
// fixed symbol order below; setCurrentProgram() converts them to 0..1 per the
// parameter's NormalisableRange (so the skewed wobble_rate maps correctly).
//
//   depth, tone, wobble, wobble_rate, dispersion, mix, level,
//   bubbles, bubble_size, immersion, reverb, reverb_size
//
// Ordered shallow/dry → deep/wet.
namespace deepblue {

struct FactoryPreset {
    const char* name;
    std::array<float, 12> values;   // raw values, symbol order above
};

// The 12 parameter symbols, in the same order as FactoryPreset::values.
static constexpr std::array<const char*, 12> kPresetSymbols = {
    "depth", "tone", "wobble", "wobble_rate", "dispersion", "mix", "level",
    "bubbles", "bubble_size", "immersion", "reverb", "reverb_size"
};

static constexpr std::array<FactoryPreset, 15> kFactoryPresets = {{
    { "Surface Tension", { 0.15f, 0.72f, 0.18f, 0.6f, 0.15f, 0.4f, 1.0f, 0.3f, 0.12f, 0.2f, 0.18f, 0.3f } },
    { "Shallows", { 0.25f, 0.6f, 0.25f, 0.4f, 0.2f, 0.45f, 1.0f, 0.12f, 0.3f, 0.25f, 0.2f, 0.4f } },
    { "Geyser", { 0.3f, 0.7f, 0.2f, 0.8f, 0.25f, 0.55f, 1.0f, 0.9f, 0.15f, 0.3f, 0.3f, 0.4f } },
    { "Champagne", { 0.4f, 0.7f, 0.2f, 0.6f, 0.3f, 0.6f, 1.0f, 0.7f, 0.18f, 0.4f, 0.3f, 0.5f } },
    { "Tide Pool", { 0.35f, 0.55f, 0.3f, 0.45f, 0.3f, 0.5f, 1.0f, 0.4f, 0.4f, 0.35f, 0.3f, 0.45f } },
    { "Diver's Bubbles", { 0.45f, 0.55f, 0.25f, 0.5f, 0.3f, 0.6f, 1.0f, 0.8f, 0.55f, 0.4f, 0.35f, 0.5f } },
    { "Riptide", { 0.5f, 0.55f, 0.8f, 1.2f, 0.7f, 0.5f, 1.0f, 0.3f, 0.35f, 0.45f, 0.35f, 0.5f } },
    { "Kelp Forest", { 0.55f, 0.45f, 0.6f, 0.5f, 0.6f, 0.6f, 1.0f, 0.3f, 0.5f, 0.6f, 0.4f, 0.55f } },
    { "Deep Current", { 0.65f, 0.45f, 0.5f, 0.25f, 0.55f, 0.65f, 1.0f, 0.15f, 0.5f, 0.55f, 0.35f, 0.6f } },
    { "Sonar Ping", { 0.6f, 0.4f, 0.25f, 0.15f, 0.4f, 0.65f, 1.0f, 0.35f, 0.8f, 0.5f, 0.6f, 0.8f } },
    { "Glacial Drift", { 0.7f, 0.35f, 0.4f, 0.08f, 0.55f, 0.75f, 1.0f, 0.15f, 0.6f, 0.7f, 0.65f, 0.85f } },
    { "Submerged Cathedral", { 0.8f, 0.4f, 0.35f, 0.18f, 0.6f, 0.75f, 1.0f, 0.2f, 0.6f, 0.7f, 0.85f, 0.95f } },
    { "Cathedral of Whales", { 0.85f, 0.3f, 0.45f, 0.12f, 0.65f, 0.9f, 0.9f, 0.5f, 0.9f, 0.85f, 0.8f, 1.0f } },
    { "Abyss", { 1.0f, 0.3f, 0.4f, 0.12f, 0.7f, 0.8f, 1.0f, 0.1f, 0.7f, 0.8f, 0.6f, 0.9f } },
    { "Mariana", { 1.0f, 0.2f, 0.5f, 0.1f, 0.8f, 0.85f, 0.9f, 0.3f, 0.85f, 0.9f, 0.7f, 0.95f } },
}};

} // namespace deepblue
