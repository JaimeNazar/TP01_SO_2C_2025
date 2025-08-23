#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <unistd.h>

#include "common.h"

#define REWARD_WEIGHT 10
#define FREE_NEIGHBORS_WEIGHT 2
#define DEAD_END_WEIGHT -5


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
bool has_valid_moves(GameState *state, int x, int y) {
    for (unsigned char dir = 0; dir < 8; dir++) {
        int dx = 0, dy = 0;
        direction_to_offset(dir, &dx, &dy);
        int nx = x + dx;
        int ny = y + dy;
        if (is_cell_free(state, nx, ny)) {
            return true;
        }
    }
    return false;
}

// Cuenta la cantidad de celdas libres alrededor de (x, y)
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

unsigned char choose_move(Player *me, GameState *state) {
    unsigned char best_direction = 0;
    int best_score = -1000000;
    bool found_move = false;

    // Evaluar todas las direcciones posibles
    for (unsigned char dir = 0; dir < 8; dir++) {
        int dx = 0, dy = 0;
        direction_to_offset(dir, &dx, &dy);

        int new_x = me->x + dx;
        int new_y = me->y + dy;

        if (is_cell_free(state, new_x, new_y)) {
            int reward = get_board_cell(state, new_x, new_y);
            int free_neighbors = count_free_neighbors(state, new_x, new_y);

            int dead_end_penalty = has_valid_moves(state, new_x, new_y) ? 0 : DEAD_END_WEIGHT;

            // Score teniendo en cuenta mayor puntaje y celdas libres cercanas
            int score = reward * REWARD_WEIGHT + free_neighbors * FREE_NEIGHBORS_WEIGHT + dead_end_penalty;

            if (!found_move || score > best_score) {

                best_direction = dir;
                best_score = score;
                found_move = true;

            }
        }
    }

    // Si no encontramos movimiento válido, decimos que está bloqueado
    if (!found_move) {
        me->blocked = true;
        return 0;
    }

    return best_direction;
}






int main(void) {

    GameState *game_state = NULL;
    GameSync *game_sync = NULL;

    // Shared memory
    int fd = shm_open(GAME_STATE_SHM, O_RDONLY, 0666);   // Open shared memory object
    if (fd == -1) {
        perror("Error SHM\n");
    }

    game_state = mmap(0, sizeof(GameState), PROT_READ, MAP_SHARED, fd, 0); // Memory map shared memory segment

    fd = shm_open(GAME_SYNC_SHM, O_RDWR, 0666);
    if (fd == -1) {
        perror("Error SHM\n");
    }

    game_sync = mmap(0, sizeof(GameSync), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);


    //strcpy(player->name, "pepe");

    srand(time(NULL));
    int id = -1;

    // Get player id
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
        // Esperar permiso para enviar movimiento
        sem_wait(&game_sync->player_can_move[id]);

        sem_wait(&game_sync->reader_count_mutex);

        // Read game state
        game_sync->reader_count++;

        if (game_sync->reader_count == 1) {
            sem_wait(&game_sync->master_mutex);
        }

        sem_post(&game_sync->reader_count_mutex);

        sem_wait(&game_sync->state_mutex);
        // Aquí leemos el estado (ya está mapeado en memoria)
        sem_post(&game_sync->state_mutex);

        sem_wait(&game_sync->reader_count_mutex);
        game_sync->reader_count--;
        if (game_sync->reader_count == 0) {
            sem_post(&game_sync->master_mutex);
        }
        sem_post(&game_sync->reader_count_mutex);

        // Verificar si el juego terminó o si estamos bloqueados
        if (game_state->finished || p->blocked) {
            break;
        }

        // Elegir y enviar movimiento
        unsigned char move = choose_move(p, game_state);
        if (write(STDOUT_FILENO, &move, 1) != 1) {
            perror("write move");
            break;
        }

    }

    return 0;
}
