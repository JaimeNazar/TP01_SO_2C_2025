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
#define TABLE_WIDTH 55
#define AUX_HEIGHT 5

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

    v->window = newwin(v->height <= 14 ? (v->height + AUX_HEIGHT) : v->height, v->width + TABLE_WIDTH, 0, 0); // TABLE_WIDTH columnas extra para info
    
    // v->window = newwin((v->height <= 14)? v->height + AUX_HEIGHT : v->height, v->width + TABLE_WIDTH, 0, 0); // TABLE_WIDTH columnas extra para info

    wrefresh(v->window);

    // Check color support
    if (!has_colors() ) {
        endwin();
        printf("Your terminal does not support color\n");
        exit(1);
    }

    // Color setup
    start_color();
    // Colores para jugadores (más oscuros)
    init_pair(PLAYER_PAIR_BASE + 0, COLOR_BLACK, COLOR_RED);      // Jugador 1
    init_pair(PLAYER_PAIR_BASE + 1, COLOR_BLACK, COLOR_GREEN);    // Jugador 2
    init_pair(PLAYER_PAIR_BASE + 2, COLOR_BLACK, COLOR_YELLOW);   // Jugador 3
    init_pair(PLAYER_PAIR_BASE + 3, COLOR_BLACK, COLOR_BLUE);     // Jugador 4
    init_pair(PLAYER_PAIR_BASE + 4, COLOR_BLACK, COLOR_MAGENTA);  // Jugador 5
    init_pair(PLAYER_PAIR_BASE + 5, COLOR_BLACK, COLOR_CYAN);     // Jugador 6
    init_pair(PLAYER_PAIR_BASE + 6, COLOR_BLACK, COLOR_WHITE);    // Jugador 7
    init_pair(PLAYER_PAIR_BASE + 7, COLOR_BLACK, COLOR_YELLOW);   // Jugador 8
    init_pair(PLAYER_PAIR_BASE + 8, COLOR_BLACK, COLOR_GREEN);    // Jugador 9
    init_pair(PLAYER_PAIR_BASE + 9, COLOR_BLACK, COLOR_BLUE);     // Jugador 10
    // Colores para paths (más claros)
    init_pair(PLAYER_PAIR_BASE + 20 + 0, COLOR_WHITE, COLOR_RED);      // Path Jugador 1
    init_pair(PLAYER_PAIR_BASE + 20 + 1, COLOR_WHITE, COLOR_GREEN);    // Path Jugador 2
    init_pair(PLAYER_PAIR_BASE + 20 + 2, COLOR_WHITE, COLOR_YELLOW);   // Path Jugador 3
    init_pair(PLAYER_PAIR_BASE + 20 + 3, COLOR_WHITE, COLOR_BLUE);     // Path Jugador 4
    init_pair(PLAYER_PAIR_BASE + 20 + 4, COLOR_WHITE, COLOR_MAGENTA);  // Path Jugador 5
    init_pair(PLAYER_PAIR_BASE + 20 + 5, COLOR_WHITE, COLOR_CYAN);     // Path Jugador 6
    init_pair(PLAYER_PAIR_BASE + 20 + 6, COLOR_WHITE, COLOR_WHITE);    // Path Jugador 7
    init_pair(PLAYER_PAIR_BASE + 20 + 7, COLOR_WHITE, COLOR_YELLOW);   // Path Jugador 8
    init_pair(PLAYER_PAIR_BASE + 20 + 8, COLOR_WHITE, COLOR_GREEN);    // Path Jugador 9
    init_pair(PLAYER_PAIR_BASE + 20 + 9, COLOR_WHITE, COLOR_BLUE);     // Path Jugador 10
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
            if (!is_player) {
                if (value < 0){
                    is_trail = 1;
                    trail_player = (unsigned int) ((-value)); // Ajuste para que A=0, B=1, etc.
                } else if (value == 0) {
                    is_trail = 1;
                    trail_player = 0;
                }
            }

            //Muestra tablero de datos
            
            // Título de la tabla
            const char *title = "Chomp Champs - Tabla de Jugadores";
            int title_length = strlen(title);
            int title_start_col = v->width + (TABLE_WIDTH - title_length) / 2;
            mvwprintw(v->window, 1, title_start_col + 1, "%s", title);
            mvwhline(v->window, 2, v->width + 2, '-', TABLE_WIDTH - 2);

            // Encabezados de la tabla
            mvwprintw(v->window, 3, v->width + 2, "%-10s | %-10s | %-10s | %-10s", "Jugador", "Puntaje", "Movimientos", "Inválidos");
            mvwhline(v->window, 4, v->width + 2, '-', TABLE_WIDTH - 2);

            // Datos de los jugadores
            for (size_t i = 0; i < v->game_state->player_count; i++) {
            mvwprintw(v->window, 5 + (int)i, v->width + 2, "%-10c | %-10d | %-10d | %-10d", 'A' + (int)i, v->game_state->players[i].score, v->game_state->players[i].valid_reqs, v->game_state->players[i].invalid_reqs);
            }
            wrefresh(v->window);


            if (is_player) {
                wattron(v->window, COLOR_PAIR(PLAYER_PAIR_BASE + player_idx % 6));
                mvwaddch(v->window, i + 1, j + 1, 'A' + player_idx);
                wattroff(v->window, COLOR_PAIR(PLAYER_PAIR_BASE + player_idx % 6));
            } else if (is_trail) {
                wattron(v->window, COLOR_PAIR(PLAYER_PAIR_BASE + 20 + (trail_player % 6)));
                mvwaddch(v->window, i + 1, j + 1, 'A' + trail_player);
                wattroff(v->window, COLOR_PAIR(PLAYER_PAIR_BASE + 20 + (trail_player % 6)));
            } else {
                mvwaddch(v->window, i + 1, j + 1, '0' + (value % 10));
            }
        }
    }
    wborder(v->window, '|', '|', '-', '-', '+', '+', '+', '+');
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
