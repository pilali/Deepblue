#ifndef DEEPBLUE_DSP_H
#define DEEPBLUE_DSP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {           /* indispensable : JUCE est du C++ */
#endif

/* One value per control port, copied from LV2 ports / JUCE parameters.
   Raw values are accepted: the clamp happens inside the process functions,
   exactly as the LV2 run() does. Field names mirror the .ttl symbols, and the
   field ORDER must match the LV2 control-port indices 3..14 (the wrapper builds
   this struct positionally from ctl(P_DEPTH)..ctl(P_REVERB_SIZE)). */
typedef struct {
    float depth;        /* idx 3   [0 – 1]   macro: surface → deep water     */
    float tone;         /* idx 4   [0 – 1]   bright/dark trim around depth    */
    float wobble;       /* idx 5   [0 – 1]   pitch-wavering depth             */
    float wobble_rate;  /* idx 6   [0.05 – 2] Hz   wavering speed             */
    float dispersion;   /* idx 7   [0 – 1]   all-pass frequency smear         */
    float mix;          /* idx 8   [0 – 1]   dry/wet                          */
    float level;        /* idx 9   [0 – 2]   output gain                      */
    float bubbles;      /* idx 10  [0 – 1]   Minnaert bubble-stream presence  */
    float bubble_size;  /* idx 11  [0 – 1]   bubble register (small → big)    */
    float immersion;    /* idx 12  [0 – 1]   loss of localisation (stereo)    */
    float reverb;       /* idx 13  [0 – 1]   dark diffuse reverb amount       */
    float reverb_size;  /* idx 14  [0 – 1]   reverb decay / size of the water */
} DeepblueParams;

typedef struct DeepblueDsp DeepblueDsp;      /* opaque state */

DeepblueDsp* deepblue_dsp_new(double sample_rate);
void         deepblue_dsp_free(DeepblueDsp*);
void         deepblue_dsp_reset(DeepblueDsp*);   /* = activate() : clears filters/state */

/* Process n mono samples. in may equal out (in-place is fine). */
void         deepblue_dsp_process(DeepblueDsp*, const DeepblueParams*,
                                  const float* in, float* out, uint32_t n);

/* Process n samples of true stereo. The two channels run independent state
   (anti-phase wobble LFO, decorrelated random drift, slightly spread
   dispersion break frequencies) so the underwater image gains natural width.
   Feed the same pointer to inL and inR for a mono source → decorrelated
   stereo. In-place is fine (read happens before write per sample). */
void         deepblue_dsp_process_stereo(DeepblueDsp*, const DeepblueParams*,
                                         const float* inL, const float* inR,
                                         float* outL, float* outR, uint32_t n);

#ifdef __cplusplus
}
#endif

#endif /* DEEPBLUE_DSP_H */
