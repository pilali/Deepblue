#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

// Provisional editor: a generic parameter panel. A custom underwater UI (and
// the MOD modgui) is scheduled for a later step; for now this exposes every
// parameter so the DSP can be auditioned in any host.
class DeepblueEditor : public juce::AudioProcessorEditor
{
public:
    explicit DeepblueEditor(DeepblueAudioProcessor&);
    void resized() override;

private:
    DeepblueAudioProcessor& proc;
    juce::GenericAudioProcessorEditor generic;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DeepblueEditor)
};
