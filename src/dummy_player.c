// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
     
#include "playerADT.h"
#include <stdlib.h>

int main(int argc, char **argv) {

    srand((unsigned int)time(NULL));

    PlayerADT p = init_player(argc, argv);

    if (p == NULL)
        return -1;

    if(init_shm(p) == -1){
        return -1;
    }

	while (1) {

        // Guardar estado actual
        get_state_snapshot(p);

        // Verificar si el juego terminó o si estamos bloqueados
        if (!still_playing(p)) {
            break;
        }
        
        // Elegir y enviar movimiento (aleatorio)
        unsigned char move =  rand() % 7;

        send_movement(p, move);

    }
 
    return 0;
}