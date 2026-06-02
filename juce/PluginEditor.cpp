#include "PluginEditor.h"

DeepblueEditor::DeepblueEditor(DeepblueAudioProcessor& p)
    : juce::AudioProcessorEditor(&p), proc(p), generic(p)
{
    addAndMakeVisible(generic);
    setResizable(true, true);
    setSize(420, 280);
}

void DeepblueEditor::resized()
{
    generic.setBounds(getLocalBounds());
}
