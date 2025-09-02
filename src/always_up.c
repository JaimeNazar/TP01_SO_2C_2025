#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <semaphore.h>
#include <errno.h>
#include "common.h"

#define MOVE_UP 0
#define MOVE_DOWN 4

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Uso: %s <width> <height>\n", argv[0]);
        return 1;
    }
    int width = atoi(argv[1]);
    int height = atoi(argv[2]);
    if (width <= 0 || height <= 0) return 1;

    // /game_state: SOLO LECTURA
    int fd_state = shm_open(GAME_STATE_SHM, O_RDONLY, 0);
    if (fd_state < 0) { perror("shm_open(GAME_STATE_SHM)"); return 1; }

    size_t state_size = sizeof(GameState) + (size_t)width * (size_t)height * sizeof(int);
    GameState *state = mmap(NULL, state_size, PROT_READ, MAP_SHARED, fd_state, 0);
    if (state == MAP_FAILED) { perror("mmap /game_state"); return 1; }

    // /game_sync: LECTURA/ESCRITURA (para semáforos)
    int fd_sync = shm_open(GAME_SYNC_SHM, O_RDWR, 0);
    if (fd_sync < 0) { perror("shm_open(GAME_SYNC_SHM)"); return 1; }

    GameSync *sync = mmap(NULL, sizeof(GameSync), PROT_READ | PROT_WRITE, MAP_SHARED, fd_sync, 0);
    if (sync == MAP_FAILED) { perror("mmap /game_sync"); return 1; }

    // Encontrar mi índice por PID
    pid_t my_pid = getpid();
    int my_index = -1;
    for (unsigned int i = 0; i < state->player_count; i++) {
        if (state->players[i].pid == my_pid) { my_index = (int)i; break; }
    }
    if (my_index < 0) {
        fprintf(stderr, "No encontré mi índice en GameState (pid=%d)\n", (int)my_pid);
        return 1;
    }

    // Loop principal: esperar permiso y mandar 0 (arriba)
    while (!state->finished) {
        if (sem_wait(&sync->player_can_move[my_index]) == -1) {
            if (errno == EINTR) continue;
            perror("sem_wait(player_can_move)");
            break;
        }
        unsigned char move = MOVE_DOWN;
        ssize_t w = write(STDOUT_FILENO, &move, sizeof(move));
        if (w != (ssize_t)sizeof(move)) { perror("write(move)"); break; }
    }

    return 0;
}
