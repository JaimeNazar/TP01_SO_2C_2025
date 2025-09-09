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

unsigned char choose_move(Player *me, GameState *state) {
    
    unsigned char best_direction = 0;
    int best_reward = 0;
    bool found_move = false;
    
    // Evaluar todas las direcciones posibles
    for (unsigned char dir = 0; dir < 8; dir++) {
        int dx = 0, dy = 0;
        direction_to_offset(dir, &dx, &dy);

        int new_x = me->x + dx;
        int new_y = me->y + dy;
        
        if (is_cell_free(state, new_x, new_y)) {
            int reward = get_board_cell(state, new_x, new_y);
            if (!found_move || reward > best_reward) {
                best_direction = dir;
                best_reward = reward;
                found_move = true;
            }
        }
    }
    
    // Si no encontramos movimiento válido, decimos que esta bloqueado
    if (!found_move) {
        me->blocked = true;
        return 0; 
    }
    
    return best_direction;
}

int main(int argc, char **argv) {

    PlayerADT p = init_player(argc, argv);

    // TODO: Error handling
    init_shm(p);

	while (1) {

        // Guardar estado actual
        get_state_snapshot(p);

        send_movement(p, 3);
       
        // Elegir y enviar movimiento
        //unsigned char move = choose_move(p, game_state);

		// Verificar si el juego terminó o si estamos bloqueados
        // if (game_state->finished || p->blocked) {
		//     reader_leave(game_sync);
        //     return -1;
        // }
        

    }
 
    return 0;
}
