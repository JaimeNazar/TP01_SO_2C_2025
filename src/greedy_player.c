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
