#include "ball.h"

ball *new_ball()
{
    ball *b = malloc(sizeof(ball));
    b->position.x = b->position.y = b->speed.x = b->speed.y = 0.0f;
    b->mass = BALL_MASS;
    b->radius = BALL_RADIUS;
    b->remote_controlled = false;
    return b;
}

void iterate_ball(ball *const b, const float dt_ms)
{
    if (b->remote_controlled) return;
    /*
     x(t) = v(0) * t + a * t^2 / 2
    */

    float dt = dt_ms * 1e-3f;
    float a_x = b->applied_force.x / b->mass;
    float a_y = b->applied_force.y / b->mass - g;

    b->position.x += b->speed.x * dt + a_x * dt * dt * 0.5f;
    b->position.y += b->speed.y * dt + a_y * dt * dt * 0.5f;
    b->speed.x += a_x * dt;
    b->speed.y += a_y * dt;
}

void force_advance_ball(ball *const b, const vector2f new_speed, const vector2f pos_delta)
{
    /*
     x(t) = v(0) * t + a * t^2 / 2
    */

    b->position.x += pos_delta.x;
    b->position.y += pos_delta.y;
    b->speed = new_speed;
}