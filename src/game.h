#ifndef GAME_H
#define GAME_H

#include "trampoline.h"
#include "ball.h"
#include "interaction.h"

struct trampoline_list {
    struct trampoline_list *next;
    trampoline *t;
};

struct ball_list {
    struct ball_list *next;
    ball *b;
};

struct wall_list {
    struct wall_list *next;
    wall *w;
};

extern struct world {
    stage game_stage;
    struct trampoline_list *trampolines;
    struct ball_list *balls;
    struct wall_list *walls;
} game_world;

void cleanup_world();

struct trampoline_list *add_trampoline(trampoline *const t);
struct ball_list *add_ball(ball *const b);
struct wall_list *add_wall(wall *const w);

bool init_game(const char *const world_file_name);
bool init_game_fd(int fd);
void game_iteration(const float dt_ms);

#endif


