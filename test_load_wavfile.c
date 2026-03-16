#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <alsa/asoundlib.h>

#pragma pack(push, 1)
typedef struct 
{
    /* data */
    char        chunkID[4];
    uint32_t    chunkSize;
    char        format[4];
    char        subchunk1ID[4];
    uint32_t    subchunk1Size;
    uint16_t    audioFormat;
    uint16_t    numChannels;
    uint32_t    sampleRate;
    uint32_t    byteRate;
    uint16_t    blockAlign;
    uint16_t    bitsPerSample;
    char        subchunk2ID[4];
    uint32_t    subchunk2Size;
}WavFile;
#pragma pack(pop)

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Utilizat: %s <file.wav>\n", argv[0]);
        return 1;
    }

    FILE *fp = fopen(argv[1], "rb");
    if (!fp) { perror("File error"); return 1;}

    WavFile wav_file;
    fread(&wav_file, sizeof(WavFile), 1, fp);

    //PCM check
    if (wav_file.audioFormat != 1) {
        printf("Sunt accepate doar fisiele PCM WAV.\n");
        return 1;
    }

    //Aloca si citeste date audio
    int16_t *buffer = malloc(wav_file.subchunk2Size);
    fread(buffer, 1, wav_file.subchunk2Size, fp);
    fclose(fp);

    // SETUP ALSA
    snd_pcm_t *handle;

    int err = snd_pcm_open(&handle, "plughw:0,0", SND_PCM_STREAM_PLAYBACK, 0);
    if (err < 0) {
        printf("Playback open error: %s\n", snd_strerror(err));
        return 1;
    }

    err = snd_pcm_set_params( handle,
                        SND_PCM_FORMAT_S16_LE,
                        SND_PCM_ACCESS_RW_INTERLEAVED,
                        wav_file.numChannels,
                        wav_file.sampleRate,
                        1,          //soft resample
                        500000  ); // 0.5s latenta
    if (err < 0) {
        printf("Eroare setare parametrii: %s\n", snd_strerror(err));
    }
    
    printf("Ruleaza: %s (%uHz, %d channels)...\n", argv[1], wav_file.sampleRate, wav_file.numChannels);
    
    int frames = wav_file.subchunk2Size / (wav_file.numChannels * (wav_file.bitsPerSample / 8));

    snd_pcm_sframes_t played = snd_pcm_writei(handle, buffer, frames);
    if (played < 0) { played = snd_pcm_recover(handle, played, 0); }
    
    snd_pcm_drain(handle);
    snd_pcm_close(handle);
    free(buffer);

    return 0;
        
}