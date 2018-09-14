#include "ball.h"

ball *new_ball()
{
    ball *b = malloc(sizeof(ball));
    b->position.x = b->position.y = b->speed.x = b->speed.y = 0.0f;
    b->mass = BALL_MASS;
    b->radius = BALL_RADIUS;
    b->remote_controlled = false;
    b->applied_force = (vector2f) {0, 0};
    b->bounce = BALL_BOUNCE;
    b->lock = SDL_CreateMutex();
    return b;
}

void free_ball(ball *b)
{
    SDL_DestroyMutex(b->lock);
    free(b);
}

void iterate_ball(ball *const b, const float dt_ms)
{
    if (b->remote_controlled) return;
    /*
     x(t) = v(0) * t + a * t^2 / 2
    */

    float dt = dt_ms * 1e-3f;
    float a_x = b->applied_force.x / b->mass + gravity_accel.x;
    float a_y = b->applied_force.y / b->mass + gravity_accel.y;

    SDL_LockMutex(b->lock);

    b->position.x += b->speed.x * dt + a_x * dt * dt * 0.5f;
    b->position.y += b->speed.y * dt + a_y * dt * dt * 0.5f;
    b->speed.x += a_x * dt;
    b->speed.y += a_y * dt;

    SDL_UnlockMutex(b->lock);
}

void force_advance_ball(ball *const b, const vector2f new_speed, const vector2f pos_delta)
{
    SDL_LockMutex(b->lock);

    b->position.x += pos_delta.x;
    b->position.y += pos_delta.y;
    b->speed = new_speed;

    SDL_UnlockMutex(b->lock);
}
