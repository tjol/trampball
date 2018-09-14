#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <SDL.h>
#include <sys/timeb.h>

#include "game.h"
#include "font.h"

#include "trampball.h"

/* local definitions */

#define DEFAULT_WINDOW_WIDTH 480
#define DEFAULT_WINDOW_HEIGHT 640
#define DEFAULT_MOUSE_SPEED_SCALE 8
#define DEFAULT_SCALING 1.0
#define OVER_EDGE_MAX 1

/* extern variables */

uint8_t game_mode = 0;
SDL_Point origin;
int WINDOW_WIDTH = DEFAULT_WINDOW_WIDTH;
int WINDOW_HEIGHT = DEFAULT_WINDOW_HEIGHT;
double SCALING = DEFAULT_SCALING;

#ifdef ENABLE_MOUSE
struct mouse_control_state mouse_control_state;
double MOUSE_SPEED_SCALE = DEFAULT_MOUSE_SPEED_SCALE;
#endif

/* internal state */

static SDL_Window *game_window = NULL;
static SDL_Renderer *renderer = NULL;
static bool must_quit = false;
static trampballfont_sdl font_perfect16_green;
static trampballfont_sdl font_perfect16_red;
static Uint64 perf_freq;

static uint16_t time_dilation = 1;

static double calc_time_us = 0;

void cleanup()
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
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s - %s\n", msg, SDL_GetError());
}

void handle_events()
{
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        switch (ev.type) {
        case SDL_KEYDOWN:
            switch (ev.key.keysym.sym) {
                case SDLK_q:
                    must_quit = true;
                    break;
                case SDLK_ESCAPE:
                case SDLK_SPACE:
                case SDLK_PAUSE:
                    game_mode ^= MODE_RUNNING;
                    break;
            }
            break;
        case SDL_WINDOWEVENT:
            switch(ev.window.event) {
                case SDL_WINDOWEVENT_HIDDEN:
                case SDL_WINDOWEVENT_FOCUS_LOST:
                    game_mode &= ~MODE_RUNNING;
                    break;
            }
            break;
#ifdef ENABLE_MOUSE
        case SDL_MOUSEBUTTONDOWN:
            if (ev.button.button == SDL_BUTTON_LEFT) {
                game_mode |= MODE_RUNNING;
            }
            break;
#endif
        case SDL_QUIT:
            must_quit = true;
            break;
        }
    }
}


#ifdef ENABLE_MOUSE

void init_mouse_support(struct mouse_control_state *mouse_state)
{
    mouse_state->original_gravity = gravity_accel;
    mouse_state->mouse_captured = false;
}

void handle_mouse(struct mouse_control_state *mouse_state)
{
    static int32_t mouse_tick = -1;
    gravity_accel = mouse_state->original_gravity;
    uint32_t now = SDL_GetTicks();

    if (mouse_tick < 0) {
        mouse_tick = now;
        return;
    }

    if (game_mode & MODE_RUNNING) {
        if (mouse_state->mouse_captured) {
            int x, y;
            SDL_GetRelativeMouseState(&x, &y);

            float v_x, v_y;
            float dt = ((float)(now - mouse_tick));

            v_x = x / dt;
            v_y = y / dt;

            gravity_accel.x += v_x * 1e3 * MOUSE_SPEED_SCALE / dt;
            gravity_accel.y -= v_y * 1e3 * MOUSE_SPEED_SCALE / dt;
        } else {
            SDL_SetRelativeMouseMode(SDL_TRUE);
            mouse_state->mouse_captured = true;
        }
    } else {
        if (mouse_state->mouse_captured) {
            SDL_SetRelativeMouseMode(SDL_FALSE);
            mouse_state->mouse_captured = false;
        }
    }

    mouse_tick = now;
}

#endif

void draw_trampoline(const trampoline *const t)
{
    SDL_Point *points = calloc(t->n_anchors, sizeof(SDL_Point));

    SDL_LockMutex(t->lock);

    float x = origin.x + t->x * SCALING;
    int y = origin.y - t->y * SCALING;
    float delta = ((float)t->width) / (t->n_anchors-1) * SCALING;

    for (int i = 0; i<t->n_anchors; ++i)
    {
        points[i].x = (int) (x + t->offsets[i].x * SCALING);
        points[i].y = (int) (y - t->offsets[i].y * SCALING);
        x += delta;
    }

    SDL_UnlockMutex(t->lock);

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

    SDL_LockMutex(b->lock);

    float x0 = origin.x + b->position.x * SCALING;
    float y0 = origin.y - b->position.y * SCALING;
    float radius = b->radius;

    SDL_UnlockMutex(b->lock);

    SDL_Point points[60];

    float angle;
    int i;
    for (i=0, angle=0; i<60; ++i, angle += angle_step) {
        points[i].x = x0 + radius * cosf(angle) * SCALING;
        points[i].y = y0 + radius * sinf(angle) * SCALING;
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
                               corners[0].y - w->side1.y * SCALING };
    corners[2] = (SDL_Point) { corners[1].x + w->side2.x * SCALING,
                               corners[1].y - w->side2.y * SCALING };
    corners[3] = (SDL_Point) { corners[2].x - w->side1.x * SCALING,
                               corners[2].y + w->side1.y * SCALING };
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

void draw_gravity()
{
    SDL_Point start = { WINDOW_WIDTH-50, 50 };
    int dx = gravity_accel.x / 20.0f;
    int dy = -gravity_accel.y / 20.0f;

    SDL_Point end = { start.x + dx, start.y + dy };

    double arrow_len = sqrt(dx*dx + dy*dy);
    double dx_hat = dx / arrow_len;
    double dy_hat = dy / arrow_len;

    SDL_Point tip1 = { end.x - 5 * dx_hat - 5 * dy_hat,
                       end.y + 5 * dx_hat - 5 * dy_hat };
    SDL_Point tip2 = { end.x - 5 * dx_hat + 5 * dy_hat,
                       end.y - 5 * dx_hat - 5 * dy_hat };

    SDL_SetRenderDrawColor(renderer, 255, 128, 0, 128);
    SDL_RenderDrawLines(renderer, (SDL_Point[]){ start, end }, 2);
    SDL_RenderDrawLines(renderer, (SDL_Point[]){ tip1, end, tip2 }, 3);
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

    // if the stage is too small for our screen, center!
    if ((game_world.game_stage.right - game_world.game_stage.left) * SCALING < WINDOW_WIDTH) {
        origin.x -= over_left - (over_left + over_right) / 2 - OVER_EDGE_MAX;
    } else if (over_left > OVER_EDGE_MAX) {
        origin.x -= (over_left - OVER_EDGE_MAX);
    } else if (over_right > OVER_EDGE_MAX){
        origin.x += (over_right - OVER_EDGE_MAX);
    }

    if ((game_world.game_stage.top - game_world.game_stage.bottom) * SCALING < WINDOW_HEIGHT) {
        origin.y -= over_top - (over_top + over_bottom) / 2 - OVER_EDGE_MAX;
    } else if (over_top > OVER_EDGE_MAX) {
        origin.y -= (over_top - OVER_EDGE_MAX);
    } else if (over_bottom > OVER_EDGE_MAX) {
        origin.y += (over_bottom - OVER_EDGE_MAX);
    }

}

void main_loop_iter()
{
    static double fps = 10;
    static char hudline[255];
    static uint32_t last_hud = 2000;

    struct trampoline_list *tl;
    struct ball_list *bl;
    struct wall_list *wl;
    struct timeb tb0, tb1;

    ftime(&tb0);

    // update window size
    SDL_GetWindowSize(game_window, &WINDOW_WIDTH, &WINDOW_HEIGHT);

    // define the origin
    if (!(game_mode & MODE_EXPLORE) && game_world.balls != NULL) {
        center_ball(game_world.balls->b);
    }

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

    if (last_hud >= 40) {
        snprintf(hudline, 255, "%.1f fps; calc in %.1f us", fps, calc_time_us);
        last_hud = 0;
    } else {
        last_hud += 1e3/fps;
    }

    render_string(&font_perfect16_green, renderer, hudline,
                  (SDL_Point) {40, 10}, 1, 0);

    if (!(game_mode & MODE_RUNNING)) {
        render_string(&font_perfect16_red, renderer, "PAUSED",
                      (SDL_Point) {WINDOW_WIDTH/2, WINDOW_HEIGHT/2}, 3,
                      TEXT_RENDER_FLAG_CENTERED);
        int line_x = WINDOW_WIDTH/2;
        int line_y = WINDOW_HEIGHT/2 + 30;
#ifdef ENABLE_MOUSE
        render_string(&font_perfect16_red, renderer, "Control gravity with your mouse",
                      (SDL_Point) {line_x, line_y}, 1,
                      TEXT_RENDER_FLAG_CENTERED);
        line_y += 16;
        render_string(&font_perfect16_red, renderer, "Click to start",
                      (SDL_Point) {line_x, line_y}, 1,
                      TEXT_RENDER_FLAG_CENTERED);
        line_y += 16;
#endif
        render_string(&font_perfect16_red, renderer, "Press Q to quit",
                      (SDL_Point) {line_x, line_y}, 1,
                      TEXT_RENDER_FLAG_CENTERED);
        line_y += 16;
    }

    draw_gravity();

    SDL_RenderPresent(renderer);

    handle_events();

#ifdef ENABLE_MOUSE
    handle_mouse(&mouse_control_state);
#endif

    ftime(&tb1);
    long dt_ms = 1000 * (tb1.time - tb0.time) + (tb1.millitm - tb0.millitm);

    fps = 1e3 / dt_ms;
}

static uint32_t game_timer_callback(uint32_t interval_ms, void *user_data)
{
    static int calc_counter = 0;

    double *last_time_taken_us = (double*) user_data;

    if ((game_mode & MODE_RUNNING) && ++calc_counter >= time_dilation) {
        calc_counter = 0;

        Uint64 t0_calc = SDL_GetPerformanceCounter();
        game_iteration(interval_ms);
        Uint64 t1_calc = SDL_GetPerformanceCounter();

        if (last_time_taken_us != NULL) {
            *last_time_taken_us = (1.0e6 * (t1_calc - t0_calc)) / perf_freq;
        }
    }

    return interval_ms;
}


static int init_sdl(bool fullscreen)
{
    if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER) != 0) {
        print_SDL_error("SDL_Init");
        return 1;
    }

    int windowflags = fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : SDL_WINDOW_RESIZABLE;

    if (!(game_window = SDL_CreateWindow("tramp",
                                         SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                         WINDOW_WIDTH, WINDOW_HEIGHT,
                                         windowflags))) {
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
                            0x11aa11ff, 0x00000000, &font_perfect16_green)) {
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "Error loading font\n");
        cleanup();
        return 1;
    }

    if (!init_trampballfont(renderer, "res/font/perfect_dos_vga/perfect16.tbf",
                            0xbb1111ff, 0x00000000, &font_perfect16_red)) {
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "Error loading font\n");
        cleanup();
        return 1;
    }

    origin = (SDL_Point) { 0, WINDOW_HEIGHT };

    return 0;
}

static int parse_args(int argc, char *argv[],
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
    char *flags[] = { "help", "fullscreen", NULL };
    char *opts[] = { "width", "height", "scaling", "interval", "slomo",
#ifdef ENABLE_MOUSE
                     "mouse",
#endif
                     NULL };
    bool flag_states[2];
    char *opt_vals[6];
    char *world_fn = "res/worldfile.txt";
    uint32_t calc_interval = 10;

    int n_args = parse_args(argc, argv, flags, opts, 1,
                            flag_states, opt_vals, &world_fn);

    if (n_args < 0 || flag_states[0]) {
        fprintf(stderr, "trampball - balls bouncing on trampolines\n"
                        "\n"
                        "  Usage: %s [-help] [-fullscreen] [-width 480] [-height 640]\n"
                        "         [-scaling 1] [-interval 10] [-slomo 1] [-mouse 8] res/worldfile.txt\n",
                        argv[0]);
        if (flag_states[0]) return 0;
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
        calc_interval = strtol(opt_vals[3], &endp, 10);
        if (*opt_vals[3] == '\0' || *endp != '\0') {
            fprintf(stderr, "not an integer: %s\n", opt_vals[3]);
            return 2;
        }
    }
    if (opt_vals[4] != NULL) {
        time_dilation = strtol(opt_vals[4], &endp, 10);
        if (*opt_vals[4] == '\0' || *endp != '\0') {
            fprintf(stderr, "not an integer: %s\n", opt_vals[4]);
            return 2;
        }
    }
#ifdef ENABLE_MOUSE
    if (opt_vals[5] != NULL) {
        MOUSE_SPEED_SCALE = strtod(opt_vals[5], &endp);
        if (*opt_vals[5] == '\0' || *endp != '\0') {
            fprintf(stderr, "not a number: %s\n", opt_vals[5]);
            return 2;
        }
    }
#endif

    if(startup(flag_states[1], world_fn, calc_interval) != 0) {
        cleanup();
        return 1;
    }

    while (!must_quit) {
        main_loop_iter();
    }

    cleanup();
    return 0;
}

int startup(bool fullscreen, const char *world_fn, uint32_t calc_interval)
{
    SDL_TimerID calc_timer;

    if (init_sdl(fullscreen) != 0) return 1;

    if (!init_game(world_fn)) {
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "Error loading %s\n",
                        world_fn);
        return 1;
    }

    perf_freq = SDL_GetPerformanceFrequency();

#ifdef ENABLE_MOUSE
    init_mouse_support(&mouse_control_state);
#endif

    game_mode = 0;
    calc_timer = SDL_AddTimer(calc_interval, game_timer_callback,
                              (void*)(&calc_time_us));

    if (!calc_timer) {
        print_SDL_error("SDL_AddTimer");
        return 1;
    }

    return 0;
}
