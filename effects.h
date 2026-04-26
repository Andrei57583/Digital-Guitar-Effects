#ifndef EFFECTS_H
#define EFFECTS_H

#ifndef M_PI
    #define M_PI 3.14159265358979323846    
#endif

#include <math.h>
#include <stdlib.h>
#include <stdint.h>

static inline float soft_clip(float x) {
    if (x > 1.0) return 0.6667f;
    if (x < -1.0f) return -0.6667f;

    // f(x) = x - (x^3 / 3)
    return x - (x * x * x) * 0.3333333f;
}

static inline float hard_clip(float x, float threshold) {
    if (x > threshold) return threshold;
    if (x < -threshold) return -threshold;
    return x;
}

////////Definirea efectului Chorus
typedef struct
{
    /* data */
    float *delay_buffer;
    uint32_t buffer_size;
    uint32_t write_pos;
    float lfo_phase;
    float sample_rate;
} ChorusEffect;

//Initializare Chorus
static inline ChorusEffect* init_chorus(float sample_rate, float max_delay_ms) {
    ChorusEffect *c = (ChorusEffect*)malloc(sizeof(ChorusEffect));
    c->sample_rate = sample_rate;
    c->buffer_size = (uint32_t)(max_delay_ms * sample_rate / 1000.0f);
    c->delay_buffer = (float*)calloc(c->buffer_size, sizeof(float));
    c->write_pos = 0;
    c->lfo_phase = 0.0f;
    return c;
}

//Procesarea unui singur esantion
static inline float process_chorus(ChorusEffect *c, float input, float depth, float rate, float mix) {
    // 1. Se calculeaza intarzierea variabila flosind un LFO (sinus)
    float lfo_val = sinf(c->lfo_phase);
    c->lfo_phase += 2.0f * M_PI * rate / c->sample_rate;
    if (c->lfo_phase > 2.0f * M_PI) {
        c->lfo_phase -= 2.0f * M_PI;
    }

    // Se calculeaza pozitia de citire
    float delay_samples = (15.0f + depth * lfo_val) * c->sample_rate / 1000.0f;
    float read_pos = (float)c->write_pos - delay_samples;
    while (read_pos < 0) {
        read_pos += c->buffer_size;
    }

    // 2. Citire cu interpolare liniara
    int base = (int)read_pos;
    float frac = read_pos - base;
    int next = (base + 1) % c->buffer_size;
    float delayed_sample = c->delay_buffer[base] * (1.0f - frac) + c->delay_buffer[next] * frac;

    // 3. Scriem in buffer
    c->delay_buffer[c->write_pos] = input;
    c->write_pos = (c->write_pos + 1) % c->buffer_size;

    // 4. Se mixeaza semnalul original cu cel intarziat
    return input * (1.0f - mix) + delayed_sample * mix;
}

#endif //EFFECTS_H