# Digital-Guitar-Effects
A se uitiliza urmatoarele comenzi:
- pt. compilare: gcc play_wav.c -o play_wav -lasound -lm
- pt. executie: ./play_wav fisier.wav

Daca apar erori la incarcarea fisierului .wav, trebuie convertit cu comanda:
- ffmpeg -i fisier.wav -ar 48000 -ac 2 -c:a pcm_s16le audio_final.wav

Pentru a testa dispozitivul de iesire:
- aplay -D plughw:0,0 fisier.wav