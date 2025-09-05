     
#include "player_common.h"

int main(int argc, char* argv[]) {

    if (argc < 3) {
        printf("Usage: ./player.out [width] [height]");

        return -1;
    }

	
    unsigned short width = atoi(argv[1]);
    unsigned short height = atoi(argv[2]);	// TODO: Esto esta repetido tmb en la vista

    PlayerADT p = init_player(width, height);

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
