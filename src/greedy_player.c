// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "playerADT.h"

unsigned char choose_move(PlayerADT p) {
    
    unsigned char best_direction = 0;
    int best_reward = 0;
    bool found_move = false;
    
    // Evaluar todas las direcciones posibles
    for (unsigned char dir = 0; dir < 8; dir++) {
        int dx = 0, dy = 0;
        direction_to_offset(dir, &dx, &dy);

        int new_x = get_x(p) + dx;
        int new_y = get_y(p) + dy;
        
        if (is_cell_free(p, new_x, new_y)) {
            int reward = get_board_cell(p, new_x, new_y);
            if (!found_move || reward > best_reward) {
                best_direction = dir;
                best_reward = reward;
                found_move = true;
            }
        }
    }
    
    return best_direction;
}

int main(int argc, char **argv) {

    PlayerADT p = init_player(argc, argv);

    if (p == NULL) {
        free(p);
        return -1;
    }
    if(init_shm(p) == -1){
        free(p);
        return -1;
    }

	while (1) {

        // Guardar estado actual
        get_state_snapshot(p);

        // Verificar si el juego terminó o si estamos bloqueados
        if (!still_playing(p)) {
            break;
        }
        
        // Elegir y enviar movimiento
        unsigned char move = choose_move(p);

        send_movement(p, move);

    }
    free(p);
    return 0;
}
