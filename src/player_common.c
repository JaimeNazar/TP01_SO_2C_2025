#include <stdlib.h>
#include "include/player_common.h"
#include "include/common.h"

bool is_valid_position(GameState *state, int x, int y) {
    return x >= 0 && x < state->width && y >= 0 && y < state->height;
}

int get_board_cell(GameState *state, int x, int y) {
    if (!is_valid_position(state, x, y)) {
        return -1; // Valor inválido para posiciones fuera del tablero
    }
    return state->board[y * state->width + x];
}

bool is_cell_free(GameState *state, int x, int y) {
    int cell_value = get_board_cell(state, x, y);
    return cell_value >= 1 && cell_value <= 9;
}

void direction_to_offset(unsigned char dir, int *dx, int *dy) {
    switch (dir) {
        case 0: // arriba
            *dx = 0;  *dy = -1; break;
        case 1: // arriba-derecha
            *dx = 1;  *dy = -1; break;
        case 2: // derecha
            *dx = 1;  *dy = 0;  break;
        case 3: // abajo-derecha
            *dx = 1;  *dy = 1;  break;
        case 4: // abajo
            *dx = 0;  *dy = 1;  break;
        case 5: // abajo-izquierda
            *dx = -1; *dy = 1;  break;
        case 6: // izquierda
            *dx = -1; *dy = 0;  break;
        case 7: // arriba-izquierda
            *dx = -1; *dy = -1; break;
        default: // dirección inválida
            *dx = 0; *dy = 0;   break;
    }
}


int count_free_neighbors(GameState *state, int x, int y) {
    int count = 0;
    for (unsigned char dir = 0; dir < 8; dir++) {
        int dx = 0, dy = 0;
        direction_to_offset(dir, &dx, &dy);
        int nx = x + dx;
        int ny = y + dy;
        if (is_cell_free(state, nx, ny)) {
            count++;
        }
    }
    return count;
}



int calculate_depth(GameState *state, int start_x, int start_y, int dx, int dy, int max_depth) {
    int depth = 0;
    int x = start_x + dx;
    int y = start_y + dy;

    while (depth < max_depth && is_cell_free(state, x, y)) {
        depth++;
        x += dx;
        y += dy;
    }

    return depth;
}

bool has_nearby_players(GameState *state, int x, int y, int my_id) {
    for (int i = 0; (unsigned int) i < state->player_count; i++) {
        if (i == my_id || state->players[i].blocked) continue;

        int player_x = state->players[i].x;
        int player_y = state->players[i].y;

        int dx = abs(x - player_x);
        int dy = abs(y - player_y);

        if (dx <= 2 && dy <= 2 && (dx + dy) <= 3) {
            return true;
        }

    }
    return false;
}

bool is_potential_trap(GameState *state, int x, int y) {
    int free_neighbors = count_free_neighbors(state, x, y);

    if (free_neighbors == 1) {
        for (unsigned char dir = 0; dir < 8; dir++) {
            int dx = 0, dy = 0;
            direction_to_offset(dir, &dx, &dy);
            int nx = x + dx;
            int ny = y + dy;

            if (is_cell_free(state, nx, ny)) {
                int next_neighbors = count_free_neighbors(state, nx, ny);
                return next_neighbors <= 2;
            }
        }
    }

    return free_neighbors == 0;
}