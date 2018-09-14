/*
    ball.h

    (rigid?) balls that fall
*/

#ifndef TRAMPBALL_BALL_H
#define TRAMPBALL_BALL_H
#include "physics.h"
#include <stdlib.h>
#include <stdbool.h>
#include <SDL.h>

#define BALL_MASS 100.0f
#define BALL_RADIUS 50.0f
#define BALL_BOUNCE 0.2f

typedef struct _ball {
    vector2f position;
    float radius;
    float mass;
    bool remote_controlled;
    vector2f speed;
    vector2f applied_force;
    float bounce;
    SDL_mutex *lock;
    // float spin;
} ball;

ball *new_ball();
void free_ball(ball *b);

void iterate_ball(ball *const b, const float dt_ms);
void force_advance_ball(ball *const b, const vector2f new_speed, const vector2f pos_delta);

#endif /* TRAMPBALL_BALL_H */
