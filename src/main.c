#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <SDL.h>
#include <sys/timeb.h>

#include "game.h"
#include "font.h"

static Uint64 perf_freq;

#define DEFAULT_WINDOW_WIDTH 480
#define DEFAULT_WINDOW_HEIGHT 640
#define DEFAULT_SCALING 1.0
#define DEFAULT_FPS 60
#define OVER_EDGE_MAX 1

static int WINDOW_WIDTH = DEFAULT_WINDOW_WIDTH;
static int WINDOW_HEIGHT = DEFAULT_WINDOW_HEIGHT;
static double SCALING = DEFAULT_SCALING;
static int FPS = DEFAULT_FPS;

static SDL_Window *game_window = NULL;
static SDL_Renderer *renderer = NULL;
static bool must_quit = false;
static trampballfont_sdl font_perfect16;

static SDL_Point origin;

static void cleanup()
{
    if (renderer != NULL) {
        SDL_DestroyRenderer(renderer);
        renderer = NULL;
    }
    if (game_window != NULL) {
        SDL_DestroyWindow(game_window);
        game_window = NULL;
    }

    cleanup_world();

    SDL_Quit();
}

static inline void print_SDL_error(const char *msg)
{
    fprintf(stderr, "error - %s - %s\n", msg, SDL_GetError());
}

void handle_events()
{
    SDL_Event ev;
    SDL_KeyboardEvent *kev;
    while (SDL_PollEvent(&ev)) {
        switch (ev.type) {
        case SDL_KEYDOWN:
            kev = (SDL_KeyboardEvent*) &ev;
            switch (kev->keysym.sym) {
                case SDLK_q:
                case SDLK_ESCAPE:
                    must_quit = true;
            }
            break;
        case SDL_QUIT:
            must_quit = true;
        }
    }
}

void draw_trampoline(const trampoline *const t)
{
    SDL_Point *points = calloc(t->n_anchors, sizeof(SDL_Point));
    float x = origin.x + t->x * SCALING;
    int y = origin.y - t->y * SCALING;
    float delta = ((float)t->width) / (t->n_anchors-1) * SCALING;

    for (int i = 0; i<t->n_anchors; ++i)
    {
        points[i].x = (int) (x + t->offsets[i].x * SCALING);
        points[i].y = (int) (y - t->offsets[i].y * SCALING);
        x += delta;
    }

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, SDL_ALPHA_OPAQUE);
    SDL_RenderDrawLines(renderer, points, t->n_anchors);

    // definitely the simplest way to draw multi-pixel markers #NOT
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, SDL_ALPHA_OPAQUE);
    SDL_RenderDrawPoints(renderer, points, t->n_anchors);
    for (int i = 0; i<t->n_anchors; ++i) points[i] = (SDL_Point) {points[i].x-1, points[i].y};
    SDL_RenderDrawPoints(renderer, points, t->n_anchors);
    for (int i = 0; i<t->n_anchors; ++i) points[i] = (SDL_Point) {points[i].x+1, points[i].y-1};
    SDL_RenderDrawPoints(renderer, points, t->n_anchors);
    for (int i = 0; i<t->n_anchors; ++i) points[i] = (SDL_Point) {points[i].x, points[i].y+2};
    SDL_RenderDrawPoints(renderer, points, t->n_anchors);
    for (int i = 0; i<t->n_anchors; ++i) points[i] = (SDL_Point) {points[i].x+1, points[i].y-1};
    SDL_RenderDrawPoints(renderer, points, t->n_anchors);

    free(points);
}

void draw_ball(const ball *const b)
{
    float angle_step = M_PI * 2 / 60;

    float x0 = origin.x + b->position.x * SCALING;
    float y0 = origin.y - b->position.y * SCALING;
    SDL_Point points[60];

    float angle;
    int i;
    for (i=0, angle=0; i<60; ++i, angle += angle_step) {
        points[i].x = x0 + b->radius * cosf(angle) * SCALING;
        points[i].y = y0 + b->radius * sinf(angle) * SCALING;
    }

    SDL_SetRenderDrawColor(renderer, 255, 0, 0, SDL_ALPHA_OPAQUE);
    SDL_RenderDrawPoints(renderer, points, 60);
}

void draw_wall(const wall *const w)
{
    SDL_Point corners[5];
    corners[0] = (SDL_Point) { origin.x + w->position.x * SCALING,
                               origin.y - w->position.y * SCALING };
    corners[1] = (SDL_Point) { corners[0].x + w->side1.x * SCALING,
                               corners[0].y - w->side1.y * SCALING};
    corners[2] = (SDL_Point) { corners[1].x + w->side2.x * SCALING,
                               corners[1].y - w->side2.y * SCALING};
    corners[3] = (SDL_Point) { corners[2].x - w->side1.x * SCALING,
                               corners[2].y + w->side1.y * SCALING};
    corners[4] = corners[0];

    SDL_SetRenderDrawColor(renderer, 0, 128, 255, SDL_ALPHA_OPAQUE);
    SDL_RenderDrawLines(renderer, corners, 5);
}

void draw_edges(const stage *const s)
{
    int top = origin.y - s->top * SCALING;
    int left = origin.x + s->left * SCALING;
    int bottom = origin.y - s->bottom * SCALING;
    int right = origin.x + s->right * SCALING;
    SDL_Point corners[5] = {
        { left, top },
        { right, top },
        { right, bottom },
        { left, bottom },
        { left, top }
    };

    SDL_SetRenderDrawColor(renderer, 255, 255, 0, SDL_ALPHA_OPAQUE);
    SDL_RenderDrawLines(renderer, corners, 5);
}

void center_ball(const ball *const b)
{
    // origin is defined as the location in window coordinates
    // of the (0,0) point in game coordinates.

    origin.x = (WINDOW_WIDTH/2 - b->position.x * SCALING);
    origin.y = (b->position.y * SCALING + WINDOW_HEIGHT/2);

    int over_left   = origin.x + game_world.game_stage.left * SCALING;
    int over_top    = origin.y - game_world.game_stage.top * SCALING;
    int over_right  = WINDOW_WIDTH - origin.x - game_world.game_stage.right * SCALING;
    int over_bottom = WINDOW_HEIGHT - origin.y + game_world.game_stage.bottom * SCALING;

    if (over_left > OVER_EDGE_MAX)
        origin.x -= (over_left - OVER_EDGE_MAX);
    if (over_top > OVER_EDGE_MAX)
        origin.y -= (over_top - OVER_EDGE_MAX);
    if (over_right > OVER_EDGE_MAX)
        origin.x += (over_right - OVER_EDGE_MAX);
    if (over_bottom > OVER_EDGE_MAX)
        origin.y += (over_bottom - OVER_EDGE_MAX);

}

void main_loop_iter(const Uint32 delay_ms, const bool calc)
{
    static double fps = 0;
    static char hudline[255];
    static Uint32 last_hud = 2000;
    static double ms_in_calc = 0;

    struct trampoline_list *tl;
    struct ball_list *bl;
    struct wall_list *wl;
    struct timeb tb0, tb1;

    clock_t t0 = clock();
    ftime(&tb0);

    // we need to define the origin FIRST
    center_ball(game_world.balls->b);

    // Draw a black background
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(renderer);

    draw_edges(&game_world.game_stage);

    // draw scene
    for (tl = game_world.trampolines; tl; tl = tl->next) {
        draw_trampoline(tl->t);
    }

    for (bl = game_world.balls; bl; bl = bl->next) {
        draw_ball(bl->b);
    }

    for (wl = game_world.walls; wl; wl = wl->next) {
        draw_wall(wl->w);
    }

    if (calc) {
        Uint64 t0_calc = SDL_GetPerformanceCounter();

        game_iteration(delay_ms);

        Uint64 t1_calc = SDL_GetPerformanceCounter();

		ms_in_calc = (1000.0 * (t1_calc - t0_calc)) / perf_freq;
    }

    if (last_hud >= 40) {
        snprintf(hudline, 255, "%.1f fps - calc in %.2f ms", fps, ms_in_calc);
        last_hud = 0;
    } else {
        last_hud += delay_ms;
    }

    render_string(&font_perfect16, renderer, hudline, (SDL_Point) {40, 10}, 1);

    SDL_RenderPresent(renderer);

    handle_events();
    clock_t t1 = clock();
    double ms_elapled = (1000.0 * (t1-t0)) / CLOCKS_PER_SEC;
    double ms_to_wait = delay_ms - ms_elapled;

    if (ms_to_wait > 0) SDL_Delay(ms_to_wait);

    ftime(&tb1);
    long dt_ms = 1000 * (tb1.time - tb0.time) + (tb1.millitm - tb0.millitm);

    fps = 1e3 / dt_ms;
}


int init_display()
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        print_SDL_error("SDL_Init");
        return 1;
    }

    if (!(game_window = SDL_CreateWindow("tramp",
                                         SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                         WINDOW_WIDTH, WINDOW_HEIGHT,
                                         SDL_WINDOW_SHOWN))) {
        print_SDL_error("SDL_CreateWindow");
        cleanup();
        return 1;
    }

    if (!(renderer = SDL_CreateRenderer(game_window, -1,
            SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC))) {
        print_SDL_error("SDL_CreateRenderer");
        cleanup();
        return 1;
    }

    if (!init_trampballfont(renderer, "res/font/perfect_dos_vga/perfect16.tbf",
                            0x11aa11ff, 0x00000000, &font_perfect16)) {
        fprintf(stderr, "Error loading font\n");
        cleanup();
        return 1;
    }

    origin = (SDL_Point) { 0, WINDOW_HEIGHT };

    return 0;
}

int parse_args(int argc, char *argv[],
               char *flags[],
               char *opts_arg[],
               int max_args,
               bool out_flags[],
               char *out_opt_values[],
               char *out_args[])
{
    int i;

    int n_args_so_far = 0;
    int hungry_opt = -1;

    for (int i=0; flags[i] != NULL; ++i) {
        out_flags[i] = false;
    }
    
    for (int i=0; opts_arg[i] != NULL; ++i) {
        out_opt_values[i] = NULL;
    }

    while (argc-- > 1) {
        ++argv;

        if (argv[0][0] == '-' && hungry_opt == -1) {
            // this is an option/a flag
            int candidates = 0;
            int cand_flag = -1;
            int cand_opt = -1;
            int len = strlen(argv[0]);

            for (i=0; flags[i] != NULL; ++i) {
                if (strncmp(flags[i], &argv[0][1], len-1) == 0) {
                    cand_flag = i;
                    ++candidates;
                }
            }

            for (i=0; opts_arg[i] != NULL; ++i) {
                if (strncmp(opts_arg[i], &argv[0][1], len-1) == 0) {
                    cand_opt = i;
                    ++candidates;
                }
            }

            if (candidates == 0) {
                fprintf(stderr, "Unrecognized option: %s\n", &argv[0][1]);
                return -1;
            } else if (candidates == 1) {
                if (cand_flag >= 0) {
                    out_flags[cand_flag] = true;
                } else  {
                    hungry_opt = cand_opt;
                }
            } else {
                fprintf(stderr, "Ambigious option: %s\n", &argv[0][1]);
                return -1;
            }
        } else if (hungry_opt >= 0) {
            // option value
            out_opt_values[hungry_opt] = argv[0];
            hungry_opt = -1;
        } else {
            // regular argument
            if (n_args_so_far >= max_args) {
                fprintf(stderr, "Too many arguments!\n");
                return -1;
            }
            out_args[n_args_so_far++] = argv[0];
        }
    }

    return n_args_so_far;
}

int main(int argc, char *argv[])
{
    char *flags[] = { "help", NULL };
    char *opts[] = { "width", "height", "scaling", "fps", "slomo", NULL };
    bool show_help = false;
    char *opt_vals[5];
    char *world_fn = "res/worldfile.txt";
    int slomo = 1;

    int n_args = parse_args(argc, argv, flags, opts, 1,
                            &show_help, opt_vals, &world_fn);

    if (n_args < 0 || show_help) {
        fprintf(stderr, "trampball - balls bouncing on trampolines\n"
                        "\n"
                        "  Usage: %s [-width 480] [-height 640] [-scaling 1]\n"
                        "            [-fps 60] [-slomo 1] [-help] res/worldfile.txt\n",
                        argv[0]);
        if (show_help) return 0;
        else return 2;
    }

    char *endp;
    if (opt_vals[0] != NULL) {
        WINDOW_WIDTH = strtol(opt_vals[0], &endp, 10);
        if (*opt_vals[0] == '\0' || *endp != '\0') {
            fprintf(stderr, "not an integer: %s\n", opt_vals[0]);
            return 2;
        }
    }
    if (opt_vals[1] != NULL) {
        WINDOW_HEIGHT = strtol(opt_vals[1], &endp, 10);
        if (*opt_vals[1] == '\0' || *endp != '\0') {
            fprintf(stderr, "not an integer: %s\n", opt_vals[1]);
            return 2;
        }
    }
    if (opt_vals[2] != NULL) {
        SCALING = strtod(opt_vals[2], &endp);
        if (*opt_vals[2] == '\0' || *endp != '\0') {
            fprintf(stderr, "not a number: %s\n", opt_vals[2]);
            return 2;
        }
    }
    if (opt_vals[3] != NULL) {
        FPS = strtol(opt_vals[3], &endp, 10);
        if (*opt_vals[3] == '\0' || *endp != '\0') {
            fprintf(stderr, "not an integer: %s\n", opt_vals[3]);
            return 2;
        }
    }
    if (opt_vals[4] != NULL) {
        slomo = strtol(opt_vals[4], &endp, 10);
        if (*opt_vals[4] == '\0' || *endp != '\0') {
            fprintf(stderr, "not an integer: %s\n", opt_vals[4]);
            return 2;
        }
    }

    if (init_display() != 0) return 1;

    if (!init_game(world_fn)) {
        cleanup();
        return 1;
    }

    perf_freq = SDL_GetPerformanceFrequency();

    for (int i=0; i<FPS && !must_quit; ++i)
        main_loop_iter(1e3/FPS, false);

    int slomo_count = 0;

    while (!must_quit) {
        if (++slomo_count == slomo) {
            slomo_count = 0;
            main_loop_iter(1e3/FPS, true);
        } else {
            main_loop_iter(1e3/FPS, false);
        }
    }

    cleanup();
    return 0;
}
