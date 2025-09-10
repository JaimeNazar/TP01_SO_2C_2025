
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <unistd.h>
#include "common.h"
#include "playerADT.h"

#define REWARD_WEIGHT 10
#define FREE_NEIGHBORS_WEIGHT 2
#define DEAD_END_WEIGHT -30

unsigned char choose_move(PlayerADT p) {
    unsigned char best_direction = 0;
    int best_score = -1000000;
    bool found_move = false;

    // Evaluar todas las direcciones posibles
    for (unsigned char dir = 0; dir < 8; dir++) {
        int dx = 0, dy = 0;
        direction_to_offset(dir, &dx, &dy);

        int new_x = get_x(p) + dx;
        int new_y = get_y(p) + dy;

        if (is_cell_free(p, new_x, new_y)) {
            int reward = get_board_cell(p, new_x, new_y);
            int free_neighbors = count_free_neighbors(p, new_x, new_y);

            int dead_end_penalty = (free_neighbors == 0 ? 1 : 0) ;

            // Score teniendo en cuenta mayor puntaje, celdas libres cercanas y que no sea dead ends
            int score = reward * REWARD_WEIGHT + free_neighbors * FREE_NEIGHBORS_WEIGHT + dead_end_penalty * DEAD_END_WEIGHT;

            if (!found_move || score > best_score) {

                best_direction = dir;
                best_score = score;
                found_move = true;

            }
        }
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

        // Elegir y enviar movimiento
        unsigned char move = choose_move(p);
        send_movement(p, move);

    }

    return 0;
}
