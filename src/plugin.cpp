// Thin LV2 wrapper around the host-agnostic DSP core (deepblue_dsp.{h,cpp}).
//
// All processing lives in deepblue_dsp.cpp. This file only:
//   - declares the LV2 port indices,
//   - holds port pointers + a DeepblueDsp* instance,
//   - maps the connected control ports into a DeepblueParams on each run().
//
// The plugin is mono-in → stereo-out: one mandatory input, two mandatory
// outputs. The stereo path always runs (the Immersion field and the stereo
// spread of the bubbles/reverb need two channels to exist); the core
// decorrelates the single mono input into a wide stereo image. There are NO
// optional ports — every port is mandatory and contiguous, the layout some
// hosts (mod-host) are most robust with. The mono-out branch below is only a
// safety net for a host that somehow ignores the right output.
#include <lv2/core/lv2.h>
#include <array>
#include <cstdint>
#include <new>

#include "deepblue_dsp.h"

static constexpr char DEEPBLUE_URI[] = "https://github.com/pilali/deepblue";

// ── Port indices ───────────────────────────────────────────────────────────
// Every port is mandatory and contiguous — no optional ports at all, the layout
// hosts (mod-host) are most robust with. Audio first, then the 12 controls.
//   0  audio_in      (mandatory)
//   1  audio_out     (mandatory)
//   2  audio_out_r   (mandatory — mono-in → stereo-out)
//   3..14 controls   (contiguous)
enum Port : uint32_t {
    P_AUDIO_IN     =  0,
    P_AUDIO_OUT    =  1,
    P_AUDIO_OUT_R  =  2,   // mandatory right output — always stereo out
    P_DEPTH        =  3,   // macro: surface → deep            [0 – 1]
    P_TONE         =  4,   // bright/dark trim                 [0 – 1]
    P_WOBBLE       =  5,   // pitch-wavering depth             [0 – 1]
    P_WOBBLE_RATE  =  6,   // wavering speed Hz                [0.05 – 2]
    P_DISPERSION   =  7,   // allpass smear                    [0 – 1]
    P_MIX          =  8,   // dry/wet                          [0 – 1]
    P_LEVEL        =  9,   // output gain                      [0 – 2]
    P_BUBBLES      = 10,   // Minnaert bubble-stream presence  [0 – 1]
    P_BUBBLE_SIZE  = 11,   // bubble register (small → big)    [0 – 1]
    P_IMMERSION    = 12,   // loss of localisation (stereo)    [0 – 1]
    P_REVERB       = 13,   // dark diffuse reverb amount       [0 – 1]
    P_REVERB_SIZE  = 14,   // reverb decay / size              [0 – 1]
    P_COUNT        = 15
};

// Control input ports stored in the ctl[] array: indices 3..14 (contiguous).
static constexpr uint32_t N_CTL = P_REVERB_SIZE - P_DEPTH + 1;

// ── Plugin instance ────────────────────────────────────────────────────────
struct DeepblueLV2 {
    DeepblueDsp* dsp = nullptr;

    const float* audio_in    = nullptr;
    float*       audio_out   = nullptr;   // left / mono output
    float*       audio_out_r = nullptr;   // right output (mandatory)
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
        // Stereo path: the single mono input feeds both channels and the core
        // decorrelates them into a wide stereo image.
        deepblue_dsp_process_stereo(p->dsp, &params, p->audio_in, p->audio_in,
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
