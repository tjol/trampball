#ifndef INTERACTION_H
#define INTERACTION_H

#include <stdbool.h>

#include "trampoline.h"
#include "ball.h"

bool collide_ball_trampoline(ball *const b, trampoline *const t);

#endif