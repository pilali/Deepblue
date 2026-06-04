// Thin LV2 wrapper around the host-agnostic DSP core (deepblue_dsp.{h,cpp}).
//
// All processing lives in deepblue_dsp.cpp. This file only:
//   - declares the LV2 port indices,
//   - holds port pointers + a DeepblueDsp* instance,
//   - maps the connected control ports into a DeepblueParams on each run().
//
// The plugin is true stereo: two mandatory inputs, two mandatory outputs, and
// nothing optional. This is the most standard, host-robust layout, and it
// matches what a saved MOD pedalboard expects (all four audio ports present, so
// no connection can dangle when the layout changes). A mono source feeding both
// inputs works identically — the core decorrelates the channels into a wide
// underwater image, and the Immersion field / stereo bubbles + reverb run on
// the stereo path. The mono branch below is only a safety net.
#include <lv2/core/lv2.h>
#include <array>
#include <cstdint>
#include <new>

#include "deepblue_dsp.h"

static constexpr char DEEPBLUE_URI[] = "https://github.com/pilali/deepblue";

// ── Port indices ───────────────────────────────────────────────────────────
// True stereo, every port mandatory and contiguous — no optional ports at all,
// the layout hosts (mod-host) are most robust with, and the one a saved MOD
// pedalboard expects (all four audio ports always present, so no connection can
// dangle when the build changes). Audio first, then the 12 controls.
//   0  audio_in      (mandatory in,  left)
//   1  audio_in_r    (mandatory in,  right)
//   2  audio_out     (mandatory out, left)
//   3  audio_out_r   (mandatory out, right)
//   4..15 controls   (contiguous)
enum Port : uint32_t {
    P_AUDIO_IN     =  0,
    P_AUDIO_IN_R   =  1,
    P_AUDIO_OUT    =  2,
    P_AUDIO_OUT_R  =  3,
    P_DEPTH        =  4,   // macro: surface → deep            [0 – 1]
    P_TONE         =  5,   // bright/dark trim                 [0 – 1]
    P_WOBBLE       =  6,   // pitch-wavering depth             [0 – 1]
    P_WOBBLE_RATE  =  7,   // wavering speed Hz                [0.05 – 2]
    P_DISPERSION   =  8,   // allpass smear                    [0 – 1]
    P_MIX          =  9,   // dry/wet                          [0 – 1]
    P_LEVEL        = 10,   // output gain                      [0 – 2]
    P_BUBBLES      = 11,   // Minnaert bubble-stream presence  [0 – 1]
    P_BUBBLE_SIZE  = 12,   // bubble register (small → big)    [0 – 1]
    P_IMMERSION    = 13,   // loss of localisation (stereo)    [0 – 1]
    P_REVERB       = 14,   // dark diffuse reverb amount       [0 – 1]
    P_REVERB_SIZE  = 15,   // reverb decay / size              [0 – 1]
    P_COUNT        = 16
};

// Control input ports stored in the ctl[] array: indices 3..14 (contiguous).
static constexpr uint32_t N_CTL = P_REVERB_SIZE - P_DEPTH + 1;

// ── Plugin instance ────────────────────────────────────────────────────────
struct DeepblueLV2 {
    DeepblueDsp* dsp = nullptr;

    const float* audio_in    = nullptr;   // left input
    const float* audio_in_r  = nullptr;   // right input
    float*       audio_out   = nullptr;   // left output
    float*       audio_out_r = nullptr;   // right output
    std::array<const float*, N_CTL> ctl = {};
};

static inline float ctl(const DeepblueLV2* p, Port port) noexcept {
    const float* ptr = p->ctl[port - P_DEPTH];
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
    else if (port == P_AUDIO_IN_R)
        p->audio_in_r = static_cast<const float*>(data);
    else if (port == P_AUDIO_OUT)
        p->audio_out = static_cast<float*>(data);
    else if (port == P_AUDIO_OUT_R)
        p->audio_out_r = static_cast<float*>(data);
    else if (port >= P_DEPTH && port <= P_REVERB_SIZE)
        p->ctl[port - P_DEPTH] = static_cast<const float*>(data);
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
        ctl(p, P_BUBBLES),
        ctl(p, P_BUBBLE_SIZE),
        ctl(p, P_IMMERSION),
        ctl(p, P_REVERB),
        ctl(p, P_REVERB_SIZE),
    };

    if (p->audio_out_r) {
        // Stereo path. A genuine stereo source uses both inputs; a mono source
        // patched to both inputs (or only the left) still widens, because the
        // core decorrelates the channels. Fall back to the left input if a host
        // somehow left the right input unconnected.
        const float* inR = p->audio_in_r ? p->audio_in_r : p->audio_in;
        deepblue_dsp_process_stereo(p->dsp, &params, p->audio_in, inR,
                                    p->audio_out, p->audio_out_r, n_samples);
    } else {
        // Safety net only — a conformant host always connects the mandatory
        // right output, so this branch should never run.
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
