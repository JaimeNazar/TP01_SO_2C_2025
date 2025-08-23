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

// Color, experimental
#define GRASS_PAIR     1
#define EMPTY_PAIR     1
#define WATER_PAIR     2
#define MOUNTAIN_PAIR  3
#define PLAYER_PAIR_BASE 10 // Base para los colores de jugadores

typedef struct {
    WINDOW *window;
    int width, height;

    GameState *game_state;
    GameSync *game_sync;
} viewCDT;

typedef viewCDT* viewADT;

void view_init_ncurses(viewADT v) {
    
    // If the terminal isnt defined, set it
    if (!getenv("TERM"))
        setenv("TERM", "xterm-256color", 1);


    // TODO: Error check

    // Ncurses
    initscr();
    clear();
	noecho();
	cbreak(); // Line buffering disabled. pass on everything

    v->window = newwin(v->height, v->width, 0, 0);
    wrefresh(v->window);

    // Check color support
    if (!has_colors() ) {
        endwin();
        printf("Your terminal does not support color\n");
        exit(1);
    }

    // Color setup
    start_color();
    init_pair(GRASS_PAIR, COLOR_YELLOW, COLOR_GREEN);
    init_pair(WATER_PAIR, COLOR_CYAN, COLOR_BLUE);
    init_pair(MOUNTAIN_PAIR, COLOR_BLACK, COLOR_WHITE);
    // Colores para jugadores (hasta 6 jugadores, puedes agregar más si necesitas)
    init_pair(PLAYER_PAIR_BASE + 0, COLOR_RED, COLOR_BLACK);
    init_pair(PLAYER_PAIR_BASE + 1, COLOR_GREEN, COLOR_BLACK);
    init_pair(PLAYER_PAIR_BASE + 2, COLOR_YELLOW, COLOR_BLACK);
    init_pair(PLAYER_PAIR_BASE + 3, COLOR_BLUE, COLOR_BLACK);
    init_pair(PLAYER_PAIR_BASE + 4, COLOR_MAGENTA, COLOR_BLACK);
    init_pair(PLAYER_PAIR_BASE + 5, COLOR_CYAN, COLOR_BLACK);
}

// TODO: Agregar como libreria
void view_init_shm(viewADT v) {
    
    // TODO: Check FLAGS
    // Shared shmory
    int fd = shm_open(GAME_STATE_SHM, O_RDONLY, 0666);   // Open shared memory object
    if (fd == -1) {
        perror("Error SHM\n");
    }

    v->game_state = mmap(0, sizeof(GameState), PROT_READ, MAP_SHARED, fd, 0); // Memory map shared memory segment

    fd = shm_open(GAME_SYNC_SHM, O_RDWR, 0666); 
    if (fd == -1) {
        perror("Error SHM\n");
    }

    v->game_sync = mmap(0, sizeof(GameSync), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

}

void view_cleanup(viewADT v) {
    clrtoeol();
	wrefresh(v->window);

    if (v->window) {
        delwin(v->window);
        v->window = NULL;
    }

	endwin();   // Deallocate memory and end ncurses
}

void view_render(viewADT v) {
    for (int i = 0; i < v->height; i++) {
        for (int j = 0; j < v->width; j++) {
            int is_player = 0;
            unsigned int player_idx = 0;
            for (unsigned int p = 0; p < v->game_state->player_count; p++) {
                if (v->game_state->players[p].x == j && v->game_state->players[p].y == i) {
                    is_player = 1;
                    player_idx = p;
                    break;
                }
            }
            int value = v->game_state->board[i * v->width + j];
            int is_trail = 0;
            unsigned int trail_player = 0;
            if (!is_player && value < 0) {
                is_trail = 1;
                trail_player = (unsigned int)(-value);
            }
            if (is_player) {
                wattron(v->window, COLOR_PAIR(PLAYER_PAIR_BASE + player_idx % 6));
                mvwaddch(v->window, i, j, 'A' + player_idx);
                wattroff(v->window, COLOR_PAIR(PLAYER_PAIR_BASE + player_idx % 6));
            } else if (is_trail) {
                wattron(v->window, COLOR_PAIR(PLAYER_PAIR_BASE + trail_player % 6));
                mvwaddch(v->window, i, j, 'A' + trail_player);
                wattroff(v->window, COLOR_PAIR(PLAYER_PAIR_BASE + trail_player % 6));
            } else {
                mvwaddch(v->window, i, j, '0' + (value % 10));
            }
        }
    }
    wrefresh(v->window);
}

int main(int argc, char **argv) {

    if (argc < 3) {
        printf("Usage: ./view.out [width] [height]");

        return -1;
    }

    // Init view context
    viewCDT v = { 0 };

    v.width = atoi(argv[1]);
    v.height = atoi(argv[2]);

    view_init_ncurses(&v);
    view_init_shm(&v);

    // Main loop

    while(!v.game_state->finished) {
        // Wait on semaphore
        sem_wait(&(v.game_sync->state_change));

        view_render(&v);

		// Signal master
		sem_post(&(v.game_sync->render_done));
    }

    view_cleanup(&v);
    
	return 0;
}
