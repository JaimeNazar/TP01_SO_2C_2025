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

WINDOW *window = NULL;
int width, height;

GameState *game_state = NULL;
GameSync *game_sync = NULL;

void view_init_ncurses(void) {
    
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

}

// TODO: Agregar como libreria
void view_init_shmem(void) {
    
    // TODO: Check FLAGS
    // Shared memory
    int fd = shm_open(GAME_STATE_MEM, O_RDONLY, 0666);   // Open shared memory object
    if (fd == -1) {
        perror("Error SHM\n");
    }

    game_state = mmap(0, sizeof(GameState) + width*height*sizeof(int), PROT_READ, MAP_SHARED, fd, 0); // Memory map shared memory segment

    fd = shm_open(GAME_SYNC_MEM, O_RDWR, 0666); 
    if (fd == -1) {
        perror("Error SHM\n");
    }

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

	waddstr(window, "This was printed using addstr\n\n");
    
	wrefresh(window);
}

// static void print_sem(void) {
//     int status = 0;

//     sem_getvalue(&game_sync->state_change, &status);
//     printf("State change: %d\n", status);

//     sem_getvalue(&game_sync->render_done, &status);
//     printf("Render done: %d\n", status);

//     sem_getvalue(&game_sync->m_can_access, &status);
//     printf("Master can access: %d\n", status);

//     sem_getvalue(&game_sync->game_state_busy, &status);
//     printf("Game state busy: %d\n", status);

//     sem_getvalue(&game_sync->next_var, &status);
//     printf("Next var: %d\n", status);

//     printf("Can move: ");

//     for (int i = 0; i < 9; i++) {
//         sem_getvalue(&game_sync->can_move[i], &status);
//         printf("%d ", status);
//     }

//     putchar('\n');
// }

// static void print_player(void) {
//     Player *p = &game_state->player_list[0];

//     printf("Is blocked: %d\n", p->is_blocked);
//     printf("Valid requests: %d\n", p->valid_reqs);
//     printf("Invalid requests: %d\n", p->invalid_reqs);

// }

int main(int argc, char **argv) {

    // TODO: Check arguments

    width = atoi(argv[1]);
    height = atoi(argv[2]);

    //view_init_ncurses();
    view_init_shmem();

    while(!game_state->finished) {
        // Wait on semaphore
        sem_wait(&(game_sync->state_change));

        //view_render();

		// Signal master
		sem_post(&(game_sync->render_done));
    }

    view_cleanup();
    
	return 0;
}
