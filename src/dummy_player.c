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
       
		reader_enter(game_sync);
		
        // Elegir y enviar movimiento 
        unsigned char move = rand() % 7;

		// Verificar si el juego terminó o si estamos bloqueados
        if (game_state->finished || p->blocked) {
		    reader_leave(game_sync);
            break;
        }
        
        if (write(STDOUT_FILENO, &move, 1) != 1) {
            perror("write move");
            break;
        }
        
		reader_leave(game_sync);

 
    }
 
    return 0;
}
