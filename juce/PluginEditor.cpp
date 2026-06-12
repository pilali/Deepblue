#include "PluginEditor.h"
#include <cmath>

using APVTS = juce::AudioProcessorValueTreeState;

juce::Font deepblue::font(float height, bool bold)
{
    return juce::Font(juce::FontOptions()
                          .withHeight(height)
                          .withStyle(bold ? "Bold" : "Regular"));
}

namespace {
// 270° sweep, from 7:30 to 4:30 — shared by every knob and the value arc.
const float kArcStart = juce::MathConstants<float>::pi * 1.25f;
const float kArcEnd   = juce::MathConstants<float>::pi * 2.75f;
} // namespace

// ════════════════════════════════════════════════════════════════════════════
// AbyssLNF — glassy dark dial lit by a glowing cyan value arc.
// ════════════════════════════════════════════════════════════════════════════
void AbyssLNF::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h,
                                float pos, float startAngle, float endAngle,
                                juce::Slider&)
{
    auto bounds = juce::Rectangle<float>((float) x, (float) y, (float) w, (float) h).reduced(3.0f);
    const float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const float cx = bounds.getCentreX();
    const float cy = bounds.getCentreY();
    auto dial = juce::Rectangle<float>(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);

    // Glass dial body — light catching the top-left, falling into the dark.
    juce::ColourGradient grad(deepblue::kDialHi, cx - radius * 0.5f, cy - radius * 0.65f,
                              deepblue::kDialLo, cx + radius * 0.5f, cy + radius * 0.8f, true);
    g.setGradientFill(grad);
    g.fillEllipse(dial);

    // Rim + a thin glassy highlight near the top.
    g.setColour(juce::Colours::black.withAlpha(0.45f));
    g.drawEllipse(dial.reduced(0.5f), 1.2f);
    g.setColour(deepblue::kWhite.withAlpha(0.10f));
    g.drawEllipse(dial.reduced(2.0f), 1.0f);

    // Value arc.
    const float arcR = radius * 0.80f;
    const float angle = startAngle + pos * (endAngle - startAngle);

    juce::Path track;
    track.addCentredArc(cx, cy, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour(deepblue::kCyanDim.withAlpha(0.30f));
    g.strokePath(track, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path value;
    value.addCentredArc(cx, cy, arcR, arcR, 0.0f, startAngle, angle, true);
    g.setColour(deepblue::kCyan.withAlpha(0.25f));    // glow
    g.strokePath(value, juce::PathStrokeType(6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(deepblue::kCyan);
    g.strokePath(value, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Pointer.
    const juce::Point<float> base(cx + radius * 0.28f * std::sin(angle),
                                  cy - radius * 0.28f * std::cos(angle));
    const juce::Point<float> tip (cx + radius * 0.66f * std::sin(angle),
                                  cy - radius * 0.66f * std::cos(angle));
    g.setColour(deepblue::kWhite.withAlpha(0.85f));
    g.drawLine({ base, tip }, 2.2f);

    // Hub.
    g.setColour(deepblue::kDialLo);
    g.fillEllipse(cx - radius * 0.16f, cy - radius * 0.16f, radius * 0.32f, radius * 0.32f);
    g.setColour(deepblue::kCyan.withAlpha(0.5f));
    g.fillEllipse(cx - radius * 0.05f, cy - radius * 0.05f, radius * 0.10f, radius * 0.10f);
}

// ════════════════════════════════════════════════════════════════════════════
// KnobControl
// ════════════════════════════════════════════════════════════════════════════
KnobControl::KnobControl(APVTS& apvts, const juce::String& paramID,
                         const juce::String& cap, juce::LookAndFeel* lnf)
    : caption(cap)
{
    slider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    slider.setRotaryParameters(kArcStart, kArcEnd, true);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    slider.setLookAndFeel(lnf);
    addAndMakeVisible(slider);
    attachment = std::make_unique<APVTS::SliderAttachment>(apvts, paramID, slider);
}

KnobControl::~KnobControl()
{
    slider.setLookAndFeel(nullptr);
}

void KnobControl::resized()
{
    const int cap = 16;
    const int dial = juce::jmin(getWidth(), getHeight() - cap);
    slider.setBounds((getWidth() - dial) / 2, 0, dial, dial);
}

void KnobControl::paint(juce::Graphics& g)
{
    const float fh = juce::jlimit(9.0f, 14.0f, getWidth() * 0.13f);
    g.setColour(deepblue::kText.withAlpha(0.85f));
    g.setFont(deepblue::font(fh, true));
    g.drawText(caption, getLocalBounds().removeFromBottom(15),
               juce::Justification::centred, false);
}

// ════════════════════════════════════════════════════════════════════════════
// DeepblueEditor
// ════════════════════════════════════════════════════════════════════════════
DeepblueEditor::DeepblueEditor(DeepblueAudioProcessor& p)
    : juce::AudioProcessorEditor(p), proc(p)
{
    depth = std::make_unique<KnobControl>(proc.apvts, "depth", "DEPTH", &lnf);
    addAndMakeVisible(*depth);

    auto add = [&](const char* id, const char* cap) {
        knobs.add(new KnobControl(proc.apvts, id, cap, &lnf));
        addAndMakeVisible(knobs.getLast());
    };
    // Left column (top→bottom): 0,1,2
    add("tone",        "TONE");        // 0
    add("wobble",      "WOBBLE");      // 1
    add("dispersion",  "DISPERSION");  // 2
    // Right column: 3,4,5
    add("mix",         "MIX");         // 3
    add("level",       "OUTPUT");      // 4
    add("immersion",   "IMMERSION");   // 5
    // Bottom row: 6,7,8,9,10
    add("wobble_rate", "RATE");        // 6
    add("reverb",      "REVERB");      // 7
    add("reverb_size", "SIZE");        // 8
    add("bubbles",     "BUBBLES");     // 9
    add("bubble_size", "BUB SIZE");    // 10

    setSize(660, 440);
    startTimerHz(20);
}

DeepblueEditor::~DeepblueEditor()
{
    stopTimer();
}

void DeepblueEditor::timerCallback()
{
    causticPhase += 0.035f;
    if (causticPhase > juce::MathConstants<float>::twoPi * 100.0f)
        causticPhase -= juce::MathConstants<float>::twoPi * 100.0f;
    repaint();
}

void DeepblueEditor::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    const float W = b.getWidth(), H = b.getHeight();

    // ── Abyssal vertical gradient ────────────────────────────────────────────
    juce::ColourGradient grad(deepblue::kAbyssTop, 0.0f, 0.0f,
                              deepblue::kAbyssBot, 0.0f, H, false);
    grad.addColour(0.45, deepblue::kAbyssMid);
    g.setGradientFill(grad);
    g.fillRect(b);

    // ── Drifting caustic light near the surface ──────────────────────────────
    for (int k = 0; k < 4; ++k) {
        juce::Path ribbon;
        const float yBase = 24.0f + k * 26.0f;
        const float amp   = 7.0f + 2.0f * k;
        const float ph    = causticPhase + k * 1.7f;
        ribbon.startNewSubPath(-10.0f, yBase);
        for (float x = -10.0f; x <= W + 10.0f; x += 14.0f)
            ribbon.lineTo(x, yBase + std::sin(x * 0.025f + ph) * amp
                                  + std::sin(x * 0.011f - ph * 0.6f) * amp * 0.5f);
        const float a = 0.07f - k * 0.012f;
        g.setColour(deepblue::kCyan.withAlpha(juce::jmax(0.02f, a)));
        g.strokePath(ribbon, juce::PathStrokeType(10.0f - k * 1.5f,
                     juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Soft top glow (light coming from the surface).
    juce::ColourGradient surf(deepblue::kCyan.withAlpha(0.10f), 0.0f, 0.0f,
                              deepblue::kCyan.withAlpha(0.0f), 0.0f, H * 0.4f, false);
    g.setGradientFill(surf);
    g.fillRect(0.0f, 0.0f, W, H * 0.4f);

    // ── Brand ────────────────────────────────────────────────────────────────
    g.setColour(deepblue::kCyan.withAlpha(0.25f));   // glow
    g.setFont(deepblue::font(27.0f, true));
    g.drawText("DEEPBLUE", juce::Rectangle<int>(27, 15, 360, 32), juce::Justification::left, false);
    g.setColour(deepblue::kCyan);
    g.drawText("DEEPBLUE", juce::Rectangle<int>(26, 14, 360, 32), juce::Justification::left, false);
    g.setColour(deepblue::kText.withAlpha(0.55f));
    g.setFont(deepblue::font(9.5f, false));
    g.drawText("underwater effect", juce::Rectangle<int>(28, 42, 300, 12), juce::Justification::left, false);

    // ── Rising bubbles motif, top-right ──────────────────────────────────────
    struct Bub { float x, y, r, a; };
    const Bub bubs[] = {
        { W - 120, 40, 3.0f, 0.30f }, { W - 95, 24, 2.0f, 0.45f },
        { W - 78,  46, 4.5f, 0.22f }, { W - 56, 30, 2.5f, 0.40f },
        { W - 40,  50, 3.5f, 0.28f }, { W - 22, 22, 1.8f, 0.50f },
    };
    for (auto& bu : bubs) {
        const float dy = std::sin(causticPhase * 0.8f + bu.x * 0.05f) * 3.0f;
        g.setColour(deepblue::kCyan.withAlpha(bu.a));
        g.drawEllipse(bu.x, bu.y + dy, bu.r * 2.0f, bu.r * 2.0f, 1.0f);
    }

    // Faint section captions framing the layout.
    g.setColour(deepblue::kText.withAlpha(0.30f));
    g.setFont(deepblue::font(8.5f, true));
    g.drawText("WATER · MOTION", juce::Rectangle<int>(20, 60, 130, 10), juce::Justification::centred, false);
    g.drawText("SPACE · OUTPUT", juce::Rectangle<int>((int) W - 150, 60, 130, 10), juce::Justification::centred, false);
}

void DeepblueEditor::resized()
{
    const int W = getWidth();

    // Central DEPTH macro.
    depth->setBounds((W - 168) / 2, 78, 168, 184);

    // Side columns (3 each).
    const int colW = 90, colH = 84;
    const int leftX  = 40;
    const int rightX = W - 40 - colW;
    const int ys[3]  = { 78, 168, 258 };

    for (int i = 0; i < 3; ++i) knobs[i]->setBounds(leftX,  ys[i], colW, colH);       // L
    for (int i = 0; i < 3; ++i) knobs[3 + i]->setBounds(rightX, ys[i], colW, colH);   // R

    // Bottom row of 5.
    const int n = 5, bw = 82, bh = 84, y = 346;
    const int span = W - 80;
    const int step = (span - bw) / (n - 1);
    for (int i = 0; i < n; ++i)
        knobs[6 + i]->setBounds(40 + i * step, y, bw, bh);
}
