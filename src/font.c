#include <fcntl.h>
#include <string.h>
#include <stdlib.h>

#include "compat.h"
#include "font.h"

bool init_trampballfont(SDL_Renderer *const ren, const char *const filename,
                        Uint32 fg_rgba, Uint32 bg_rgba,
                        trampballfont_sdl *font)
{
    int i, fd, total_pixels, total_bytes, bytes, bitmode;
    char buf1[16], *buf2, *buf3;
    struct {
        Uint32 fontsize, w, h, nchars;
    } pars;
    SDL_Surface *surface;
    SDL_Color colours[256];

    if ((fd = open(filename, O_RDONLY)) < 0) {
        return false;
    }

    /* read header */
    if (read(fd, buf1, 16) != 16) {
        close(fd);
        return false;
    }
    /* check file format */
    if (strncmp("TRAMPBALLFONT 1", buf1, 15) != 0) {
        close(fd);
        return false;
    }
    switch(buf1[15]) {
        case 0:
            bitmode = 0;
            break;
        case 'b':
            bitmode = 1;
            break;
        default:
            close(fd);
            return false;
    }
    /* read parameters */
    if (read(fd, &pars, 16) != 16) {
        close(fd);
        return false;
    }
    font->fontsize = ntohl(pars.fontsize);
    font->chr_w = ntohl(pars.w);
    font->chr_h = ntohl(pars.h);
    font->n_chars = ntohl(pars.nchars);

    total_pixels = font->n_chars * font->chr_w * font->chr_h;
    if (bitmode) {
        total_bytes = total_pixels/8 + !!(total_pixels%8);
    } else {
        total_bytes = total_pixels;
    }

    buf2 = malloc(total_bytes);

    bytes = 0;
    do {
        int just_read = read(fd, &buf2[bytes], total_bytes-bytes);
        if (just_read <= 0) {
            close(fd);
            free(buf2);
            return false;
        }
        bytes += just_read;
    } while (bytes < total_bytes);

    if (bitmode) {
        buf3 = malloc(total_pixels);
        for (i=0; i<total_pixels; ++i) {
            buf3[i] = ((buf2[i/8] >> (7 - i%8)) & 1) ? 0xff : 0x00;
        }
    } else {
        buf3 = buf2;
    }

    close(fd);

    surface = SDL_CreateRGBSurfaceFrom(buf3, font->chr_w,
                                       font->n_chars * font->chr_h,
                                       8, font->chr_w,
                                       0, 0, 0, 0);

    if (surface == NULL) {
        free(buf2);
        if (bitmode) free(buf3);
        return false;
    }

    for (i=0; i<256; ++i) {
        colours[i].r = ((bg_rgba >> 24)&0xff) + i * (((fg_rgba >> 24)&0xff) - ((bg_rgba >> 24)&0xff)) / 255;
        colours[i].g = ((bg_rgba >> 16)&0xff) + i * (((fg_rgba >> 16)&0xff) - ((bg_rgba >> 16)&0xff)) / 255;
        colours[i].b = ((bg_rgba >>  8)&0xff) + i * (((fg_rgba >>  8)&0xff) - ((bg_rgba >>  8)&0xff)) / 255;
        colours[i].a = ((bg_rgba >>  0)&0xff) + i * (((fg_rgba >>  0)&0xff) - ((bg_rgba >>  0)&0xff)) / 255;
    }

    SDL_SetPaletteColors(surface->format->palette, colours, 0, 256);
    SDL_SetSurfaceBlendMode(surface, SDL_BLENDMODE_BLEND);

    font->texture = SDL_CreateTextureFromSurface(ren, surface);

    SDL_FreeSurface(surface);

    free(buf2);
    if (bitmode) free(buf3);

    if (font->texture != NULL) {
        return true;
    } else {
        return false;
    }
}


void render_string(const trampballfont_sdl *const font, SDL_Renderer *const ren,
                   const char *const str, SDL_Point location,
                   const float scale, const int flags)
{
    int out_chr_w = scale * font->chr_w;
    int out_chr_h = scale * font->chr_h;

    SDL_Rect src, dst;

    src.w = font->chr_w;
    src.h = font->chr_h;
    src.x = 0;
    src.y = 0;

    if (flags & TEXT_RENDER_FLAG_CENTERED_X) {
        int total_w = strlen(str) * out_chr_w;
        location.x -= total_w/2;
    }

    if (flags & TEXT_RENDER_FLAG_CENTERED_X) {
        location.y -= out_chr_h/2;
    }

    dst.w = out_chr_w;
    dst.h = out_chr_h;
    dst.x = location.x;
    dst.y = location.y;

    for (int i=0; str[i]; ++i) {
        int charidx = (str[i] - ' ');
        if (charidx < 0 || charidx >= font->n_chars) {
            charidx = 0;
        }

        src.y = font->chr_h * charidx;
        SDL_RenderCopy(ren, font->texture, &src, &dst);

        dst.x += out_chr_w;
    }
}


