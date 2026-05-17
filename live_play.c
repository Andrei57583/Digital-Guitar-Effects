#include <arm_neon.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <alsa/asoundlib.h>
#include "acc_effects.h"
#include "effects.h"

#define PERIOD_SIZE 128
#define SAMPLE_RATE 44100
#define CHANNELS 2

int main() {
    int err;
    snd_pcm_t *capture_handle, *playback_handle;

    // 1. Se deschide dispozitivul ALSA
    // plughw:2,0
    const char *device = "plughw:2,0";

    printf("1. Deschidere dispozitiv captura (%s) \n", device);
    if ((err = snd_pcm_open(&capture_handle, device, SND_PCM_STREAM_CAPTURE, 0)) < 0) {
        fprintf(stderr, "Eroare deschidere captura: %s\n", snd_strerror(err));
        return 1;
    }
    
    printf("2. Deschidere dispozitiv playback (%s) \n", device);
    if ((err = snd_pcm_open(&playback_handle, device, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
        fprintf(stderr, "Eroare deschidere playback: %s\n", snd_strerror(err));
        snd_pcm_close(capture_handle);
        return 1;
    }

    // 2. Configurare parametrii S32_LE
    snd_pcm_format_t format =  SND_PCM_FORMAT_S32_LE;

    printf("3. Configurare parametrii captura (S32_LE, %dHz).\n", SAMPLE_RATE);      // 20ms buffer total
    if ((err = snd_pcm_set_params(capture_handle, format, SND_PCM_ACCESS_RW_INTERLEAVED, CHANNELS, SAMPLE_RATE, 1, 20000)) < 0) {
        fprintf(stderr, "Parametrii incorecti pentru captura: %s\n", snd_strerror(err));
        snd_pcm_close(capture_handle);
        snd_pcm_close(playback_handle);
        return 1;
    }
    printf("4. Configurare parametrii playback (S32_LE, %dHz).\n", SAMPLE_RATE);      // 20ms buffer total
    if ((err = snd_pcm_set_params(playback_handle, format, SND_PCM_ACCESS_RW_INTERLEAVED, CHANNELS, SAMPLE_RATE, 1, 20000)) < 0) {
        fprintf(stderr, "Parametrii incorecti pentru captura: %s\n", snd_strerror(err));
        snd_pcm_close(capture_handle);
        snd_pcm_close(playback_handle);
        return 1;
    }

    // 3. Initializare efecte
    AccChorusEffect *chorus = init_chorus_acc((float)SAMPLE_RATE, 30.0f);
    float gain = 40.0f;
    float output_vol = 0.15f;

    // Buffer procesare
    int32_t *raw_buffer = malloc(PERIOD_SIZE * CHANNELS * sizeof(int32_t));
    float *float_input = malloc(PERIOD_SIZE * sizeof(float));
    float *float_output = malloc(PERIOD_SIZE * sizeof(float));

    printf("Live processing pornit... Apasa Ctrl+C pentru oprire.\n");

    while(1) {
        // 3. Citire intrare (chitara)
        err = snd_pcm_readi(capture_handle, raw_buffer, PERIOD_SIZE);
        if (err < 0) {
            if (err == -EPIPE) {
                snd_pcm_prepare(capture_handle);
                continue;
            } else if (err == -EIO) {
                usleep(1000); // asteapta o milisecunda
                snd_pcm_prepare(capture_handle);
                snd_pcm_start(capture_handle);
                continue;
            } 
            else {
                fprintf(stderr, "Eroare citire audio: %s\n",snd_strerror(err));
                break;
        }
        }
        
        //(err < 0) break;

        // 4. Conversie S32_LE la float. S32_LE are range: -2,147,483,647 la +2,147,483,647
        for (int i = 0; i < PERIOD_SIZE; i++) {
            float_input[i] = ((float)raw_buffer[i * CHANNELS] / 2147483647.0f) * gain;
        }

        // 5. Prcoesare Chorus
        process_chorus_block(chorus, float_input, float_output, PERIOD_SIZE, 5.0f, 1.5f, 0.5f);

        // 6. Optimizare NEON: Soft Clip
        uint32_t vect_size = (PERIOD_SIZE / 4) * 4;
        for(uint32_t j = 0; j < vect_size; j += 4) {
            float32x4_t v = vld1q_f32(&float_output[j]);
            v = soft_clip_neon(v);
            vst1q_f32(&float_output[j], v);
        }
        for (uint32_t j = vect_size; j < PERIOD_SIZE; j++) {
            float_output[j] = soft_clip(float_output[j]);
        }

        // 7. Conversie inapoi la S32_LE (Stereo)
        for (int i = 0; i < PERIOD_SIZE; i++) {
            int32_t out_val = (int32_t)(float_output[i] * output_vol * 2147483647.0f);
            raw_buffer[i * CHANNELS] = out_val; // stanga
            raw_buffer[i * CHANNELS + 1] = out_val; // dreapta
        }

        // 8. Trimite sunetul la interfata
        err = snd_pcm_writei(playback_handle, raw_buffer, PERIOD_SIZE);
        if (err == -EPIPE) {
            snd_pcm_prepare(playback_handle);
        }
    }

    // Curatare
    free(raw_buffer);
    free(float_input);
    free(float_output);
    snd_pcm_close(capture_handle);
    snd_pcm_close(playback_handle);
    return 0;
}