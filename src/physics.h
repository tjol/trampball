#ifndef PHYSICS_H
#define PHYSICS_H

/*
 * We need some units!
 *
 * unit of distance: pixel
 * unit of time: second
 * unit of mass: arb.u. = 1
 * unit of momentum: pixel / sec
 * unit of force: pixel * sec^-2
 * unit for k: sec^-2
 */

typedef struct _vector2f {
    float x, y;
} vector2f;

typedef struct _vector2i {
    int x, y;
} vector2i;

extern vector2f gravity_accel;

#endif
