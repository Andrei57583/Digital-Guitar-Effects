# Digital-Guitar-Effects
A se uitiliza urmatoarele comenzi:
- pt. compilare: gcc play_wav.c -o play_wav -lasound -lm
- pt. compilare+neon: gcc -o3 -mcpu=cortex-a76 -ffast-math acc_wav_play.c acc_effects.h -lasound -lm -o acc_wav_play
- pt. executie: ./play_wav fisier.wav

Daca apar erori la incarcarea fisierului .wav, trebuie convertit cu comanda:
- ffmpeg -i fisier.wav -ar 48000 -ac 2 -c:a pcm_s16le audio_final.wav

Pentru a testa dispozitivul de iesire:
- aplay -D plughw:0,0 fisier.wav

Pentru live-play este nevoie sa stim portul interfetei externe (Focusrite 2i2):
- Pentru iesire  (playback): aplay -l
- Pentru intrare (record)  : arecord -l

Pentru testarea interfetei (iesire):
- speaker-test -D plughw:2,0 -c 2 -r 44100 -F S32_LE -t sine -f 440
SAU
- aplay -D plughw:2,0 fisier.wav

Pentru testarea interfetei (intrare):
arecord -D plughw:2,0 -c 2 -r 44100 -f S32_LE -d 5 -V stereo test_chitara.wav