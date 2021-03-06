/*
    trampoline.h

    trampoline objects
*/

#ifndef TRAMPBALL_TRAMPOLINE_H
#define TRAMPBALL_TRAMPOLINE_H
#include "physics.h"
#include "ball.h"

#include <stdlib.h>
#include <stdbool.h>
#include <SDL.h>

#define TRAMPOLINE_SPRING_CONSTANT 80000
#define TRAMPOLINE_DAMPING 2.0f
#define TRAMPOLINE_DENSITY 0.1f /* per pixel */

typedef struct _attachment {
    struct _attachment *next;
    ball *b;
    vector2f direction_n;
    int n_contacts;
    int contact_points[];
} attachment;

typedef struct _trampoline {
    int x;
    int y;
    int width;
    int n_anchors;
    float k;
    float damping;
    float density;
    SDL_mutex *lock;
    attachment *attached_objects;
    vector2f *offsets;
    vector2f *speed;
} trampoline;

trampoline *new_trampoline(int anchors);
void free_trampoline(trampoline *const t);

attachment *new_attachment(trampoline *const t, int max_contacts);
bool remove_attachment(trampoline *const t, attachment *a);
bool detach_ball(trampoline *const t, const ball *const b);
attachment *find_ball_attached(trampoline *const t, const ball *const b);

void iterate_trampoline(trampoline *const t, const float dt_ms);

#endif /* TRAMPBALL_TRAMPOLINE_H */
