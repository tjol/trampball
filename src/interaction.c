#ifdef _MSC_VER
#  include <malloc.h>
#else
#  include <alloca.h>
#endif

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
    vector2f direction;
    float min_dr_sq = 2 * r_sq;

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

            if (min_dr_sq > delta_r_sq) {
                min_dr_sq = delta_r_sq;
                float this_dr = sqrtf(delta_r_sq);
                int i_before = i ? i-1 : 0;
                int i_after = (i != n_anchors-1) ? i+1 : i;
                float norm_x = (t->offsets[i_after].y - t->offsets[i_before].y);
                float norm_y = - (2 * dx + t->offsets[i_after].x - t->offsets[i_before].x);
                direction.x = this_dr * norm_x;
                direction.y = this_dr * norm_y;
            }

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
        a = new_attachment(t, n_anchors); // over-allocating, but that's OK
        a->b = b;
    }

    for (i=0, j=0; i<n_colliding; ++i) {
        k = colliding_indices[i];
        for (;j < a->n_contacts; ++j)
            if (a->contact_points[j] == k)
                break;

        /* only set the speed if this contact point is new */
        if (j == a->n_contacts && k != 0 && k != (n_anchors-1)) {
            t->speed[k].x = speed_x;
            t->speed[k].y = speed_y;
            any_new = true;
            j = 0;
        }
    }
    float dir_magn = sqrtf(direction.x*direction.x +
                           direction.y*direction.y);
    a->direction_n.x = direction.x / dir_magn;
    a->direction_n.y = direction.y / dir_magn;

    if (any_new) {
        b->speed.x = speed_x;
        b->speed.y = speed_y;
    }

    a->n_contacts = n_colliding;
    memcpy(a->contact_points, colliding_indices, n_colliding * sizeof(int));

    return true;
}

bool collide_ball_edges(ball *const b, const stage *const s)
{
    float overlap;

    vector2f reflection = {1, 1};
    if (((overlap = b->position.x - b->radius - s->left) <= 0) ||
        ((overlap = b->position.x + b->radius - s->right) >= 0)) {
            reflection.x = -b->bounce;
            b->position.x -= overlap;
    }
    if (((overlap = b->position.y - b->radius - s->bottom) <= 0) ||
        ((overlap = b->position.y + b->radius - s->top) >= 0)) {
            reflection.y = -b->bounce;
            b->position.y -= overlap;
    }

    if (reflection.x != 1 || reflection.y != 1) {
        b->speed.x *= reflection.x;
        b->speed.y *= reflection.y;
        return true;
    } else {
        return false;
    }
}

bool collide_ball_ball(ball *const b1, ball *const b2)
{
    float min_dist = b1->radius + b2->radius;
    float min_dist_sq = min_dist * min_dist;

    vector2f sep = { b2->position.x - b1->position.x,
                     b2->position.y - b1->position.y };
    float dist_sq = sep.x*sep.x + sep.y*sep.y;

    if (dist_sq > min_dist_sq) {
        return false;
    } else {
        float dist = sqrtf(dist_sq);
        vector2f sep_n = { sep.x/dist, sep.y/dist };
        vector2f momentum_transfer = {
            (b1->speed.x * b1->mass * fabsf(sep_n.x) -
             b2->speed.x * b2->mass * fabsf(sep_n.x)),
            (b1->speed.y * b1->mass * fabsf(sep_n.y) -
             b2->speed.y * b2->mass * fabsf(sep_n.y)),
        };

        // TODO: integrate ball.bounce factor somehow...
        b1->speed.x -= momentum_transfer.x / b1->mass;
        b1->speed.y -= momentum_transfer.y / b1->mass;
        b2->speed.x += momentum_transfer.x / b2->mass;
        b2->speed.y += momentum_transfer.y / b2->mass;

        float rel_r = b1->radius / min_dist;
        b1->position.x -= (min_dist - dist) * rel_r * sep_n.x;
        b1->position.y -= (min_dist - dist) * rel_r * sep_n.y;
        b2->position.x += (min_dist - dist) * (1 - rel_r) * sep_n.x;
        b2->position.y += (min_dist - dist) * (1 - rel_r) * sep_n.y;
        return true;
    }
}

static int collide_ball_line(ball *const b, vector2i pos, vector2i extent)
{
    vector2f offset, line_vec_hat;
    float length, offset_sq, dist;

    vector2i end_pos = { pos.x + extent.x, pos.y + extent.y };
    float r_sq = b->radius*b->radius;

    /* check whether the perpendicular from the centre onto our line
       falls within our segment */
    offset = (vector2f) { b->position.x - pos.x, b->position.y - pos.y };
    if ((extent.x * offset.x + extent.y * offset.y) < 0) {
        goto check_corner;
    } else {
        offset = (vector2f) { b->position.x - end_pos.x, b->position.y - end_pos.y };
        if ((extent.x * offset.x + extent.y * offset.y) > 0) {
            goto check_corner;
        }
    }
    // above the line segment? ok
    goto check_perpendicular;

check_corner:
    offset_sq = offset.x*offset.x + offset.y*offset.y;
    if (offset_sq <= r_sq) {
        length = sqrtf(extent.x*extent.x + extent.y*extent.y);
        dist = sqrtf(offset_sq);
        goto collision_detected;
    } else {
        return 0;
    }

check_perpendicular:
    /* a .CROSS. b = |a| |b| sin \theta

       if we got here, then ``offset'' is the offset from the END point.
    */
    length = sqrtf(extent.x*extent.x + extent.y*extent.y);
    dist = fabsf((offset.x * extent.y - offset.y * extent.x) / length);
    if (dist <= b->radius)
        goto collision_detected;
    else
        return 0;

collision_detected:
    /* reflect off of the line */
    line_vec_hat = (vector2f) { extent.x / length, extent.y / length };
    float speed_along = b->speed.x * line_vec_hat.x + b->speed.y * line_vec_hat.y;
    vector2f velocity_along = { speed_along * line_vec_hat.x, speed_along * line_vec_hat.y };
    b->speed.x = -b->bounce * (b->speed.x - velocity_along.x) + velocity_along.x;
    b->speed.y = -b->bounce * (b->speed.y - velocity_along.y) + velocity_along.y;

    b->position.x -= line_vec_hat.y * (b->radius - dist);
    b->position.y += line_vec_hat.x * (b->radius - dist);
    return 1;
}

bool collide_ball_wall(ball *const b, const wall *const w)
{
    return (
        collide_ball_line(b, w->position, w->side1) +
        collide_ball_line(b, w->position, w->side2) +
        collide_ball_line(b, (vector2i){w->position.x + w->side1.x,
                                        w->position.y + w->side1.y}, w->side2) +
        collide_ball_line(b, (vector2i){w->position.x + w->side2.x,
                                        w->position.y + w->side2.y}, w->side1)
        ) ? true : false;
}

