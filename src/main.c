#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <SDL2/SDL.h>
#include <sys/timeb.h>

#include "trampoline.h"
#include "ball.h"
#include "interaction.h"

#define WINDOW_WIDTH 480
#define WINDOW_HEIGHT 640

static SDL_Window *game_window = NULL;
static SDL_Renderer *renderer = NULL;
static bool must_quit = false;

static struct trampoline_list {
    struct trampoline_list *next;
    trampoline *t;
} *trampolines;

static struct ball_list {
    struct ball_list *next;
    ball *b;
} *balls;

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

    while (trampolines != NULL) {
        struct trampoline_list *t_item = trampolines;
        free_trampoline(t_item->t);
        trampolines = t_item->next;
        free(t_item);
    }

    while (balls != NULL) {
        struct ball_list *b_item = balls;
        free_ball(b_item->b);
        balls = b_item->next;
        free(b_item);
    }

    SDL_Quit();
}

static inline struct trampoline_list *add_trampoline(trampoline *const t)
{
    struct trampoline_list *tl = malloc(sizeof(struct trampoline_list));
    tl->t = t;
    tl->next = trampolines;
    trampolines = tl;
    return tl;
}

static inline struct ball_list *add_ball(ball *const b)
{
    struct ball_list *bl = malloc(sizeof(struct ball_list));
    bl->b = b;
    bl->next = balls;
    balls = bl;
    return bl;
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

void main_loop_iter(const Uint32 delay_ms, const bool calc)
{
    struct trampoline_list *tl;
    struct ball_list *bl;
    struct timeb tb0, tb1;

    clock_t t0 = clock();
    ftime(&tb0);

    // Draw a black background
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(renderer);

    // draw scene
    for(tl = trampolines; tl; tl = tl->next) {
        draw_trampoline(tl->t);
    }

    for(bl = balls; bl; bl = bl->next) {
        draw_ball(bl->b);
    }

    if (calc) {
        clock_t t0_calc = clock();

        for(tl = trampolines; tl; tl = tl->next) {
            for(bl = balls; bl; bl = bl->next) {
                collide_ball_trampoline(bl->b, tl->t);
            }
            iterate_trampoline(tl->t, delay_ms);
        }

        for(bl = balls; bl; bl = bl->next) {
            iterate_ball(bl->b, delay_ms);
        }
        
        clock_t t1_calc = clock();
        double ms_in_calc = (1000.0 * (t1_calc-t0_calc)) / CLOCKS_PER_SEC;
        printf("calculation in %g us.                \r",
               1000*ms_in_calc);
        fflush(stdout);
    }

    SDL_RenderPresent(renderer);

    handle_events();
    clock_t t1 = clock();
    double ms_elapled = (1000.0 * (t1-t0)) / CLOCKS_PER_SEC;
    double ms_to_wait = delay_ms - ms_elapled;

    if (ms_to_wait > 0) SDL_Delay(ms_to_wait);
    //if (ms_to_wait > 0) usleep(1000 * ms_to_wait);

    ftime(&tb1);
    long dt_ms = 1000 * (tb1.time - tb0.time) + (tb1.millitm - tb0.millitm);

    printf("%.1f fps - ", 1e3 / dt_ms);
}

int main(int argc, char **argv)
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

    viewport_offset = (SDL_Point) { 0, 0 };

    trampoline *my_trampoline = new_trampoline(49);
    my_trampoline->x = 0;
    my_trampoline->y = 100;
    my_trampoline->width = WINDOW_WIDTH;

    add_trampoline(my_trampoline);

    // my_trampoline2 = new_trampoline(49);
    // my_trampoline2->x = 0;
    // my_trampoline2->y = 580;
    // my_trampoline2->width = WINDOW_WIDTH;

    /* // trampoline test

    float i0 = (my_trampoline->n_anchors-1)/2.0;
    float maxoff = -50;
    // float A = maxoff / (i0*i0);
    // float m = maxoff / i0;
    for (int i=0; i<my_trampoline->n_anchors; ++i) {
        float shifted_i = i - i0;
        my_trampoline->offsets[i].y = maxoff * exp(-shifted_i*shifted_i/50);
        // my_trampoline->offsets[i].y = A * shifted_i*shifted_i - maxoff;
        // my_trampoline->offsets[i].y = m * abs(shifted_i) - maxoff;
    }
    */

    ball *my_ball = new_ball();
    my_ball->position.x = WINDOW_WIDTH/3;
    my_ball->position.y = WINDOW_HEIGHT - 50;
    my_ball->radius = 25;
    add_ball(my_ball);

    ball *my_ball2 = new_ball();
    my_ball2->position.x = 2*WINDOW_WIDTH/3;
    my_ball2->position.y = WINDOW_HEIGHT - 150;
    my_ball2->radius = 25;
    add_ball(my_ball2);

    main_loop_iter(1000/60., false);
    SDL_Delay(1000);
    while (!must_quit) {
        main_loop_iter(1000/60., true);
    }

    cleanup();
    return 0;
}