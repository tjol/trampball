/*
    font.h

    super simple spritesheet font rendering
*/

#ifndef FONT_H
#define FONT_H
#include <SDL.h>
#include <stdbool.h>

typedef struct {
    SDL_Texture *texture;
    int fontsize;
    int n_chars;
    int chr_w;
    int chr_h;
} trampballfont_sdl;

bool init_trampballfont(SDL_Renderer *const ren, const char *const filename,
                        Uint32 fg_rgba, Uint32 bg_rgba,
                        trampballfont_sdl *font);
void render_string(const trampballfont_sdl *const font, SDL_Renderer *const ren,
                   const char *const str, const SDL_Point topleft,
                   const float scale);

#endif
