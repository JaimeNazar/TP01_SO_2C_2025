

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
#include <limits.h>

#define REWARD_WEIGHT 15
#define FREE_NEIGHBORS_WEIGHT 3
#define DEAD_END_WEIGHT -50
#define DEPTH_WEIGHT 5
#define PLAYER_PROXIMITY_PENALTY -20

#define LOOKAHEAD_DEPTH 2


// Flood fill para estimar el área de control desde (x, y)
int flood_fill_area(PlayerADT p, int x, int y, bool *visited) {
    int area = 0;
    int w = get_width(p), h = get_height(p);
    int stack_size = w * h;
    int *stack_x = malloc(stack_size * sizeof(int));
    int *stack_y = malloc(stack_size * sizeof(int));
    int top = 0;
    stack_x[top] = x;
    stack_y[top] = y;
    visited[y * w + x] = true;
    while (top >= 0) {
        int cx = stack_x[top];
        int cy = stack_y[top];
        top--;
        area++;
        for (unsigned char dir = 0; dir < 8; dir++) {
            int dx, dy;
            direction_to_offset(dir, &dx, &dy);
            int nx = cx + dx, ny = cy + dy;
            if (is_cell_free(p, nx, ny) && !visited[ny * w + nx]) {
                visited[ny * w + nx] = true;
                top++;
                stack_x[top] = nx;
                stack_y[top] = ny;
            }
        }
    }
    free(stack_x);
    free(stack_y);
    return area;
}

// Devuelve true si un rival puede bloquear la celda (x, y) en 1 movimiento
bool rival_can_block(PlayerADT p, int x, int y, int my_id) {

    GameState* state = get_game_state(p);
    reader_enter(p);

    for (unsigned int i = 0; i < state->player_count; i++) {
        if ((int)i == my_id || state->players[i].blocked) continue;
        int px = state->players[i].x;
        int py = state->players[i].y;
        for (unsigned char dir = 0; dir < 8; dir++) {
            int dx, dy;
            direction_to_offset(dir, &dx, &dy);
            if (px + dx == x && py + dy == y) {

                reader_leave(p);
                return true;
            }
        }
    }

    reader_leave(p);
    return false;
}

// Calcula la distancia Manhattan al rival más cercano
int min_dist_to_rival(PlayerADT p, int x, int y) {
    GameState* state = get_game_state(p);
    reader_enter(p);

    int min_dist = INT_MAX;
    for (unsigned int i = 0; i < state->player_count; i++) {
        if ((int)i == get_id(p) || state->players[i].blocked) continue;
        int px = state->players[i].x;
        int py = state->players[i].y;
        int dist = abs(x - px) + abs(y - py);
        if (dist < min_dist) min_dist = dist;
    }

    reader_leave(p);
    return min_dist == INT_MAX ? 0 : min_dist;
}
// Simula hasta 'depth' movimientos hacia adelante y retorna el puntaje máximo alcanzable desde (x, y)
int simulate_future(PlayerADT p, int x, int y, int depth, bool *visited) {
    if (depth == 0) {
        int reward = get_board_cell(p, x, y);
        return reward;
    }
    int best = -1000000;
    for (unsigned char dir = 0; dir < 8; dir++) {
        int dx = 0, dy = 0;
        direction_to_offset(dir, &dx, &dy);
        int nx = x + dx;
        int ny = y + dy;
        if (is_cell_free(p, nx, ny) && !visited[ny * get_width(p) + nx]) {
            visited[ny * get_width(p) + nx] = true;
            int cell_value = get_board_cell(p, nx, ny);
            int score = cell_value + simulate_future(p, nx, ny, depth - 1, visited);
            if (score > best) best = score;
            visited[ny * get_width(p) + nx] = false;
        }
    }
    if (best == -1000000) {
        best = get_board_cell(p, x, y);
    }
    return best;
}


unsigned char choose_move(PlayerADT p) {
    bool endgame = is_endgame(p);
    unsigned char best_direction = 0;
    int best_score = -1000000;
    bool found_move = false;

    // NOTE: Evitar usar asi los punteros
    GameState* state = get_game_state(p);
    reader_enter(p);

    int my_id = -1;
    pid_t my_pid = getpid();
    for (unsigned int i = 0; i < state->player_count; i++) {
        if (state->players[i].pid == my_pid) {
            my_id = i;
            break;
        }
    }

    reader_leave(p);

    int board_size = get_width(p) * get_height(p);
    bool *visited = calloc(board_size, sizeof(bool));
    visited[get_y(p) * get_width(p) + get_x(p)] = true;

    for (unsigned char dir = 0; dir < 8; dir++) {
        int dx = 0, dy = 0;
        direction_to_offset(dir, &dx, &dy);

        int new_x = get_x(p) + dx;
        int new_y = get_y(p) + dy;

        if (is_cell_free(p, new_x, new_y)) {
            visited[new_y * get_width(p) + new_x] = true;
            int lookahead_score = simulate_future(p, new_x, new_y, LOOKAHEAD_DEPTH - 1, visited);
            visited[new_y * get_width(p) + new_x] = false;

            int reward = get_board_cell(p, new_x, new_y);
            int free_neighbors = count_free_neighbors(p, new_x, new_y);

            // Penalización fuerte si es dead end
            int dead_end_penalty = (free_neighbors == 0 ? 1 : 0);

            int trap_penalty = is_potential_trap(p, new_x, new_y) ? 1 : 0;

            int depth = calculate_depth(p, dx, dy, 4);

            int proximity_penalty = (my_id >= 0 && has_nearby_players(p, new_x, new_y, my_id)) ? 1 : 0;

        // 1. Penaliza si un rival puede bloquear esta celda en 1 movimiento
        int rival_block_penalty = rival_can_block(p, new_x, new_y, my_id) ? 1 : 0;

        // 2. Calcula área de control estimada desde la celda destino
        bool *ff_visited = calloc(board_size, sizeof(bool));
        int area = flood_fill_area(p, new_x, new_y, ff_visited);
        free(ff_visited);

        // 3. Prefiere moverse lejos de rivales (aislamiento)
        int dist_to_rival = min_dist_to_rival(p, new_x, new_y);

            int score = 0;
            if (endgame) {
                // En endgame, prioriza el puntaje de la celda y la profundidad (movilidad)
        score = reward * (REWARD_WEIGHT + 10) +
            depth * (DEPTH_WEIGHT + 5) +
            lookahead_score * 3 +
            area * 2 +
            dist_to_rival * 2 -
            rival_block_penalty * 1000;
            } else {
        score = reward * REWARD_WEIGHT +
            free_neighbors * FREE_NEIGHBORS_WEIGHT +
            trap_penalty * DEAD_END_WEIGHT +
            dead_end_penalty * DEAD_END_WEIGHT * 2 +
            depth * DEPTH_WEIGHT +
            proximity_penalty * PLAYER_PROXIMITY_PENALTY +
            lookahead_score * 2 +
            area * 2 +
            dist_to_rival * 2 -
            rival_block_penalty * 1000;
            }

            if (!found_move || score > best_score) {
                best_direction = dir;
                best_score = score;
                found_move = true;
            }
        }
    }

    free(visited);

    if (!found_move) {
        return 0;
    }

    return best_direction;
}

int main(int argc, char **argv) {

    PlayerADT p = init_player(argc, argv);

    if (p == NULL)
        return -1;

    // TODO: Error handling
    if(p_init_shm(p) == -1){
        return -1;
    }

    while (1) {
		// Verificar si el juego terminó o si estamos bloqueados
        if (!still_playing(p)) {
            break;
        }
        // Guardar estado actual
        get_state_snapshot(p);

        unsigned char move = choose_move(p);

        send_movement(p, move);

    }

    return 0;
}

