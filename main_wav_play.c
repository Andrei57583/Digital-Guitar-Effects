#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <alsa/asoundlib.h>
#include "effects.h"

#pragma pack(push, 1)
typedef struct 
{
    char        chunkID[4];     // Trebuie sa fie "RIFF"
    uint32_t    chunkSize;      // Dim. totala a fisierului  minus 8 bytes
    char        format[4];      // Trebuie sa fie "WAVE"
}RiffHeader;

typedef struct 
{
    char        subchunk1ID[4]; //  "fmt" descrie formatul
    uint32_t    subchunk1Size;  // De obicei 16 pentru PCM
    uint16_t    audioFormat;    // 1 inseamna PCM (fara compresie)
    uint16_t    numChannels;    // 1 = Mono, 2 = Stereo
    uint32_t    sampleRate;     // Ex: 441000 (CD Quality)
    uint32_t    byteRate;       // Cati bytes/s sunt procesati
    uint16_t    blockAlign;     // Cati bytes/frame de sunet 
    uint16_t    bitsPerSample;  // 8, 16 sau 24 biti
    char        subchunk2ID[4]; // "data" (sunetul propriu-zis)
    uint32_t    subchunk2Size;  // Cat de mare e zona de sunet
}FmtChunk;

#pragma pack(pop)

int main(int argc, char *argv[]) {
    if (argc < 2) { printf("Utilizare: %s <file.wav>\n", argv[0]); return 1; }

    FILE *fp = fopen(argv[1], "rb");
    if (!fp) { perror("Eroare deschidere"); return 1; }

    char id[4];
    uint32_t size;
    uint16_t format, channels, bits;
    uint32_t rate;
    uint32_t data_size = 0;

    // 1. Verificăm începutul (RIFF)
    fread(id, 1, 4, fp);
    if (strncmp(id, "RIFF", 4) != 0) { printf("Nu e RIFF\n"); return 1; }
    fseek(fp, 4, SEEK_CUR); // Sărim peste dimensiunea fișierului
    fread(id, 1, 4, fp);
    if (strncmp(id, "WAVE", 4) != 0) { printf("Nu e WAVE\n"); return 1; }

    // 2. Căutăm bucățile "fmt " și "data"
    while (fread(id, 1, 4, fp) == 4) {
        fread(&size, 4, 1, fp); // Citim dimensiunea chunk-ului curent

        if (strncmp(id, "fmt ", 4) == 0) {
            fread(&format, 2, 1, fp);
            fread(&channels, 2, 1, fp);
            fread(&rate, 4, 1, fp);
            fseek(fp, 6, SEEK_CUR); // Sărim peste byteRate și blockAlign
            fread(&bits, 2, 1, fp);
            
            // Dacă fmt e mai mare de 16 octeți, sărim restul (extensii)
            if (size > 16) fseek(fp, size - 16, SEEK_CUR);
            
            if (format != 1) {
                printf("Eroare: Formatul este %u (nu e PCM Integer!)\n", format);
                return 1;
            }
        } 
        else if (strncmp(id, "data", 4) == 0) {
            data_size = size;
            break; // Am găsit sunetul!
        } 
        else {
            fseek(fp, size, SEEK_CUR); // Sărim peste orice altceva (LIST, JUNK, etc.)
        }
    }

    printf("Incarcat: %uHz, %u-bit, %u canale, %u bytes audio\n", rate, bits, channels, data_size);

    // 3. Citim datele audio
    int16_t *buffer = malloc(data_size);
    fread(buffer, 1, data_size, fp);
    fclose(fp);

    // Incarcam Efectul

    uint32_t num_samples = data_size/ sizeof(int16_t);
    float gain = 50.0f; // Crestem volumul pentru a forta distorsiunea
    float output_vol = 0.15f; // se scade volumul final pentru a compensa gain-ul

    ChorusEffect *chorus = init_chorus((float)rate, 30.0f); // delay de 30 ms


    for (uint32_t i = 0; i < num_samples; i++) {
        // Convertire int16_t la float (-1.0, 1.0)
        float sample = (float)buffer[i] / 32768.0f;
        // aplicare gain
        sample *= gain;
        
        // aplicare efect
        sample = process_chorus(chorus, sample, 5.0f, 1.5f, 0.5f); // Depth = 5ms, Rate = 1.5Hz, Mix = 0.5

        sample = soft_clip(sample);
        //sample = hard_clip(sample, 0.7f);
        sample *= output_vol;

        // Se converteste inapoi la int16_t pentru audio
        buffer[i] = (int16_t)(sample * 32767.0f);
    }

    // 4. ALSA SETUP
    snd_pcm_t *handle;
    snd_pcm_open(&handle, "plughw:0,0", SND_PCM_STREAM_PLAYBACK, 0);
    snd_pcm_set_params(handle, SND_PCM_FORMAT_S16_LE, SND_PCM_ACCESS_RW_INTERLEAVED, channels, rate, 1, 500000);

    // Redare
    snd_pcm_uframes_t frames = data_size / (channels * (bits / 8));
    snd_pcm_writei(handle, buffer, frames);

    snd_pcm_drain(handle);
    snd_pcm_close(handle);
    free(buffer);
    return 0;
}