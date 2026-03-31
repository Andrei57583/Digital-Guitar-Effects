# Digital-Guitar-Effects
A se uitiliza urmatoarele comenzi:
- pt. compilare: gcc play_wav.c -o play_wav -lasoun
- pt. executie: ./play_wav fisier.wav

Daca apar erori la incarcarea fisierului .wav, trebuie convertit cu comanda:
- ffmpeg -i fisier.wav -ar 48000 -ac 2 -c:a pcm_s16le audio_final.wav
