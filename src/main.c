#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <SDL.h>
#include <sys/timeb.h>

#ifdef _WIN32
#  include <Windows.h>
static INT64 qpc_freq;
#endif

#include "game.h"
#include "font.h"

#define WINDOW_WIDTH 480
#define WINDOW_HEIGHT 640
#define FPS 60

static SDL_Window *game_window = NULL;
static SDL_Renderer *renderer = NULL;
static bool must_quit = false;
static trampballfont_sdl font_perfect16;

static SDL_Point viewport_offset;

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
    float x = viewport_offset.x + t->x;
    int y = WINDOW_HEIGHT + viewport_offset.y - t->y;
    float delta = ((float)t->width) / (t->n_anchors-1);

    for (int i = 0; i<t->n_anchors; ++i)
    {
        points[i].x = (int) (x + t->offsets[i].x);
        points[i].y = (int) (y - t->offsets[i].y);
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

    float x0 = viewport_offset.x + b->position.x;
    float y0 = WINDOW_HEIGHT + viewport_offset.y - b->position.y;
    SDL_Point points[60];

    float angle;
    int i;
    for (i=0, angle=0; i<60; ++i, angle += angle_step) {
        points[i].x = x0 + b->radius * cosf(angle);
        points[i].y = y0 + b->radius * sinf(angle);
    }

    SDL_SetRenderDrawColor(renderer, 255, 0, 0, SDL_ALPHA_OPAQUE);
    SDL_RenderDrawPoints(renderer, points, 60);
}

void draw_wall(const wall *const w)
{
    SDL_Point corners[5];
    corners[0] = (SDL_Point) { viewport_offset.x + w->position.x,
                               viewport_offset.y + WINDOW_HEIGHT - w->position.y };
    corners[1] = (SDL_Point) { corners[0].x + w->side1.x, corners[0].y - w->side1.y};
    corners[2] = (SDL_Point) { corners[1].x + w->side2.x, corners[1].y - w->side2.y};
    corners[3] = (SDL_Point) { corners[2].x - w->side1.x, corners[2].y + w->side1.y};
    corners[4] = corners[0];

    SDL_SetRenderDrawColor(renderer, 0, 128, 255, SDL_ALPHA_OPAQUE);
    SDL_RenderDrawLines(renderer, corners, 5);
}

void draw_edges(const stage *const s)
{
    int top = viewport_offset.y + WINDOW_HEIGHT - s->top;
    int left = viewport_offset.x + s->left;
    int bottom = viewport_offset.y + WINDOW_HEIGHT - s->bottom;
    int right = viewport_offset.x + s->right;
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

void main_loop_iter(const Uint32 delay_ms, const bool calc)
{
    static double fps = 0;
    static char hudline[255];
    static Uint32 last_hud = 2000;

    struct trampoline_list *tl;
    struct ball_list *bl;
    struct wall_list *wl;
    struct timeb tb0, tb1;

    clock_t t0 = clock();
    ftime(&tb0);

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
#ifdef _WIN32
		INT64 t0_calc;
		QueryPerformanceCounter((LARGE_INTEGER*)(&t0_calc));
#else
        clock_t t0_calc = clock();
#endif

        game_iteration(delay_ms);

#ifdef _WIN32
		INT64 t1_calc;
		QueryPerformanceCounter((LARGE_INTEGER*)(&t1_calc));
		double ms_in_calc = (1000.0 * (t1_calc - t0_calc)) / qpc_freq;
#else
        clock_t t1_calc = clock();
        double ms_in_calc = (1000.0 * (t1_calc - t0_calc)) / CLOCKS_PER_SEC;
#endif
        if (last_hud >= 40) {
            snprintf(hudline, 255, "%.1f fps - calc in %.2f ms", fps, ms_in_calc);
            last_hud = 0;
        } else {
            last_hud += delay_ms;
        }

        render_string(&font_perfect16, renderer, hudline, (SDL_Point) {40, 10}, 1);
    }

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

int main()
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

    viewport_offset = (SDL_Point) { 0, 0 };

    if (!init_game("res/worldfile.txt")) {
        cleanup();
        return 1;
    }

#ifdef _WIN32
	QueryPerformanceFrequency((LARGE_INTEGER*)(&qpc_freq));
#endif

    for (int i=0; i<FPS && !must_quit; ++i)
        main_loop_iter(1e3/FPS, false);

    while (!must_quit)
        main_loop_iter(1e3/FPS, true);

    cleanup();
    return 0;
}
