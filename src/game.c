#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include "compat.h"

#include "game.h"

vector2f gravity_accel = {0, -700};

struct world game_world = { { /* top */ 300,
                              /* left */ 0,
                              /* bottom */ 0,
                              /* right */ 300 },
                            NULL,
                            NULL,
                            NULL };


void cleanup_world()
{
    while (game_world.trampolines != NULL) {
        struct trampoline_list *t_item = game_world.trampolines;
        free_trampoline(t_item->t);
        game_world.trampolines = t_item->next;
        free(t_item);
    }

    while (game_world.balls != NULL) {
        struct ball_list *b_item = game_world.balls;
        free_ball(b_item->b);
        game_world.balls = b_item->next;
        free(b_item);
    }

    while (game_world.walls != NULL) {
        struct wall_list *w_item = game_world.walls;
        free_wall(w_item->w);
        game_world.walls = w_item->next;
        free(w_item);
    }
}

inline struct trampoline_list *add_trampoline(trampoline *const t)
{
    struct trampoline_list *tl = malloc(sizeof(struct trampoline_list));
    tl->t = t;
    tl->next = game_world.trampolines;
    game_world.trampolines = tl;
    return tl;
}

inline struct ball_list *add_ball(ball *const b)
{
    struct ball_list *bl = malloc(sizeof(struct ball_list));
    bl->b = b;
    bl->next = game_world.balls;
    game_world.balls = bl;
    return bl;
}

inline struct wall_list *add_wall(wall *const w)
{
    struct wall_list *wl = malloc(sizeof(struct wall_list));
    wl->w = w;
    wl->next = game_world.walls;
    game_world.walls = wl;
    return wl;
}

void game_iteration(const float dt_ms)
{
    struct trampoline_list *tl;
    struct ball_list *bl, *bl2;
    struct wall_list *wl;

    for (tl = game_world.trampolines; tl; tl = tl->next) {
        for (bl = game_world.balls; bl; bl = bl->next)
            collide_ball_trampoline(bl->b, tl->t);

        iterate_trampoline(tl->t, dt_ms);
    }

    for (bl = game_world.balls; bl; bl = bl->next) {
        collide_ball_edges(bl->b, &game_world.game_stage);

        for (wl = game_world.walls; wl; wl = wl->next)
            collide_ball_wall(bl->b, wl->w);

        for (bl2 = bl->next; bl2; bl2 = bl2->next)
            collide_ball_ball(bl->b, bl2->b);

        iterate_ball(bl->b, dt_ms);
    }
}

bool init_game_sdlrw(SDL_RWops *fp);

bool init_game(const char *const world_file_name)
{
    SDL_RWops *fp;
    if ((fp = SDL_RWFromFile(world_file_name, "rb")) == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "[Opening world file] %s\n", SDL_GetError());
        return false;
    }

    bool status = init_game_sdlrw(fp);
    SDL_RWclose(fp);
    return status;
}

#define max_line_len 200

struct parser_state {
    trampoline *t;
    ball *b;
};

static bool handle_worldfile_line(const char *lineptr, size_t len,
                                  struct parser_state *const state);

bool init_game_sdlrw(SDL_RWops *fp)
{
    int new_bytes;
    size_t len_buffered;
    char linebuffer[max_line_len+1];
    char *lineptr = linebuffer;
    char *data_endptr = linebuffer;
    char *buffer_end = &linebuffer[max_line_len];
    char *newline_ptr;
    bool eof = false;
    struct parser_state state = { NULL, NULL };

    while (!eof) {
        new_bytes = SDL_RWread(fp, data_endptr, 1, (buffer_end-data_endptr));
        if (new_bytes == -1) {
            perror("Error reading file");
            return false; // error
        } else if (new_bytes == 0) {
            eof = true;
            // handle files not ending in newlines.
            if (*(data_endptr-1) != '\n')
                *(data_endptr++) = '\n';
        }

        data_endptr += new_bytes;

        while ((newline_ptr = memchr(lineptr, '\n', data_endptr-lineptr)) != NULL) {
            if (!handle_worldfile_line(lineptr, newline_ptr-lineptr, &state))
                return false;
            lineptr = newline_ptr+1;
        }

        len_buffered = data_endptr - lineptr;
        lineptr = memmove(linebuffer, lineptr, len_buffered);
        data_endptr = linebuffer + len_buffered;
    }
    return true;
}

static inline const char *get_floats_from_line(const char *lineptr, size_t len, int count, float *dest)
{
    char *sep, *val_end;
    for (int i=0; count != 0; i++, count--) {
        sep = memchr(lineptr, ' ', len);
        if (sep == NULL && count != 1) return NULL;

        dest[i] = strtof(lineptr, &val_end);
        if (sep != NULL && val_end != sep)
            return NULL;

        len -= (1 + sep - lineptr);
        lineptr = sep + 1;
    }
    return lineptr;
}

static inline const char *get_longs_from_line(const char *lineptr, size_t len, int count, long *dest)
{
    char *sep, *val_end;
    for (int i=0; count != 0; i++, count--) {
        sep = memchr(lineptr, ' ', len);
        if (sep == NULL && count != 1) return NULL;

        dest[i] = strtol(lineptr, &val_end, 10);
        if (sep != NULL && val_end != sep)
            return NULL;

        len -= (1 + sep - lineptr);
        lineptr = sep + 1;
    }
    return lineptr;
}

static bool handle_worldfile_line(const char *lineptr, size_t len,
                                  struct parser_state *const state)
{
    char *sep;
    float fvalues[4];
    long ivalues[6];

    while (len != 0 && isspace(*lineptr)) {
        lineptr++;
        len--;
    }

    if (len == 0 || *lineptr == '#' || *lineptr == ';') return true;

    // tokenize on spaces
    if ((sep = memchr(lineptr, ' ', len)) == NULL)
        return false;

    /* [root] STAGE top left bottom right */
    if (strncasecmp("STAGE", lineptr, sep-lineptr) == 0) {
        len -= (1 + sep - lineptr);
        lineptr = sep + 1;

        if(get_longs_from_line(lineptr, len, 4, ivalues) == NULL) return false;

        game_world.game_stage.top = ivalues[0];
        game_world.game_stage.left = ivalues[1];
        game_world.game_stage.bottom = ivalues[2];
        game_world.game_stage.right = ivalues[3];

        state->b = NULL;
        state->t = NULL;
    /* [root] GRAVITY x y */
    } else if (strncasecmp("GRAVITY", lineptr, sep-lineptr) == 0) {
        len -= (1 + sep - lineptr);
        lineptr = sep + 1;

        if (get_floats_from_line(lineptr, len, 2, fvalues) == NULL) return false;

        gravity_accel.x = fvalues[0];
        gravity_accel.y = fvalues[1];

        state->b = NULL;
        state->t = NULL;
    /* [root] BALL x y */
    } else if (strncasecmp("BALL", lineptr, sep-lineptr) == 0) {
        len -= (1 + sep - lineptr);
        lineptr = sep + 1;

        if (get_floats_from_line(lineptr, len, 2, fvalues) == NULL) return false;

        ball *b = new_ball();
        b->position = (vector2f) { fvalues[0], fvalues[1] };
        add_ball(b);
        state->b = b;
        state->t = NULL;
    /* [>BALL] RADIUS r */
    } else if (strncasecmp("RADIUS", lineptr, sep-lineptr) == 0) {
        len -= (1 + sep - lineptr);
        lineptr = sep + 1;

        if (state->b == NULL) return false;

        if (get_floats_from_line(lineptr, len, 1, fvalues) == NULL) return false;

        state->b->radius = fvalues[0];
    /* [>BALL] MASS m */
    } else if (strncasecmp("MASS", lineptr, sep-lineptr) == 0) {
        len -= (1 + sep - lineptr);
        lineptr = sep + 1;

        if (state->b == NULL) return false;

        if (get_floats_from_line(lineptr, len, 1, fvalues) == NULL) return false;

        state->b->mass = fvalues[0];
    /* [>BALL] BOUNCE factor */
    } else if (strncasecmp("BOUNCE", lineptr, sep-lineptr) == 0) {
        len -= (1 + sep - lineptr);
        lineptr = sep + 1;

        if (state->b == NULL) return false;

        if (get_floats_from_line(lineptr, len, 1, fvalues) == NULL) return false;

        state->b->bounce = fvalues[0];
    /* [root] TRAMPOLINE anchors x y width height */
    } else if (strncasecmp("TRAMPOLINE", lineptr, sep-lineptr) == 0) {
        len -= (1 + sep - lineptr);
        lineptr = sep + 1;

        if(get_longs_from_line(lineptr, len, 5, ivalues) == NULL) return false;

        trampoline *t = new_trampoline(ivalues[0]);
        t->x = ivalues[1];
        t->y = ivalues[2];
        t->width = ivalues[3];
        if (ivalues[4] != 0) { /* height */
            double delta_y = ((double)ivalues[4])/t->n_anchors;
            for (int i=0; i<t->n_anchors; ++i) {
                t->offsets[i].y = i * delta_y;
            }
        }
        add_trampoline(t);
        state->b = NULL;
        state->t = t;
    /* [>TRAMPOLINE] K spring-constant */
    } else if (strncasecmp("K", lineptr, sep-lineptr) == 0) {
        len -= (1 + sep - lineptr);
        lineptr = sep + 1;

        if (state->t == NULL) return false;

        if (get_floats_from_line(lineptr, len, 1, fvalues) == NULL) return false;

        state->t->k = fvalues[0];
    /* [>TRAMPOLINE] DENSITY density */
    } else if (strncasecmp("DENSITY", lineptr, sep-lineptr) == 0) {
        len -= (1 + sep - lineptr);
        lineptr = sep + 1;

        if (state->t == NULL) return false;

        if (get_floats_from_line(lineptr, len, 1, fvalues) == NULL) return false;

        state->t->density = fvalues[0];
    /* [>TRAMPOLINE] DAMPING damping */
    } else if (strncasecmp("DAMPING", lineptr, sep-lineptr) == 0) {
        len -= (1 + sep - lineptr);
        lineptr = sep + 1;

        if (state->t == NULL) return false;

        if (get_floats_from_line(lineptr, len, 1, fvalues) == NULL) return false;

        state->t->damping = fvalues[0];
    /* [root] WALL x y dx1 dy1 dx2 dy2 */
    } else if (strncasecmp("WALL", lineptr, sep-lineptr) == 0) {
        len -= (1 + sep - lineptr);
        lineptr = sep + 1;

        if(get_longs_from_line(lineptr, len, 6, ivalues) == NULL) return false;

        wall *w = new_wall();
        w->position.x = ivalues[0];
        w->position.y = ivalues[1];
        w->side1.x = ivalues[2];
        w->side1.y = ivalues[3];
        w->side2.x = ivalues[4];
        w->side2.y = ivalues[5];
        add_wall(w);
        state->b = NULL;
        state->t = NULL;
    } else {
        return false;
    }

    return true;
}
