/*
    font.h

    super simple spritesheet font rendering
*/

#ifndef TRAMPBALL_FONT_H
#define TRAMPBALL_FONT_H
#include <SDL.h>
#include <stdbool.h>

#define TEXT_RENDER_FLAG_CENTERED_X 1
#define TEXT_RENDER_FLAG_CENTERED_Y 2
#define TEXT_RENDER_FLAG_CENTERED (TEXT_RENDER_FLAG_CENTERED_X|TEXT_RENDER_FLAG_CENTERED_Y)

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
                   const char *const str, SDL_Point location,
                   const float scale, const int flags);

#endif /* TRAMPBALL_FONT_H */
