#include "common.h"

// Color, experimental
#define GRASS_PAIR     1
#define EMPTY_PAIR     1
#define WATER_PAIR     2
#define MOUNTAIN_PAIR  3
#define PLAYER_PAIR_BASE 10 // Base para los colores de jugadores
#define TABLE_WIDTH 55
#define AUX_HEIGHT 5
#define FRAME 2

typedef struct {
    WINDOW *window;
    int width, height;
    char finished;
	unsigned int player_count;	// No va a cambiar mucho, conviene tener una copia
    GameState *game_state;
    GameSync *game_sync;
} viewCDT;

typedef viewCDT* viewADT;

void view_init_ncurses(viewADT v) {
    
    // si la terminal no esta definida, hacerlo
    if (!getenv("TERM"))
        setenv("TERM", "xterm-256color", 1);

    // Ncurses
    initscr();
    clear();
	noecho();
	cbreak(); // Line buffering disabled. pass on everything

    // TABLE_WIDTH columnas extra para la info de la tabla y FRAME para el marco
    v->window = newwin((v->height <= 14 ? (v->height + AUX_HEIGHT) : v->height) + FRAME, v->width + TABLE_WIDTH + FRAME, 0, 0); 

    wrefresh(v->window);

    // checkear si soporta color
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

void view_init_shm(viewADT v) {

    v->game_state = open_game_state(v->width, v->height);
	v->game_sync = open_game_sync();

	// Obtener el count
	reader_enter(v->game_sync);
	v->player_count = v->game_state->player_count;
	reader_leave(v->game_sync);
}

void view_cleanup(viewADT v) {
    
    if (v->window) {
        wclrtoeol(v->window);
        wrefresh(v->window);

        delwin(v->window);
        v->window = NULL;
    }

	endwin();   //desasignar memoria y terminar ncurses
}

void view_render(viewADT v) {
    for (int i = 0; i < v->height; i++) {
        for (int j = 0; j < v->width; j++) {
            
			reader_enter(v->game_sync);
            int value = v->game_state->board[i * v->width + j];
			reader_leave(v->game_sync);

            // Muestra tablero de datos
            
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
            for (size_t i = 0; i < v->player_count; i++) {
				reader_enter(v->game_sync);

				mvwprintw(v->window, 5 + (int)i, 
						v->width + 2, 
						"%-10s | %-10d | %-10d | %-10d", 
						v->game_state->players[i].name, 
                        v->game_state->players[i].score, 
						v->game_state->players[i].valid_reqs, 
						v->game_state->players[i].invalid_reqs);

				reader_leave(v->game_sync);
            }

            wrefresh(v->window);

			// Si es un jugador, marcarlo
            if (value <= 0) {
                wattron(v->window, COLOR_PAIR(PLAYER_PAIR_BASE + 20 + ((-value) % 6)));
                mvwaddch(v->window, i + 1, j + 1, 'A' + (-value));
                wattroff(v->window, COLOR_PAIR(PLAYER_PAIR_BASE + 20 + ((-value) % 6)));
            } else {
                mvwaddch(v->window, i + 1, j + 1, '0' + (value % 10));
            }
        }
    }

	// Agregar las cabezas de los jugadores
	unsigned int x, y;
	for (unsigned int i = 0; i < v->player_count; i++) {
		reader_enter(v->game_sync);
		x = v->game_state->players[i].x;
		y = v->game_state->players[i].y;
		reader_leave(v->game_sync);

		// Dibujarlo
        wattron(v->window, COLOR_PAIR(PLAYER_PAIR_BASE + i % 6));
        mvwaddch(v->window, y + 1, x + 1, 'A' + i);
        wattroff(v->window, COLOR_PAIR(PLAYER_PAIR_BASE + i % 6));
 	}

    wborder(v->window, '|', '|', '-', '-', '+', '+', '+', '+');
    wrefresh(v->window);
}

static void view_update(viewADT v) {
    reader_enter(v->game_sync);

    v->finished = v->game_state->finished;

    reader_leave(v->game_sync);
}

int main(int argc, char **argv) {

    if (argc < 3) {
        printf("Usage: ./view.out [width] [height]");

        return -1;
    }

    // inicializa el contexto de la view
    viewCDT v = { 0 };

    v.width = atoi(argv[1]);
    v.height = atoi(argv[2]);
    v.finished = 0;

    view_init_ncurses(&v);
    view_init_shm(&v);

    // Main loop

    while(!v.finished) {
        // espera el semaforo
        sem_wait(&(v.game_sync->state_change));

        view_update(&v);

        view_render(&v);

		// Signal master
		sem_post(&(v.game_sync->render_done));
    }

    view_cleanup(&v);
    
	return 0;
}
