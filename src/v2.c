
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <unistd.h>
#include "common.h"
#include "player_common.h"

#define REWARD_WEIGHT 15
#define FREE_NEIGHBORS_WEIGHT 3
#define DEAD_END_WEIGHT -50
#define DEPTH_WEIGHT 5
#define PLAYER_PROXIMITY_PENALTY -20






unsigned char choose_move(Player *me, GameState *state) {
    unsigned char best_direction = 0;
    int best_score = -1000000;
    bool found_move = false;

    int my_id = -1;
    pid_t my_pid = getpid();
    for (unsigned int i = 0; i < state->player_count; i++) {
        if (state->players[i].pid == my_pid) {
            my_id = i;
            break;
        }
    }

    for (unsigned char dir = 0; dir < 8; dir++) {
        int dx = 0, dy = 0;
        direction_to_offset(dir, &dx, &dy);

        int new_x = me->x + dx;
        int new_y = me->y + dy;

        if (is_cell_free(state, new_x, new_y)) {
            int reward = get_board_cell(state, new_x, new_y);
            int free_neighbors = count_free_neighbors(state, new_x, new_y);

            int trap_penalty = is_potential_trap(state, new_x, new_y) ? 1 : 0;

            int depth = calculate_depth(state, me->x, me->y, dx, dy, 4);

            int proximity_penalty = (my_id >= 0 && has_nearby_players(state, new_x, new_y, my_id)) ? 1 : 0;

            int score = reward * REWARD_WEIGHT +
                        free_neighbors * FREE_NEIGHBORS_WEIGHT +
                        trap_penalty * DEAD_END_WEIGHT +
                        depth * DEPTH_WEIGHT +
                        proximity_penalty * PLAYER_PROXIMITY_PENALTY;

            if (!found_move || score > best_score) {
                best_direction = dir;
                best_score = score;
                found_move = true;
            }
        }
    }

    if (!found_move) {
        me->blocked = true;
        return 0;
    }

    return best_direction;
}

int main(void) {

    GameState *game_state = NULL;
    GameSync *game_sync = NULL;

    int fd = shm_open(GAME_STATE_SHM, O_RDONLY, 0666);
    if (fd == -1) {
        perror("Error SHM\n");
    }

    game_state = mmap(0, sizeof(GameState), PROT_READ, MAP_SHARED, fd, 0);

    fd = shm_open(GAME_SYNC_SHM, O_RDWR, 0666);
    if (fd == -1) {
        perror("Error SHM\n");
    }

    game_sync = mmap(0, sizeof(GameSync), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    srand(time(NULL));
    int id = -1;

    pid_t my_pid = getpid();
    for (unsigned int i = 0; id < 0 && i < game_state->player_count; i++) {
        if (game_state->players[i].pid == my_pid) {
            id = i;
        }
    }
    if (id == -1) {
        fprintf(stderr, "Could not find player ID\n");
        return 1;
    }

    Player *p = &game_state->players[id];

    while (1) {
        sem_wait(&game_sync->player_can_move[id]);

        sem_wait(&game_sync->reader_count_mutex);

        game_sync->reader_count++;

        if (game_sync->reader_count == 1) {
            sem_wait(&game_sync->master_mutex);
        }

        sem_post(&game_sync->reader_count_mutex);

        sem_wait(&game_sync->state_mutex);
        sem_post(&game_sync->state_mutex);

        sem_wait(&game_sync->reader_count_mutex);
        game_sync->reader_count--;
        if (game_sync->reader_count == 0) {
            sem_post(&game_sync->master_mutex);
        }
        sem_post(&game_sync->reader_count_mutex);

        if (game_state->finished || p->blocked) {
            break;
        }

        unsigned char move = choose_move(p, game_state);
        if (write(STDOUT_FILENO, &move, 1) != 1) {
            perror("write move");
            break;
        }

    }

    return 0;
}

