#include "PluginProcessor.h"
#include "PluginEditor.h"

using APVTS = juce::AudioProcessorValueTreeState;
using Range = juce::NormalisableRange<float>;
namespace { const int kVer = 1; }

// Parameter IDs are exactly the LV2 .ttl symbols so state is portable.
APVTS::ParameterLayout DeepblueAudioProcessor::createLayout()
{
    using AF = juce::AudioParameterFloat;
    using FA = juce::AudioParameterFloatAttributes;

    APVTS::ParameterLayout p;
    auto pid = [](const char* s){ return juce::ParameterID { s, kVer }; };

    // wobble_rate is logarithmic in the .ttl — skew so the centre sits low.
    Range rateRange(0.05f, 2.0f);
    rateRange.setSkewForCentre(0.4f);

    p.add(std::make_unique<AF>(pid("depth"),       "Depth",       Range(0.0f, 1.0f), 0.5f));
    p.add(std::make_unique<AF>(pid("tone"),        "Tone",        Range(0.0f, 1.0f), 0.5f));
    p.add(std::make_unique<AF>(pid("wobble"),      "Wobble",      Range(0.0f, 1.0f), 0.35f));
    p.add(std::make_unique<AF>(pid("wobble_rate"), "Wobble Rate", rateRange, 0.3f, FA{}.withLabel("Hz")));
    p.add(std::make_unique<AF>(pid("dispersion"),  "Dispersion",  Range(0.0f, 1.0f), 0.4f));
    p.add(std::make_unique<AF>(pid("mix"),         "Mix",         Range(0.0f, 1.0f), 0.6f));
    p.add(std::make_unique<AF>(pid("level"),       "Output",      Range(0.0f, 2.0f), 1.0f));
    p.add(std::make_unique<AF>(pid("bubbles"),     "Bubbles",     Range(0.0f, 1.0f), 0.0f));
    p.add(std::make_unique<AF>(pid("bubble_size"), "Bubble Size", Range(0.0f, 1.0f), 0.4f));
    p.add(std::make_unique<AF>(pid("immersion"),   "Immersion",   Range(0.0f, 1.0f), 0.4f));
    p.add(std::make_unique<AF>(pid("reverb"),      "Reverb",      Range(0.0f, 1.0f), 0.25f));
    p.add(std::make_unique<AF>(pid("reverb_size"), "Reverb Size", Range(0.0f, 1.0f), 0.5f));

    return p;
}

DeepblueAudioProcessor::DeepblueAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMS", createLayout())
{
    auto raw = [this](const char* id){ return apvts.getRawParameterValue(id); };
    pDepth      = raw("depth");
    pTone       = raw("tone");
    pWobble     = raw("wobble");
    pWobbleRate = raw("wobble_rate");
    pDispersion = raw("dispersion");
    pMix        = raw("mix");
    pLevel      = raw("level");
    pBubbles    = raw("bubbles");
    pBubbleSize = raw("bubble_size");
    pImmersion  = raw("immersion");
    pReverb     = raw("reverb");
    pReverbSize = raw("reverb_size");
}

DeepblueAudioProcessor::~DeepblueAudioProcessor()
{
    if (dsp) deepblue_dsp_free(dsp);
}

void DeepblueAudioProcessor::prepareToPlay(double sampleRate, int)
{
    // (Re)create the DSP if the sample rate changed — allocation happens here,
    // off the audio thread.
    if (dsp == nullptr || sampleRate != currentSampleRate) {
        if (dsp) deepblue_dsp_free(dsp);
        dsp = deepblue_dsp_new(sampleRate);
        currentSampleRate = sampleRate;
    }
    if (dsp) deepblue_dsp_reset(dsp);
}

bool DeepblueAudioProcessor::isBusesLayoutSupported(const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    const auto in = l.getMainInputChannelSet();
    return in == juce::AudioChannelSet::mono()
        || in == juce::AudioChannelSet::stereo()
        || in.isDisabled();
}

void DeepblueAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (dsp == nullptr || n == 0) return;

    const DeepblueParams p {
        pDepth->load(), pTone->load(), pWobble->load(), pWobbleRate->load(),
        pDispersion->load(), pMix->load(), pLevel->load(),
        pBubbles->load(), pBubbleSize->load(), pImmersion->load(),
        pReverb->load(), pReverbSize->load()
    };

    const int numIn  = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();

    if (numOut >= 2) {
        // Stereo out. In-place is safe (the core reads each sample before
        // writing it). For a mono source, copy ch0 into a scratch so the
        // right input is distinct and the core decorrelates the two.
        float* outL = buffer.getWritePointer(0);
        float* outR = buffer.getWritePointer(1);
        const float* inL = buffer.getReadPointer(0);   // channel 0 always exists
        const float* inR;
        if (numIn >= 2) {
            inR = buffer.getReadPointer(1);
        } else {
            rightScratch.setSize(1, n, false, false, true);
            float* r = rightScratch.getWritePointer(0);
            if (numIn > 0) juce::FloatVectorOperations::copy(r, buffer.getReadPointer(0), n);
            else           juce::FloatVectorOperations::clear(r, n);
            inR = r;
        }
        deepblue_dsp_process_stereo(dsp, &p, inL, inR, outL, outR, (uint32_t) n);
        for (int ch = 2; ch < numOut; ++ch)
            juce::FloatVectorOperations::copy(buffer.getWritePointer(ch), outL, n);
    } else {
        // Mono out (mono guitar pedal path).
        float* m = buffer.getWritePointer(0);
        deepblue_dsp_process(dsp, &p, m, m, (uint32_t) n);
    }
}

juce::AudioProcessorEditor* DeepblueAudioProcessor::createEditor()
{
    return new DeepblueEditor(*this);
}

void DeepblueAudioProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary(*xml, dest);
}

void DeepblueAudioProcessor::setStateInformation(const void* data, int size)
{
    if (auto xml = getXmlFromBinary(data, size))
        if (xml->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

// Mandatory entry point.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DeepblueAudioProcessor();
}
