#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "trampoline.h"
#include "interaction.h"

trampoline *new_trampoline(int anchors)
{
    trampoline *t = malloc(sizeof(trampoline) +
                           2 * anchors * sizeof(vector2f));
    t->offsets = (vector2f *)(((char *) t) + sizeof(trampoline));
    t->speed = t->offsets + anchors;
    t->attached_objects = NULL;

    t->n_anchors = anchors;
    t->k = TRAMPOLINE_SPRING_CONSTANT;
    t->damping = TRAMPOLINE_DAMPING;
    t->density = TRAMPOLINE_DENSITY;
    t->x = t->y = t-> width = 0;
    for (int i = 0; i < anchors; ++i) {
        t->offsets[i] = (vector2f) {0, 0};
        t->speed[i] = (vector2f) {0, 0};
    }
    return t;
}

void free_trampoline(trampoline *const t)
{
    while (t->attached_objects)
        remove_attachment(t, t->attached_objects);
    free(t);
}

attachment *new_attachment(trampoline *const t, int max_contacts)
{
    attachment *a = malloc(sizeof(attachment) + max_contacts * sizeof(int));
    a->n_contacts = 0;
    a->next = t->attached_objects;
    a->b = NULL;
    a->direction_n = (vector2f) {0, 0};
    for (int i=0; i<max_contacts; a->contact_points[i++] = -1);

    t->attached_objects = a;

    return a;
}

bool remove_attachment(trampoline *const t, attachment *a)
{
    attachment **p = &(t->attached_objects);
    while ((*p) != NULL) {
        if (*p == a) {
            *p = a->next;
            free(a);
            return true;
        }
        p = &((*p)->next);
    }
    return false;
}

bool detach_ball(trampoline *const t, const ball *const b)
{
    attachment **p = &(t->attached_objects);
    while ((*p) != NULL) {
        if ((*p)->b == b) {
            attachment *a = *p;
            *p = a->next;
            free(a);
            return true;
        }
        p = &((*p)->next);
    }
    return false;
}

attachment *find_ball_attached(trampoline *const t, const ball *const b)
{
    attachment *a = t->attached_objects;
    while (a != NULL) {
        if (a->b == b)
            return a;
        a = a->next;
    }
    
    return NULL;
}

static inline void trampoline_advance(const vector2f *const speed_in,
                                      const vector2f *const offset_in,
                                      const float *const attached_mass,
                                      vector2f *const speed_out,
                                      vector2f *const accel_out,
                                      const int n_anchors, const float dx,
                                      const float dt, const float k,
                                      const float dm,
                                      const float damping)
{
    int i;

    accel_out[0] = (vector2f) {0, 0};
    for (i=1; i<(n_anchors-1); ++i) {
        float dx1 = dx + offset_in[i].x - offset_in[i-1].x;
        float dx2 = dx + offset_in[i+1].x - offset_in[i].x;
        float dy1 = offset_in[i].y - offset_in[i-1].y;
        float dy2 = offset_in[i+1].y - offset_in[i].y;

        float m = (dm + attached_mass[i]);
        float k_over_m = k / m;
        float this_accel_x = k_over_m * (dx2 - dx1);
        float this_accel_y = k_over_m * (dy2 - dy1);

        accel_out[i].x = this_accel_x - speed_in[i].x * damping;
        accel_out[i].y = this_accel_y - speed_in[i].y * damping - g;
    }
    
    accel_out[0] = (vector2f) {0, 0};
    accel_out[n_anchors-1] = (vector2f) {0, 0};

    if (dt == 0) {
        memcpy(speed_out, speed_in, n_anchors*sizeof(vector2f));
        speed_out[0] = (vector2f) {0, 0};
        speed_out[n_anchors-1] = (vector2f) {0, 0};
        return;
    } 
    
    for (i=0; i<n_anchors; ++i) {
        speed_out[i].x = speed_in[i].x + accel_out[i].x * dt;
        speed_out[i].y = speed_in[i].y + accel_out[i].y * dt;
    }

    speed_out[0] = (vector2f) {0, 0};
    speed_out[n_anchors-1] = (vector2f) {0, 0};
}

void iterate_trampoline(trampoline *const t, const float dt_ms)
{
    int i, j;
    attachment *a;
    int n_anchors = t->n_anchors;
    float dt = dt_ms / 1000.0f;
    float dx = ((float) t->width) / n_anchors;
    float k = t->k;
    float dm = (t->density * dx);
    float tau_ms = 2e3f * sqrtf(dm/k);
    int iters_left, iters_total;

    // Try a standard (4th order) Runge-Kutta integration.
    // See: https://math.stackexchange.com/questions/721076/help-with-using-the-runge-kutta-4th-order-method-on-a-system-of-2-first-order-od
    // This kind of code makes you wish you were using FORTRAN really...
    vector2f *buf_v_a = malloc(10 * n_anchors * sizeof(vector2f));
    vector2f *v0 = buf_v_a;
    vector2f *v1 = buf_v_a + 2 * n_anchors;
    vector2f *v2 = buf_v_a + 4 * n_anchors;
    vector2f *v3 = buf_v_a + 6 * n_anchors;
    vector2f *a0 = buf_v_a + n_anchors;
    vector2f *a1 = buf_v_a + 3 * n_anchors;
    vector2f *a2 = buf_v_a + 5 * n_anchors;
    vector2f *a3 = buf_v_a + 7 * n_anchors;
    vector2f *x_tmp = buf_v_a + 8 * n_anchors;
    vector2f *v_tmp = buf_v_a + 9 * n_anchors;

    float *attached_mass = malloc(n_anchors * sizeof(float));

    for (iters_left = 1, iters_total = 1; iters_left; --iters_left) {
        for (i=0; i<n_anchors; ++i)
            attached_mass[i] = 0;

        attachment *next = NULL;
        for (a = t->attached_objects; a != NULL; a = next) {
            next = a->next;
            //if (collide_ball_trampoline(a->b, t)) {
                float extra_dm = a->b->mass / a->n_contacts;
                for (j=0; j<a->n_contacts; ++j) {
                    i = a->contact_points[j];
                    attached_mass[i] += extra_dm;
                }
            //}
        }

        trampoline_advance(t->speed, t->offsets, attached_mass, v0, a0,
                           n_anchors, dx, 0, k, dm, t->damping);
        
        float v_max = 0;
        for (i=0; i<n_anchors; ++i) {
            x_tmp[i].x = t->offsets[i].x + v0[i].x * dt/2;
            x_tmp[i].y = t->offsets[i].y + v0[i].y * dt/2;
            v_tmp[i].x = t->speed[i].x + a0[i].x * dt/2;
            v_tmp[i].y = t->speed[i].y + a0[i].y * dt/2;
            if (iters_total == 1) {
                float v_y_abs = fabsf(v_tmp[i].y);
                if (v_y_abs > v_max) v_max = v_y_abs;
            }
        }

        if (iters_total == 1) {
            // we might have to increase the number of iterations!
            // the tau term is a heuristic term to prevent numerical fluctuations
            // from inducing aphysical resonances
            iters_total = ceilf(v_max * dt + 2.1f * dt_ms/tau_ms);
            if (dt / iters_total < 1e-4f) iters_total = dt / 1e-4f;
            if (iters_total > 1) {
                iters_left = iters_total;
                dt /= iters_total;
                continue;
            }
        }

        trampoline_advance(v_tmp, x_tmp, attached_mass, v1, a1, n_anchors,
                           dx, dt/2, k, dm, t->damping);

        for (i=0; i<n_anchors; ++i) {
            x_tmp[i].x = t->offsets[i].x + v1[i].x * dt/2;
            x_tmp[i].y = t->offsets[i].y + v1[i].y * dt/2;
            v_tmp[i].x = t->speed[i].x + a1[i].x * dt/2;
            v_tmp[i].y = t->speed[i].y + a1[i].y * dt/2;
        }
        trampoline_advance(v_tmp, x_tmp, attached_mass, v2, a2, n_anchors,
                           dx, dt/2, k, dm, t->damping);

        for (i=0; i<n_anchors; ++i) {
            x_tmp[i].x = t->offsets[i].x + v2[i].x * dt;
            x_tmp[i].y = t->offsets[i].y + v2[i].y * dt;
            v_tmp[i].x = t->speed[i].x + a2[i].x * dt;
            v_tmp[i].y = t->speed[i].y + a2[i].y * dt;
        }
        trampoline_advance(v_tmp, x_tmp, attached_mass, v3, a3, n_anchors,
                           dx, dt, k, dm, t->damping);

        /* save the old positions in x_tmp. 
           we'll need them to move the ball(s)! */
        memcpy(x_tmp, t->offsets, n_anchors * sizeof(vector2f));

        for (i=0; i<n_anchors; ++i) {
            t->offsets[i].x = t->offsets[i].x + dt * (v0[i].x + v1[i].x + v2[i].x + v3[i].x)/6;
            t->offsets[i].y = t->offsets[i].y + dt * (v0[i].y + v1[i].y + v2[i].y + v3[i].y)/6;
            t->speed[i].x = t->speed[i].x + dt * (a0[i].x + a1[i].x + a2[i].x + a3[i].x)/6;
            t->speed[i].y = t->speed[i].y + dt * (a0[i].y + a1[i].y + a2[i].y + a3[i].y)/6;
        }

        for (a = t->attached_objects; a != NULL; a = a->next) {
            vector2f dx = {0, 0};
            vector2f new_speed = {0, 0};
            float new_speed_sq = 0;
            for (j=0; j<a->n_contacts; ++j) {
                i = a->contact_points[j];

                float my_dx = (t->offsets[i].x - x_tmp[i].x) * fabsf(a->direction_n.x);
                float my_dy = (t->offsets[i].y - x_tmp[i].y) * fabsf(a->direction_n.y);
                float my_vx = t->speed[i].x * fabsf(a->direction_n.x);
                float my_vy = t->speed[i].y * fabsf(a->direction_n.y);

                float my_speed_sq = my_vx*my_vx + my_vy*my_vy;
                if (my_speed_sq > new_speed_sq) {
                    dx = (vector2f) {my_dx, my_dy};
                    new_speed = (vector2f) {my_vx, my_vy};
                    new_speed_sq = my_speed_sq;
                }
            }

            vector2f gravity_slip = {- dt * g * a->direction_n.x * a->direction_n.y,
                                     + dt * g * a->direction_n.x * a->direction_n.x};

            a->b->speed.x += gravity_slip.x;
            a->b->speed.y += gravity_slip.y;

            float orthogal_speed = (a->b->speed.y * a->direction_n.x) - 
                                   (a->b->speed.x * a->direction_n.y);
            vector2f orthogal_velocity = {orthogal_speed * a->direction_n.y,
                                        - orthogal_speed * a->direction_n.x};

            new_speed.x += orthogal_velocity.x;
            new_speed.y += orthogal_velocity.y;

            vector2f speed_change = {new_speed.x - a->b->speed.x,
                                     new_speed.y - a->b->speed.y};
            
            // can only push, not pull.
            if (((a->direction_n.x > 0) && (speed_change.x > 0)) ||
                ((a->direction_n.x < 0) && (speed_change.x < 0))) {
                new_speed.x = a->b->speed.x;
                if (((a->direction_n.x > 0) && (dx.x < 0)) ||
                    ((a->direction_n.x < 0) && (dx.x > 0))) {
                    dx.x = new_speed.x * dt;
                }
            }
            if (((a->direction_n.y > 0) && (speed_change.y > 0)) ||
                ((a->direction_n.y < 0) && (speed_change.y < 0))) {
                new_speed.y = a->b->speed.y;
                if (((a->direction_n.y > 0) && (dx.y < 0)) ||
                    ((a->direction_n.y < 0) && (dx.y > 0))) {
                    dx.y = new_speed.y * dt;
                }
            }

            dx.x += orthogal_velocity.x * dt;
            dx.y += orthogal_velocity.y * dt;

            force_advance_ball(a->b, new_speed, dx);
        }

    }

    free(buf_v_a);
    free(attached_mass);
}
