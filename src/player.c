#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>
#include <semaphore.h>
#include <signal.h>
#include <assert.h>
#include "common.h"

#ifndef GAME_STATE_MEM
#define GAME_STATE_MEM "/game_state"
#endif
#ifndef GAME_SYNC_MEM
#define GAME_SYNC_MEM  "/game_sync"
#endif

static GameState *game_state = NULL;
static GameSync  *syncs = NULL;
static  int my_idx = -1;

static void die(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

static void find_me() {// para saber quien sooy me busco
    pid_t me = getpid();
    my_idx = -1;
    for (unsigned i = 0; i < game_state->player_count; i++) {
        if (game_state->players[i].pid == me) { my_idx = (int)i; break; }
    }
    if (my_idx < 0) {
        fprintf(stderr, "No encontré mi pid en la lista de jugadores\n");
        exit(EXIT_FAILURE);
    }
}

static void attach_shared_mem() {
    int fd_state = shm_open(GAME_STATE_MEM, O_RDWR, 0666);
    if (fd_state < 0) die("shm_open game_state");

    game_state = mmap(NULL, sizeof(GameState), PROT_READ | PROT_WRITE, MAP_SHARED, fd_state, 0);
    if (game_state == MAP_FAILED) {
        die("mmap game_state");
    }
    close(fd_state);

    int fd_sync = shm_open(GAME_SYNC_MEM, O_RDWR, 0666);
    if (fd_sync < 0) {
        die("shm_open game_sync");
    }

    syncs = mmap(NULL, sizeof(GameSync), PROT_READ | PROT_WRITE, MAP_SHARED, fd_sync, 0);
    if (syncs == MAP_FAILED) {
        die("mmap game_sync");
    }
    close(fd_sync);
}

int main(void){

    attach_shared_mem();
    
    find_me();

    Player *me = &game_state->players[my_idx];
    me->pid = getpid();
    strncpy(me->name, "teofum", sizeof(me->name) - 1);//CAMBIAR X DIOS
    me->score = 0;
    me->invalid_reqs = 0;
    me->valid_reqs = 0;
    me->x = 0;
    me->y = 0;
    me->is_blocked = false;


    srand(time(NULL));

    printf("Player process\n");

    while(1) {

        sem_wait(&game_sync->can_move[0]);
        sem_post(&game_sync->game_state_busy);

        printf("Player can move\n");
        putchar('0' + rand() % 7);

        sem_post(&game_sync->m_can_access);
    }

    return 0;

}

