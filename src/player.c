#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>
#include <semaphore.h>
#include <signal.h>
#include <assert.h>
#include "common.h"

#ifndef GAME_STATE_MEM
#define GAME_STATE_MEM "/game_state"
#endif
#ifndef GAME_SYNC_MEM
#define GAME_SYNC_MEM  "/game_sync"
#endif

static GameState *game_state = NULL;
static GameSync  *syncs = NULL;
static  int my_idx = -1;

static void die(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

static void find_me() {// para saber quien sooy me busco
    pid_t me = getpid();
    my_idx = -1;
    for (unsigned i = 0; i < game_state->player_count; i++) {
        if (game_state->players[i].pid == me) { my_idx = (int)i; break; }
    }
    if (my_idx < 0) {
        fprintf(stderr, "No encontré mi pid en la lista de jugadores\n");
        exit(EXIT_FAILURE);
    }
}

static void attach_shared_mem() {
    int fd_state = shm_open(GAME_STATE_MEM, O_RDWR, 0666);
    if (fd_state < 0) die("shm_open game_state");

    game_state = mmap(NULL, sizeof(GameState), PROT_READ | PROT_WRITE, MAP_SHARED, fd_state, 0);
    if (game_state == MAP_FAILED) {
        die("mmap game_state");
    }
    close(fd_state);

    int fd_sync = shm_open(GAME_SYNC_MEM, O_RDWR, 0666);
    if (fd_sync < 0) {
        die("shm_open game_sync");
    }

    syncs = mmap(NULL, sizeof(GameSync), PROT_READ | PROT_WRITE, MAP_SHARED, fd_sync, 0);
    if (syncs == MAP_FAILED) {
        die("mmap game_sync");
    }
    close(fd_sync);
}


// bool is_valid_position(GameState *state, int x, int y) {
//     return x >= 0 && x < state->width && y >= 0 && y < state->height;
// }


// int get_board_cell(GameState *state, int x, int y) {
//     if (!is_valid_position(state, x, y)) {
//         return -1; // Valor inválido para posiciones fuera del tablero
//     }
//     return state->board[y * state->width + x];
// }

// bool is_cell_free(GameState *state, int x, int y) {
//     int cell_value = get_board_cell(state, x, y);
//     return cell_value >= 1 && cell_value <= 9;
// }

// void direction_to_offset(unsigned char dir, int *dx, int *dy) {
//     switch (dir) {
//         case 0: // arriba
//             *dx = 0;  *dy = -1; break;
//         case 1: // arriba-derecha
//             *dx = 1;  *dy = -1; break;
//         case 2: // derecha
//             *dx = 1;  *dy = 0;  break;
//         case 3: // abajo-derecha
//             *dx = 1;  *dy = 1;  break;
//         case 4: // abajo
//             *dx = 0;  *dy = 1;  break;
//         case 5: // abajo-izquierda
//             *dx = -1; *dy = 1;  break;
//         case 6: // izquierda
//             *dx = -1; *dy = 0;  break;
//         case 7: // arriba-izquierda
//             *dx = -1; *dy = -1; break;
//         default: // dirección inválida
//             *dx = 0; *dy = 0;   break;
//     }
// }


// unsigned char choose_move(Player *me, GameState *state); {
    
//     unsigned char best_direction = 0;
//     int best_reward = 0;
//     bool found_move = false;
    
//     // Evaluar todas las direcciones posibles
//     for (unsigned char dir = 0; dir < 8; dir++) {
//         int dx = 0, dy = 0;
//         direction_to_offset(dir, &dx, &dy);

//         int new_x = me->x + dx;
//         int new_y = me->y + dy;
        
//         if (is_cell_free(state, new_x, new_y)) {
//             int reward = get_board_cell(state, new_x, new_y);
//             if (!found_move || reward > best_reward) {
//                 best_direction = dir;
//                 best_reward = reward;
//                 found_move = true;
//             }
//         }
//     }
    
//     // Si no encontramos movimiento válido, decimos que esta bloqueado
//     if (!found_move) {
//         me->is_blocked = true;
//         return 0; 
//     }
    
//     return best_direction;
// }


