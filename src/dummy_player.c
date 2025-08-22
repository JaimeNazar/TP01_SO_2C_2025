#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>       
#include <fcntl.h>  
#include <string.h>
#include <time.h>
#include <stdio.h>

#include "common.h"

int main(void) {

    GameState *game_state = NULL;
    GameSync *game_sync = NULL;

    // TODO: Check FLAGS
    // Shared memory
    int fd = shm_open(GAME_STATE_MEM, O_CREAT | O_RDWR, 0666);   // Open shared memory object
    game_state = mmap(0, sizeof(GameState), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0); // Memory map shared memory segment

    fd = shm_open(GAME_SYNC_MEM, O_CREAT | O_RDWR, 0666); 
    game_sync = mmap(0, sizeof(GameSync), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    //strcpy(player->name, "pepe");

    srand(time(NULL));

    Player *p = &game_state->player_list[0];

    unsigned char move = '1';

    while(!game_state->is_finished) {
        //sem_post(&game_sync->game_state_busy);

        sem_wait(&game_sync->can_move[0]);

		printf("%c", move);

        if (p->is_blocked)
		    printf("%u", 2);

        sem_post(&game_sync->next_var);

       // sem_post(&game_sync->m_can_access);
    }

    return 0;
}
