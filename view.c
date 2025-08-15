#include <stdlib.h>
#include <ncurses.h>
#include <sys/mman.h>
#include <sys/stat.h>       
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdio.h>


#include "common.h"

#define GAME_STATE_MEM "/game_state"
#define GAME_SYNC_MEM "/game_sync"

WINDOW *window = NULL;
int width, height;

Player *game_state = NULL;
GameSync *game_sync = NULL;

void view_init(int width, int height) {

    // TODO: Error check

    // Ncurses
    // initscr();
	// clear();
	// noecho();
	// cbreak(); // Line buffering disabled. pass on everything

    // window = newwin(height, width, 0, 0);
    // wrefresh(window);

    // Shared memory
    printf("Initializing view... \n");

    // NOTE: GameState is right after Player in shared memory
    int fd = shm_open(GAME_STATE_MEM, O_RDONLY, 0666);   // Open shared memory objetc
    game_state = mmap(0, sizeof(Player), PROT_READ, MAP_SHARED, fd, 0); // Memory map shared memory segment
    
    fd = shm_open(GAME_SYNC_MEM, O_RDONLY, 0666);   // Open shared memory objetc
    game_sync = mmap(0, sizeof(GameSync), PROT_READ, MAP_SHARED, fd, 0); // Memory map shared memory segment

    printf("DATA: %s", game_state->name);

}

void view_cleanup() {
    clrtoeol();
	wrefresh(window);

    if (window) {
        delwin(window);
        window = NULL;
    }

	endwin();   // Deallocate memory and end ncurses
    free(window);
}

void view_render() {

    // Render board
    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
           // mvwaddch(window, i, j, (char)(game_state->board[i * width + j]));
        }
    }

	wrefresh(window);
}

int main(int argc, char **argv) {

    // TODO: Recibir ancho y alto por parametros

    printf(argv[1]);
    view_init(20, 20);
    

    while(1) {
        // Wait on semaphore  
        sem_wait(&game_sync->state_change);

        view_render();

		// Signal master
		sem_post(&game_sync->render_done);
    }

    view_cleanup();
    
	return 0;
}
