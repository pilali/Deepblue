#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "deepblue_dsp.h"

// Thin JUCE wrapper around the host-agnostic DSP core (src/deepblue_dsp.{h,cpp}),
// the same core compiled into the LV2 plugin. Parameters use APVTS with IDs
// identical to the LV2 .ttl symbols, so state stays consistent across formats.
class DeepblueAudioProcessor : public juce::AudioProcessor
{
public:
    DeepblueAudioProcessor();
    ~DeepblueAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Deepblue"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    // Factory presets (shared with the LV2 bundle — see juce/Presets.h).
    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram(int) override;
    const juce::String getProgramName(int) override;
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    DeepblueDsp* dsp = nullptr;
    double currentSampleRate = 0.0;
    int currentProgram = 0;
    juce::AudioBuffer<float> rightScratch;   // mono-in → stereo-out right feed

    // Cached raw-parameter atomics (denormalised values, ready for DeepblueParams).
    std::atomic<float>* pDepth      = nullptr;
    std::atomic<float>* pTone       = nullptr;
    std::atomic<float>* pWobble     = nullptr;
    std::atomic<float>* pWobbleRate = nullptr;
    std::atomic<float>* pDispersion = nullptr;
    std::atomic<float>* pMix        = nullptr;
    std::atomic<float>* pLevel      = nullptr;
    std::atomic<float>* pBubbles    = nullptr;
    std::atomic<float>* pBubbleSize = nullptr;
    std::atomic<float>* pImmersion  = nullptr;
    std::atomic<float>* pReverb     = nullptr;
    std::atomic<float>* pReverbSize = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DeepblueAudioProcessor)
};
