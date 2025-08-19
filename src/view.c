#include <stdlib.h>
#include <ncurses.h>
#include <sys/mman.h>
#include <sys/stat.h>       
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdio.h>

#include <unistd.h>
#include <pty.h>
#include <sys/ioctl.h>
#include <stdlib.h>

#include "common.h"

#define GAME_STATE_MEM "/game_state"
#define GAME_SYNC_MEM "/game_sync"

WINDOW *window = NULL;
int width, height;

GameState *game_state = NULL;
GameSync *game_sync = NULL;

void view_init(void) {
    
    // If the terminal isnt defined, set it
    if (!getenv("TERM"))
        setenv("TERM", "xterm-256color", 1);

    // TODO: Error check

    // Ncurses
    initscr();
    clear();
	noecho();
	cbreak(); // Line buffering disabled. pass on everything

    window = newwin(height, width, 0, 0);
    wrefresh(window);

    // TODO: Check FLAGS
    // Shared memory
    int fd = shm_open(GAME_STATE_MEM, O_CREAT | O_RDWR, 0666);   // Open shared memory object
    game_state = mmap(0, sizeof(GameState), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0); // Memory map shared memory segment

    fd = shm_open(GAME_SYNC_MEM, O_CREAT | O_RDWR, 0666); 
    game_sync = mmap(0, sizeof(GameSync), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

}

void view_cleanup(void) {
    clrtoeol();
	wrefresh(window);

    if (window) {
        delwin(window);
        window = NULL;
    }

	endwin();   // Deallocate memory and end ncurses
}

void view_render(void) {


    // Render board
    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            mvwaddch(window, i, j, (char)(game_state->board[i * width + j])); 
        }
    }

	wrefresh(window);
}

int main(int argc, char **argv) {

    // TODO: Recibir ancho y alto por parametros

    width = atoi(argv[1]);
    height = atoi(argv[2]);

    printf("View: width %d; height %d\n", width, height);
    
    view_init();

    while(!game_state->is_finished) {
        // Wait on semaphore
        sem_wait(&(game_sync->state_change));

        view_render();

		// Signal master
		sem_post(&(game_sync->render_done));
    }

    view_cleanup();
    
	return 0;
}
