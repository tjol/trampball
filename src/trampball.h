#ifndef TRAMPBALL_TRAMPBALL_H
#define TRAMPBALL_TRAMPBALL_H

#include <SDL.h>
#include <stdint.h>

#include "config.h"

#define MODE_RUNNING 0x01
#define MODE_EXPLORE 0x02
#define MODE_INTERACTIVE 0x04

extern uint8_t game_mode;

#ifdef ENABLE_MOUSE
extern struct mouse_control_state {
    vector2f original_gravity;
    bool mouse_captured;
} mouse_control_state;

extern double MOUSE_SPEED_SCALE;
#endif

extern SDL_Point origin;
extern int WINDOW_WIDTH;
extern int WINDOW_HEIGHT;
extern double SCALING;

void cleanup();
void handle_events();

#ifdef ENABLE_MOUSE
void init_mouse_support(struct mouse_control_state *mouse_state);
void handle_mouse(struct mouse_control_state *mouse_state);
#endif

void draw_ball(const ball *const b);
void draw_wall(const wall *const w);
void draw_edges(const stage *const s);
void draw_gravity();

void center_ball(const ball *const b);

void main_loop_iter();

int startup(bool fullscreen, const char *world_fn, uint32_t calc_interval);


#endif /* TRAMPBALL_TRAMPBALL_H */
