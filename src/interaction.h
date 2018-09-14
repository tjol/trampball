#ifndef TRAMPBALL_INTERACTION_H
#define TRAMPBALL_INTERACTION_H

#include <stdbool.h>

#include "trampoline.h"
#include "ball.h"

typedef struct _stage {
    int top;
    int left;
    int bottom;
    int right;
} stage;

typedef struct _wall {
    vector2i position;
    vector2i side1;
    vector2i side2;
} wall;

bool collide_ball_trampoline(ball *const b, trampoline *const t);
bool collide_ball_edges(ball *const b, const stage *const s);
bool collide_ball_ball(ball *const b1, ball *const b2);
bool collide_ball_wall(ball *const b, const wall *const w);

#define new_wall() ((wall*)malloc(sizeof(wall)))
#define free_wall(w) free(w)

#endif /* TRAMPBALL_INTERACTION_H */
