/*
 * SDL / SDL_mixer audio entry points missing from emscripten's SDL1 port,
 * implemented for real so the game is audible.
 *
 * Two things are missing in the browser:
 *
 *  1. SDL_LoadWAV_RW is not provided at all. Armagetron loads its sound
 *     effects (8-bit unsigned PCM WAVs) through it, so we parse the RIFF/WAVE
 *     container ourselves and hand back 16-bit signed samples at the file's
 *     native rate -- exactly what the desktop SDL_BuildAudioCVT path produced,
 *     so the engine's conversion branch (and its stub functions below) is never
 *     taken.
 *
 *  2. Mix_SetPostMix is a no-op in emscripten. Armagetron does all of its own
 *     mixing inside that post-mix callback and plays no mixer channels, so with
 *     the stub nothing is ever heard. We route Mix_OpenAudio through plain
 *     SDL_OpenAudio -- which emscripten drives via Web Audio -- and invoke the
 *     game's callback from there, zeroing the buffer first (the callback
 *     accumulates, and SDL_mixer would have pre-silenced it).
 *
 * Music (Mix_LoadMUS / fire.xm) is a separate, tracker-format path that is not
 * shipped in the data package; it stays a no-op.
 */
#include <SDL.h>
#include <SDL_mixer.h>
#include <stdlib.h>
#include <string.h>

/* ---- WAV loading --------------------------------------------------------- */

static Uint32 rd_le32(const Uint8 *p) { return p[0] | (p[1]<<8) | (p[2]<<16) | ((Uint32)p[3]<<24); }
static Uint16 rd_le16(const Uint8 *p) { return (Uint16)(p[0] | (p[1]<<8)); }

SDL_AudioSpec *SDL_LoadWAV_RW(SDL_RWops *src, int freesrc,
                              SDL_AudioSpec *spec, Uint8 **audio_buf,
                              Uint32 *audio_len)
{
    Uint8 *file = NULL;
    SDL_AudioSpec *ret = NULL;

    if (!src || !spec || !audio_buf || !audio_len)
        goto done;

    *audio_buf = NULL;
    *audio_len = 0;

    /* Slurp the whole file into memory; these effects are a few tens of KB. */
    Sint64 size = SDL_RWseek(src, 0, RW_SEEK_END);
    if (size <= 44) goto done;                 /* smaller than a WAV header */
    SDL_RWseek(src, 0, RW_SEEK_SET);
    file = (Uint8 *)malloc((size_t)size);
    if (!file) goto done;
    if (SDL_RWread(src, file, 1, (size_t)size) != (size_t)size) goto done;

    if (memcmp(file, "RIFF", 4) != 0 || memcmp(file + 8, "WAVE", 4) != 0)
        goto done;

    /* Walk the chunks looking for "fmt " and "data". */
    Uint16 fmt = 0, channels = 0, bits = 0;
    Uint32 rate = 0;
    const Uint8 *data = NULL;
    Uint32 dataLen = 0;

    size_t off = 12;
    while (off + 8 <= (size_t)size) {
        const Uint8 *ch = file + off;
        Uint32 clen = rd_le32(ch + 4);
        const Uint8 *body = ch + 8;
        if (memcmp(ch, "fmt ", 4) == 0 && clen >= 16) {
            fmt      = rd_le16(body + 0);
            channels = rd_le16(body + 2);
            rate     = rd_le32(body + 4);
            bits     = rd_le16(body + 14);
        } else if (memcmp(ch, "data", 4) == 0) {
            data = body;
            dataLen = clen;
            if (off + 8 + dataLen > (size_t)size)   /* clamp to what we read */
                dataLen = (Uint32)size - (Uint32)(off + 8);
        }
        off += 8 + clen + (clen & 1);               /* chunks are word-aligned */
    }

    if (fmt != 1 /* PCM */ || channels == 0 || !data || dataLen == 0)
        goto done;

    /* Produce 16-bit signed samples in host byte order. */
    Uint8 *out = NULL;
    Uint32 outLen = 0;
    if (bits == 8) {
        outLen = dataLen * 2;                       /* U8 -> S16 */
        out = (Uint8 *)malloc(outLen);
        if (!out) goto done;
        Sint16 *o = (Sint16 *)out;
        for (Uint32 i = 0; i < dataLen; ++i)
            o[i] = (Sint16)((int)data[i] - 128) << 8;
    } else if (bits == 16) {
        outLen = dataLen;                           /* already S16 little-endian */
        out = (Uint8 *)malloc(outLen);
        if (!out) goto done;
        memcpy(out, data, outLen);
    } else {
        goto done;                                  /* unsupported width */
    }

    spec->freq     = (int)rate;
    spec->format   = AUDIO_S16SYS;
    spec->channels = (Uint8)channels;
    spec->samples  = 4096;
    spec->size     = outLen;
    spec->callback = NULL;
    spec->userdata = NULL;

    *audio_buf = out;
    *audio_len = outLen;
    ret = spec;

done:
    free(file);
    if (src && freesrc)
        SDL_RWclose(src);
    return ret;
}

void SDL_FreeWAV(Uint8 *audio_buf)
{
    free(audio_buf);
}

/* Reached only for formats we already normalised away; keep them benign. */
int SDL_BuildAudioCVT(SDL_AudioCVT *cvt, Uint16 src_format, Uint8 src_channels,
                      int src_rate, Uint16 dst_format, Uint8 dst_channels,
                      int dst_rate)
{
    (void)src_format; (void)src_channels; (void)src_rate;
    (void)dst_format; (void)dst_channels; (void)dst_rate;
    if (cvt) { cvt->needed = 0; cvt->len_mult = 1; cvt->len_ratio = 1.0; }
    return 0;
}

int SDL_ConvertAudio(SDL_AudioCVT *cvt) { (void)cvt; return 0; }

/* ---- mixer -> SDL_OpenAudio bridge --------------------------------------- */

static void (*s_postmix)(void *, Uint8 *, int) = NULL;
static void *s_postmix_arg = NULL;
static SDL_AudioSpec s_spec;
static int s_have_spec = 0;

static void audio_trampoline(void *userdata, Uint8 *stream, int len)
{
    (void)userdata;
    /* SDL_mixer hands its post-mix callback an already-silenced buffer, into
       which the game accumulates; SDL_OpenAudio does not, so silence it here. */
    memset(stream, 0, (size_t)len);
    if (s_postmix)
        s_postmix(s_postmix_arg, stream, len);
}

int Mix_OpenAudio(int frequency, Uint16 format, int channels, int chunksize)
{
    SDL_AudioSpec desired, obtained;
    memset(&desired, 0, sizeof desired);
    desired.freq     = frequency;
    desired.format   = format;
    desired.channels = (Uint8)channels;
    desired.samples  = (Uint16)chunksize;
    desired.callback = audio_trampoline;
    desired.userdata = NULL;

    if (SDL_OpenAudio(&desired, &obtained) < 0)
        return -1;

    s_spec = obtained;
    s_have_spec = 1;
    SDL_PauseAudio(0);          /* a freshly opened mixer is playing */
    return 0;
}

void Mix_CloseAudio(void)
{
    SDL_CloseAudio();
    s_postmix = NULL;
    s_have_spec = 0;
}

void Mix_SetPostMix(void (*mix_func)(void *, Uint8 *, int), void *arg)
{
    s_postmix = mix_func;
    s_postmix_arg = arg;
}

int Mix_QuerySpec(int *frequency, Uint16 *format, int *channels)
{
    if (!s_have_spec)
        return 0;
    if (frequency) *frequency = s_spec.freq;
    if (format)    *format    = s_spec.format;
    if (channels)  *channels  = s_spec.channels;
    return 1;
}

int Mix_FadeInMusic(Mix_Music *music, int loops, int ms)
{
    (void)ms;
    return Mix_PlayMusic(music, loops);
}
