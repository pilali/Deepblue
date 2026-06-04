#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

// Custom "abyssal" UI: a deep navy→black gradient with drifting caustic light,
// a large central DEPTH macro and glassy knobs lit by a luminous cyan value
// arc. Everything is drawn with paths/gradients — no bitmaps — so it stays
// crisp at any size. The same visual language drives the MOD modgui.

namespace deepblue {
    const juce::Colour kAbyssTop { 0xff0b3a5c };   // lit shallows
    const juce::Colour kAbyssMid { 0xff052236 };
    const juce::Colour kAbyssBot { 0xff020a12 };   // abyss floor
    const juce::Colour kCyan     { 0xff48d6e6 };   // luminous value / accent
    const juce::Colour kCyanDim  { 0xff1c6f86 };
    const juce::Colour kDialHi   { 0xff15546f };
    const juce::Colour kDialLo   { 0xff04111d };
    const juce::Colour kText     { 0xffbfe8f0 };   // pale cyan captions
    const juce::Colour kWhite    { 0xffffffff };

    juce::Font font (float height, bool bold = false);
}

// ── Rotary knob look: glassy dark dial + glowing cyan value arc ──────────────
class AbyssLNF : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider(juce::Graphics&, int x, int y, int w, int h,
                          float pos, float startAngle, float endAngle,
                          juce::Slider&) override;
};

// ── A rotary knob + caption below (sizes itself to its bounds) ───────────────
class KnobControl : public juce::Component
{
public:
    KnobControl(juce::AudioProcessorValueTreeState&, const juce::String& paramID,
                const juce::String& caption, juce::LookAndFeel*);
    ~KnobControl() override;
    void resized() override;
    void paint(juce::Graphics&) override;
private:
    juce::Slider slider;
    juce::String caption;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KnobControl)
};

// ── The editor ───────────────────────────────────────────────────────────────
class DeepblueEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit DeepblueEditor(DeepblueAudioProcessor&);
    ~DeepblueEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    void timerCallback() override;

    DeepblueAudioProcessor& proc;
    AbyssLNF lnf;

    std::unique_ptr<KnobControl> depth;          // central macro
    juce::OwnedArray<KnobControl> knobs;         // the rest

    float causticPhase = 0.0f;                   // drifts the light shimmer

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DeepblueEditor)
};
