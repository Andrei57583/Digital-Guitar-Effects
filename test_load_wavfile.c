#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <alsa/asoundlib.h>

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
    if (argc < 2) {
        printf("Utilizat: %s <file.wav>\n", argv[0]);
        return 1;
    }

    FILE *fp = fopen(argv[1], "rb");
    if (!fp) { perror("File error"); return 1;}

    // Se citeste header-ul
    RiffHeader riff;
    fread(&riff, sizeof(riff), 1, fp);

    // Se cauta chunk-ul 'fmt' si 'data'
    FmtChunk fmt;
    uint32_t dataSize = 0;
    char chunkID[4];
    uint32_t chunkSize;

    // Cautam bucatile necesare
    while (fread(chunkID, 1, 4, fp) == 4) {
        fread(&chunkSize, 4, 1, fp);

        if (strncmp(chunkID, "fmt", 4) == 0) {
            fread(&fmt, sizeof(FmtChunk), 1,fp);
            // fmt > 16, se sare peste extensii
            if (chunkSize > sizeof(FmtChunk))
            fseek(fp, chunkSize - sizeof(FmtChunk), SEEK_CUR);
        }
        else if (strncmp(chunkID, "data", 4) == 0) {
            dataSize = chunkSize;
            break; // S-au gasit datele, ne oprim aici
        }
        else
        {
            // Sarim peste chunk-urile neimportante (LIST, JUNK, etc.)
            fseek(fp, chunkSize, SEEK_CUR);
        }
        
    }

    //PCM check
    if (fmt.audioFormat != 1) {
        printf("Sunt accepate doar fisiele PCM WAV.\n");
        return 1;
    }

    //Aloca si citeste date audio
    int16_t *buffer = malloc(fmt.subchunk2Size);
    if (!buffer) {printf("Eroare alocare memorie.\n"); return 1; }
    fread(buffer, 1, fmt.subchunk2Size, fp);
    fclose(fp);

    // SETUP ALSA
    snd_pcm_t *handle;

    int err = snd_pcm_open(&handle, "plughw:0,0", SND_PCM_STREAM_PLAYBACK, 0);
    if (err < 0) {
        printf("Playback open error: %s\n", snd_strerror(err));
        free(buffer);
        return 1;
    }

    err = snd_pcm_set_params( handle,
                        SND_PCM_FORMAT_S16_LE,
                        SND_PCM_ACCESS_RW_INTERLEAVED,
                        fmt.numChannels,
                        fmt.sampleRate,
                        1,          //soft resample
                        500000  ); // 0.5s latenta
    if (err < 0) {
        printf("Eroare setare parametrii: %s\n", snd_strerror(err));
    }
    
    printf("Ruleaza: %s (%uHz, %d channels, %u bytes)...\n", argv[1], fmt.sampleRate, fmt.numChannels, dataSize);
    
    // Calculam frame-urile
    int frames = fmt.subchunk2Size / (fmt.numChannels * (fmt.bitsPerSample / 8));

    // Redare
    snd_pcm_sframes_t played = snd_pcm_writei(handle, buffer, frames);
    if (played < 0) { played = snd_pcm_recover(handle, played, 0); }
    
    snd_pcm_drain(handle);
    snd_pcm_close(handle);
    free(buffer);

    return 0;
}