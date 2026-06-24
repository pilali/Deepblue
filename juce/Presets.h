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
    { "Geyser", { 0.0f, 1.0f, 0.11f, 2.0f, 1.0f, 0.59f, 1.0f, 1.0f, 0.0f, 0.0f, 0.11f, 1.0f } },
    { "Champagne", { 0.78f, 0.61f, 0.04f, 0.6f, 0.3f, 0.5f, 1.5f, 0.7f, 0.04f, 0.13f, 0.68f, 0.09f } },
    { "Tide Pool", { 0.35f, 0.55f, 0.3f, 0.45f, 0.3f, 0.5f, 1.0f, 0.1f, 0.4f, 0.35f, 0.3f, 0.45f } },
    { "Diver's Bubbles", { 0.45f, 0.55f, 0.63f, 0.5f, 0.3f, 0.4f, 1.2f, 0.68f, 0.71f, 0.4f, 0.35f, 0.5f } },
    { "Riptide", { 0.5f, 0.55f, 0.8f, 1.2f, 0.7f, 0.5f, 1.0f, 0.3f, 0.35f, 0.45f, 0.35f, 0.5f } },
    { "Kelp Forest", { 0.28f, 0.00f, 0.77f, 0.4f, 1.0f, 0.66f, 1.25f, 0.17f, 0.47f, 1.0f, 0.7f, 0.55f } },
    { "Deep Current", { 0.82f, 0.58f, 0.72f, 0.51f, 1.0f, 1.0f, 1.2f, 0.9f, 0.87f, 0.73f, 0.86f, 0.74f } },
    { "Sonar Ping", { 0.6f, 0.4f, 0.25f, 0.15f, 0.4f, 0.65f, 1.0f, 0.35f, 0.8f, 0.5f, 0.6f, 0.8f } },
    { "Glacial Drift", { 0.7f, 0.5f, 0.4f, 0.08f, 0.55f, 0.75f, 1.4f, 0.04f, 0.6f, 0.7f, 0.67f, 0.85f } },
    { "Submerged Cathedral", { 0.8f, 0.85f, 0.35f, 0.18f, 0.6f, 0.75f, 1.0f, 0.2f, 0.6f, 0.7f, 0.85f, 0.95f } },
    { "Cathedral of Whales", { 0.43f, 0.79f, 0.96f, 0.19f, 0.52f, 0.8f, 1.0f, 0.57f, 0.93f, 0.72f, 0.86f, 0.72f } },
    { "Abyss", { 0.87f, 0.8f, 1.0f, 0.05f, 1.0f, 1.0f, 1.0f, 0.4f, 1.0f, 0.85f, 1.0f, 1.0f } },
    { "Mariana", { 0.8f, 0.85f, 0.75f, 0.1f, 0.75f, 0.9f, 2.0f, 0.3f, 0.85f, 0.69f, 0.84f, 0.95f } },
}};

} // namespace deepblue
