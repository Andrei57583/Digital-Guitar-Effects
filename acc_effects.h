#ifndef ACC_EFFECTS_H
#define ACC_EFFECTS_H

#ifndef M_PI
    #define M_PI 3.14159265358979323846    
#endif

#define LUT_SIZE 1024

#include <arm_neon.h>
#include <math.h>
#include <stdlib.h>
#include <stdint.h>

/*
    Soft clip vectorizat cu Neon
    Calculeaza: x - (x^3 / 3) pentru 4 esantioane simultan
*/
static inline float32x4_t soft_clip_neon(float32x4_t x) {

    const float32x4_t const_1 = vdupq_n_f32(1.0f);
    const float32x4_t const_neg_1 = vdupq_n_f32(-1.0f);
    const float32x4_t limit_pos = vdupq_n_f32(0.6667f);
    const float32x4_t limit_neg = vdupq_n_f32(-0.6667f);
    const float32x4_t third = vdupq_n_f32(0.33333333f);

    // x^3
    float32x4_t x2 = vmulq_f32(x, x);
    float32x4_t x3 = vmulq_f32(x2, x);

    // f(x) = x - (x^3 * 1/3)
    float32x4_t res = vmlsq_f32(x,x3,third);

    // Limite
    uint32x4_t ut = vcgtq_f32(x, const_1); // upper threshold
    uint32x4_t lt = vcltq_f32(x, const_neg_1); // lower threshold

    // Rezultat
    res = vbslq_f32(ut, limit_pos, res);
    res = vbslq_f32(lt, limit_neg, res);

    return res;
}

static inline float32x4_t hard_clip_neon(float32x4_t x, float32x4_t v_threshold) {

    // Prag negativ
    float32x4_t v_neg_threshold = vnegq_f32(v_threshold);

    // Folosim functii ce mapeaza direct la instructiunile hardware FMIN si FMAX
    // x = min(x, threshold)
    float32x4_t res = vminq_f32(x, v_threshold);

    // x = max(x, threshold)
    res = vmaxq_f32(x, v_neg_threshold);

    return res;
}

////////Definirea efectului Chorus
typedef struct
{
    /* data */
    float *delay_buffer;
    float *sin_lut;
    uint32_t buffer_size;
    uint32_t write_pos;
    float lfo_phase;
    float sample_rate;
} AccChorusEffect;

//Initializare Chorus
static inline AccChorusEffect* init_chorus_acc(float sample_rate, float max_delay_ms) {
    AccChorusEffect *c = (AccChorusEffect*)malloc(sizeof(AccChorusEffect));
    c->sample_rate = sample_rate;
    c->buffer_size = (uint32_t)(max_delay_ms * sample_rate / 1000.0f);
    c->delay_buffer = (float*)calloc(c->buffer_size, sizeof(float));
    c->sin_lut = (float*)malloc(LUT_SIZE * sizeof(float));
    for (int i = 0; i < LUT_SIZE; i++) {
        // Se calculeaza o perioada completa de sinus (0 la 2PI)
        c->sin_lut[i] = sinf(i * 2.0f * M_PI / (float)LUT_SIZE);
    }
    c->write_pos = 0;
    c->lfo_phase = 0.0f;
    return c;
}

// Accelerarea chorus prin procesare in blocuri si vectorizare NEON
static inline float process_chorus_block(AccChorusEffect *c, float *input, float *output, uint32_t num_samples,
                                    float depth, float rate, float mix) {

    const float lfo_inc = 2.0f * M_PI * rate / c->sample_rate;
    // Conversie de la faza (0...2PI) la index (0...1023)
    const float phase_to_lut = (float)LUT_SIZE / (2.0f * M_PI);
    const float ms_to_samples = c->sample_rate / 1000.0f;
    const float base_delay = 15.0f * ms_to_samples;
    const float depth_samples = depth * ms_to_samples;

    // Constante NEON pentru mixaj
    float32x4_t v_mix = vdupq_n_f32(mix);
    float32x4_t v_inv_mix = vdupq_n_f32(1.0f - mix);

    for (uint32_t i = 0; i < num_samples; i++) {
        // 1. Calcul Low-Frequency Oscillator (LFO)
        uint32_t lut_idx = (uint32_t)(c->lfo_phase * phase_to_lut) % LUT_SIZE;
        float lfo_val = c->sin_lut[lut_idx];
        
        c->lfo_phase += lfo_inc;
        if (c->lfo_phase > 2.0f * M_PI) {
            c->lfo_phase -= 2.0f * M_PI;    
        }

        // 2. Calcul Pozitie Citire
        float delay_samples = base_delay + depth_samples * lfo_val;
        float read_pos = (float)c->write_pos - delay_samples;

        if (read_pos < 0) {
            read_pos += (float)c->buffer_size;
        }

        // 3. Interpolare liniara
        uint32_t base = (uint32_t)read_pos;
        float frac = read_pos - (float)base;
        uint32_t next = (base + 1 >= c->buffer_size) ? 0 : base + 1;

        float delayed_sample = c->delay_buffer[base] * (1.0f - frac) + c->delay_buffer[next] * frac;

        // 4. Scriem in buffer
        c->delay_buffer[c->write_pos] = input[i];
        c->write_pos = (c->write_pos + 1 == c->buffer_size) ? 0 : c->write_pos + 1;

        // 5. Output scalar
        output[i] = input[i] * (1.0f - mix) + delayed_sample * mix;
    }
}

#endif //ACC_EFFECTS_H