     
#include "playerADT.h"

int main(int argc, char* argv[]) {

    PlayerADT p = init_player(argc, argv);

    srand(time(NULL));

	while (1) {
      
		get_state_snapshot(p);
		
        // Elegir y enviar movimiento 
        unsigned char move = rand() % 7;

		// Verificar si el juego terminó o si estamos bloqueados
        if (!still_playing(p)) {
            break;
        }

		send_movement(p, move);
        
    }
 
    return 0;
}
