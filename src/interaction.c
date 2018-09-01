#include <alloca.h>
#include <math.h>
#include <string.h>

#include "interaction.h"


bool collide_ball_trampoline(ball *const b, trampoline *const t)
{
    int i, j, k;

    /* do they collide? */
    float bb_left = b->position.x - b->radius;
    float bb_right = b->position.x + b->radius;
    float bb_top = b->position.y + b->radius;
    float bb_bottom = b->position.y - b->radius;
    float r_sq = b->radius * b->radius;

    int n_anchors = t->n_anchors;
    float dx = ((float) t->width) / (n_anchors-1);
    int t_x = t->x;
    int t_y = t->y;

    int n_colliding = 0;
    int *colliding_indices = alloca(n_anchors * sizeof(int));
    vector2f *delta_hats = alloca(n_anchors * sizeof(vector2f));

    float combined_mass;
    vector2f combined_momentum = {0, 0};

    for (i=0; i<n_anchors; ++i) {
        float x, y;

        y = t_y + t->offsets[i].y;
        if (y < bb_bottom || y > bb_top) continue;
        x = t_x + i*dx + t->offsets[i].x;
        if (x < bb_left || x > bb_right) continue;
        
        // We're within the bounding box rect.
        float delta_x = b->position.x - x;
        float delta_y = b->position.y - y;
        float delta_r_sq = delta_x*delta_x + delta_y*delta_y;
        if (delta_r_sq <= r_sq) {
            // collision!
            colliding_indices[n_colliding] = i;
            // we'll multiply in the mass later
            combined_momentum.x += t->speed[i].x;
            combined_momentum.y += t->speed[i].y;

            float delta_r = sqrtf(delta_r_sq);
            delta_hats[n_colliding].x = delta_x / delta_r;
            delta_hats[n_colliding].y = delta_y / delta_r;

            n_colliding++;

            // float my_dist = fabsf(delta_x*v_hat.x + delta_y*v_hat.y);
            // if (my_dist < closest) closest = my_dist;
        }
    }

    if (!n_colliding) {
        if(detach_ball(t, b))
            b->remote_controlled = false;
        return false;
    } else {
        b->remote_controlled = true;
    }

    float dm = t->density * dx;
    combined_momentum.x *= dm;
    combined_momentum.y *= dm;
    combined_momentum.x += b->mass * b->speed.x;
    combined_momentum.y += b->mass * b->speed.y;
    combined_mass = dm * n_colliding + b->mass;

    float speed_x = combined_momentum.x / combined_mass;
    float speed_y = combined_momentum.y / combined_mass;

    attachment *a = find_ball_attached(t, b);
    bool any_new = false;

    if (a == NULL) {
        // this is a collision we didn't know about!
        a = new_attachment(t, 100); // TODO: calculate how much we NEED to allocate
        a->b = b;
    }

    a->direction_n = (vector2f) {0, 0};

    for (i=0, j=0; i<n_colliding; ++i) {
        k = colliding_indices[i];
        for (;j < a->n_contacts; ++j)
            if (a->contact_points[j] == k)
                break;

        /* only set the speed if this contact point is new */
        if (j == a->n_contacts) {
            t->speed[k].x = speed_x;
            t->speed[k].y = speed_y;
            any_new = true;
            j = 0;
        }

        a->direction_n.x -= delta_hats[i].x / n_colliding;
        a->direction_n.y -= delta_hats[i].y / n_colliding;
    }

    if (any_new) {
        b->speed.x = speed_x;
        b->speed.y = speed_y;
    }

    a->n_contacts = n_colliding;
    memcpy(a->contact_points, colliding_indices, n_colliding * sizeof(int));

    return true;
}
