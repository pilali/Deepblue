// Thin LV2 wrapper around the host-agnostic DSP core (deepblue_dsp.{h,cpp}).
//
// All processing lives in deepblue_dsp.cpp. This file only:
//   - declares the LV2 port indices,
//   - holds port pointers + a DeepblueDsp* instance,
//   - maps the connected control ports into a DeepblueParams on each run().
//
// Mono/stereo is handled with optional right-side ports (like Megalo): when the
// host connects audio_out_r the stereo path runs, otherwise the mono path. The
// right *input* is also optional — if the host gives us a stereo out but only a
// mono in, the mono signal feeds both channels and is decorrelated by the core.
#include <lv2/core/lv2.h>
#include <array>
#include <cstdint>
#include <new>

#include "deepblue_dsp.h"

static constexpr char DEEPBLUE_URI[] = "https://github.com/pilali/deepblue";

// ── Port indices ───────────────────────────────────────────────────────────
enum Port : uint32_t {
    P_AUDIO_IN     =  0,
    P_AUDIO_OUT    =  1,
    P_DEPTH        =  2,   // macro: surface → deep   [0 – 1]
    P_TONE         =  3,   // bright/dark trim        [0 – 1]
    P_WOBBLE       =  4,   // pitch-wavering depth     [0 – 1]
    P_WOBBLE_RATE  =  5,   // wavering speed Hz        [0.05 – 2]
    P_DISPERSION   =  6,   // allpass smear            [0 – 1]
    P_MIX          =  7,   // dry/wet                  [0 – 1]
    P_LEVEL        =  8,   // output gain              [0 – 2]
    P_AUDIO_IN_R   =  9,   // optional right input  — connectionOptional
    P_AUDIO_OUT_R  = 10,   // optional right output — connected ⇒ stereo path
    P_BUBBLES      = 11,   // Minnaert bubble-stream presence  [0 – 1]
    P_BUBBLE_SIZE  = 12,   // bubble register (small → big)    [0 – 1]
    P_IMMERSION    = 13,   // loss of localisation (stereo)    [0 – 1]
    P_REVERB       = 14,   // dark diffuse reverb amount       [0 – 1]
    P_REVERB_SIZE  = 15,   // reverb decay / size              [0 – 1]
    P_COUNT        = 16
};

// Control input ports stored in the ctl[] array: indices 2..8.
static constexpr uint32_t N_CTL = P_AUDIO_IN_R - 2;

// ── Plugin instance ────────────────────────────────────────────────────────
struct DeepblueLV2 {
    DeepblueDsp* dsp = nullptr;

    const float* audio_in    = nullptr;
    const float* audio_in_r  = nullptr;   // NULL when host runs us mono in
    float*       audio_out   = nullptr;   // left / mono output
    float*       audio_out_r = nullptr;   // NULL when host runs us mono out
    std::array<const float*, N_CTL> ctl = {};

    // Bubble controls live past the optional audio ports (idx 11/12), so they
    // aren't contiguous with ctl[]; keep them as plain pointers.
    const float* p_bubbles     = nullptr;
    const float* p_bubble_size = nullptr;
    const float* p_immersion   = nullptr;
    const float* p_reverb      = nullptr;
    const float* p_reverb_size = nullptr;
};

static inline float opt(const float* ptr, float dflt) noexcept {
    return ptr ? *ptr : dflt;
}

static inline float ctl(const DeepblueLV2* p, Port port) noexcept {
    const float* ptr = p->ctl[port - 2];
    return ptr ? *ptr : 0.0f;
}

// ── LV2 callbacks ──────────────────────────────────────────────────────────
static LV2_Handle instantiate(const LV2_Descriptor*,
                              double rate,
                              const char*,
                              const LV2_Feature* const*)
{
    DeepblueLV2* p = new (std::nothrow) DeepblueLV2();
    if (!p) return nullptr;
    p->dsp = deepblue_dsp_new(rate);
    if (!p->dsp) { delete p; return nullptr; }
    return p;
}

static void connect_port(LV2_Handle handle, uint32_t port, void* data)
{
    DeepblueLV2* p = static_cast<DeepblueLV2*>(handle);
    if (port == P_AUDIO_IN)
        p->audio_in = static_cast<const float*>(data);
    else if (port == P_AUDIO_OUT)
        p->audio_out = static_cast<float*>(data);
    else if (port == P_AUDIO_IN_R)
        p->audio_in_r = static_cast<const float*>(data);
    else if (port == P_AUDIO_OUT_R)
        p->audio_out_r = static_cast<float*>(data);
    else if (port == P_BUBBLES)
        p->p_bubbles = static_cast<const float*>(data);
    else if (port == P_BUBBLE_SIZE)
        p->p_bubble_size = static_cast<const float*>(data);
    else if (port == P_IMMERSION)
        p->p_immersion = static_cast<const float*>(data);
    else if (port == P_REVERB)
        p->p_reverb = static_cast<const float*>(data);
    else if (port == P_REVERB_SIZE)
        p->p_reverb_size = static_cast<const float*>(data);
    else if (port >= 2 && port < P_AUDIO_IN_R)
        p->ctl[port - 2] = static_cast<const float*>(data);
}

static void activate(LV2_Handle handle)
{
    DeepblueLV2* p = static_cast<DeepblueLV2*>(handle);
    deepblue_dsp_reset(p->dsp);
}

static void run(LV2_Handle handle, uint32_t n_samples)
{
    DeepblueLV2* p = static_cast<DeepblueLV2*>(handle);

    const DeepblueParams params {
        ctl(p, P_DEPTH),
        ctl(p, P_TONE),
        ctl(p, P_WOBBLE),
        ctl(p, P_WOBBLE_RATE),
        ctl(p, P_DISPERSION),
        ctl(p, P_MIX),
        ctl(p, P_LEVEL),
        opt(p->p_bubbles,     0.0f),
        opt(p->p_bubble_size, 0.4f),
        opt(p->p_immersion,   0.4f),
        opt(p->p_reverb,      0.25f),
        opt(p->p_reverb_size, 0.5f),
    };

    if (p->audio_out_r) {
        // Stereo path. A mono input (no right port connected) feeds both
        // channels; the core decorrelates them.
        const float* inR = p->audio_in_r ? p->audio_in_r : p->audio_in;
        deepblue_dsp_process_stereo(p->dsp, &params, p->audio_in, inR,
                                    p->audio_out, p->audio_out_r, n_samples);
    } else {
        deepblue_dsp_process(p->dsp, &params, p->audio_in, p->audio_out, n_samples);
    }
}

static void cleanup(LV2_Handle handle)
{
    DeepblueLV2* p = static_cast<DeepblueLV2*>(handle);
    deepblue_dsp_free(p->dsp);
    delete p;
}

static const LV2_Descriptor descriptor = {
    DEEPBLUE_URI, instantiate, connect_port, activate, run, nullptr, cleanup, nullptr
};

LV2_SYMBOL_EXPORT const LV2_Descriptor* lv2_descriptor(uint32_t index)
{
    return (index == 0) ? &descriptor : nullptr;
}
