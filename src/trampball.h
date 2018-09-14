#ifndef TRAMPBALL_TRAMPBALL_H
#define TRAMPBALL_TRAMPBALL_H

#include <SDL.h>
#include <stdint.h>

#define MODE_RUNNING 0x01
#define MODE_EXPLORE 0x02
#define MODE_INTERACTIVE 0x04

extern uint8_t game_mode;

extern struct mouse_control_state {
    vector2f original_gravity;
    bool mouse_captured;
} mouse_control_state;

extern SDL_Point origin;
extern int WINDOW_WIDTH;
extern int WINDOW_HEIGHT;
extern double SCALING;
extern double MOUSE_SPEED_SCALE;

void cleanup();
void handle_events();
void init_mouse_support(struct mouse_control_state *mouse_state);
void handle_mouse(struct mouse_control_state *mouse_state);

void draw_ball(const ball *const b);
void draw_wall(const wall *const w);
void draw_edges(const stage *const s);
void draw_gravity();

void center_ball(const ball *const b);

void main_loop_iter();

int startup(bool fullscreen, const char *world_fn, uint32_t calc_interval);


#endif /* TRAMPBALL_TRAMPBALL_H */
