#include <arm_neon.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <alsa/asoundlib.h>
//#include "acc_effects.h"
#include "effects.h"
#include <time.h>

#define PERIOD_SIZE 128
#define SAMPLE_RATE 44100
#define CHANNELS 2

int main() {
    int err;
    snd_pcm_t *capture_handle, *playback_handle;

    // 1. Se deschide dispozitivul ALSA
    // plughw:2,0
    const char *device = "plughw:CARD=USB,DEV=0";

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
    ChorusEffect *chorus = init_chorus((float)SAMPLE_RATE, 40.0f);
    float gain = 2.5f;
    float output_vol = 0.5f;
    float dist_gain = 20.0f;

    // Buffer procesare
    int32_t *raw_buffer = malloc(PERIOD_SIZE * CHANNELS * sizeof(int32_t));
    
    // Calculul timpului de procesare
    struct timespec start, end;
    uint64_t total_time_ns = 0;
    uint32_t loop_counter = 0;

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
        
        // START CRONOMETRU HW
        clock_gettime(CLOCK_MONOTONIC, &start);

        //(err < 0) break;
        for (int i = 0; i < PERIOD_SIZE; i++) {
            // 4. Conversie S32_LE la float. S32_LE are range: -2,147,483,647 la +2,147,483,647
            float sample = ((float)raw_buffer[i * CHANNELS] / 2147483647.0f) * gain;
            

            // 5.Soft Clip
            // float overdrive_gain = 20.0f;
            // sample = (sample * dist_gain);
            
            // 5. Hard Clip
            
            sample = hard_clip(sample * dist_gain, 0.5f);

            // 6. Prcoesare Chorus
            // depth = 2, rate = 1.0f Hz, mix = 0.6f

            sample = process_chorus(chorus, sample, 2.0f, 1.0f, 0.6f);

            // 7. Conversie inapoi la S32_LE (Stereo)
        
            int32_t out_val = (int32_t)(sample * output_vol * 2147483647.0f);
            raw_buffer[i * CHANNELS] = out_val; // stanga
            raw_buffer[i * CHANNELS + 1] = out_val; // dreapta
        }

        // Stop Cronometru
        clock_gettime(CLOCK_MONOTONIC, &end);

        // Calculăm diferența în nanosecunde
        uint64_t diff_ns = (end.tv_sec - start.tv_sec) * 1000000000LL + (end.tv_nsec - start.tv_nsec);
        total_time_ns += diff_ns;
        loop_counter++;

        if (loop_counter >= 500) {
        double avg_us = (double)total_time_ns / (loop_counter * 1000.0);
        printf("[BENCHMARK] Timp mediu procesare block (%d cadre): %.2f microsecunde\n", PERIOD_SIZE, avg_us);
        
        // Resetăm contoarele pentru următorul set de măsurători
        total_time_ns = 0;
        loop_counter = 0;
        }   


        // 8. Trimite sunetul la interfata
        err = snd_pcm_writei(playback_handle, raw_buffer, PERIOD_SIZE);
        if (err == -EPIPE) {
            snd_pcm_prepare(playback_handle);
        }
    }

    // Curatare
    free(raw_buffer);
    snd_pcm_close(capture_handle);
    snd_pcm_close(playback_handle);
    return 0;
}