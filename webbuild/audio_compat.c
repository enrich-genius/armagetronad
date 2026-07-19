/*
 * SDL/SDL_mixer audio entry points missing from emscripten's SDL1 port.
 *
 * Armagetron loads 8-bit PCM WAV sound effects and converts them to the mixer
 * format with SDL_BuildAudioCVT/SDL_ConvertAudio, and fades in music with
 * Mix_FadeInMusic -- none of which emscripten's SDL1 provides.
 *
 * For this first WebAssembly port audio is stubbed: WAV loads succeed but
 * return a short run of silence already in the engine's native S16 format (so
 * the conversion path, and its missing functions, are never taken), and music
 * fade-in maps to a plain play. The game runs; it is simply quiet. Replacing
 * these with a real loader/converter is a self-contained follow-up.
 */
#include <SDL.h>
#include <SDL_mixer.h>
#include <stdlib.h>

SDL_AudioSpec *SDL_LoadWAV_RW(SDL_RWops *src, int freesrc,
                              SDL_AudioSpec *spec, Uint8 **audio_buf,
                              Uint32 *audio_len)
{
    if (src && freesrc)
        SDL_RWclose(src);

    if (!spec || !audio_buf || !audio_len)
        return NULL;

    spec->freq     = 22050;
    spec->format   = AUDIO_S16SYS;   /* engine-native: skips the CVT branch */
    spec->channels = 1;
    spec->samples  = 1024;
    spec->size     = 0;
    spec->callback = NULL;
    spec->userdata = NULL;

    *audio_len = 2048;               /* 1024 silent 16-bit samples */
    *audio_buf = (Uint8 *)calloc(1, *audio_len);
    return *audio_buf ? spec : NULL;
}

void SDL_FreeWAV(Uint8 *audio_buf)
{
    free(audio_buf);
}

int SDL_BuildAudioCVT(SDL_AudioCVT *cvt, Uint16 src_format, Uint8 src_channels,
                      int src_rate, Uint16 dst_format, Uint8 dst_channels,
                      int dst_rate)
{
    /* Never reached (WAVs are handed back as S16SYS), but keep it benign. */
    if (cvt) {
        cvt->needed = 0;
        cvt->len_mult = 1;
        cvt->len_ratio = 1.0;
    }
    return 0;
}

int SDL_ConvertAudio(SDL_AudioCVT *cvt)
{
    (void)cvt;
    return 0;
}

/*
 * emscripten's SDL_mixer declares Mix_QuerySpec but its implementation is a
 * hard abort("TODO"). The caller pre-loads freq/format with the spec it asked
 * Mix_OpenAudio for and only leaves `channels` uninitialised, so reporting the
 * requested values back unchanged is the accurate answer here.
 */
int Mix_QuerySpec(int *frequency, Uint16 *format, int *channels)
{
    (void)frequency;
    (void)format;
    if (channels)
        *channels = 2;
    return 1;
}

int Mix_FadeInMusic(Mix_Music *music, int loops, int ms)
{
    (void)ms;
    return Mix_PlayMusic(music, loops);
}
