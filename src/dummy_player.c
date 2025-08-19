#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>       
#include <fcntl.h>  
#include <string.h>
#include <time.h>
#include <stdio.h>

#include "common.h"

int main(void) {

    // Player *player = NULL;

    // int fd = shm_open("/player", O_RDONLY, 0666);   // Open shared memory objetc
    // player = mmap(0, sizeof(Player), PROT_READ, MAP_SHARED, fd, 0); // Memory map shared memory segment

    GameSync *game_sync = NULL;

    int fd = shm_open("/game_sync", O_RDONLY, 0666);   // Open shared memory objetc
    game_sync = mmap(0, sizeof(GameSync), PROT_READ, MAP_SHARED, fd, 0); // Memory map shared memory segment

    //strcpy(player->name, "pepe");

    srand(time(NULL));
    
    printf("Player process\n");

    while(1) {
        
        sem_wait(&game_sync->can_move[0]);
        sem_post(&game_sync->game_state_busy);

        printf("Player can move\n");
		putchar('0' + rand() % 7);

        sem_post(&game_sync->m_can_access);
    }
}
