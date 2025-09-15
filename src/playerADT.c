// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "playerADT.h"

struct PlayerCDT {
	GameState* game_state;
	GameSync* game_sync;
	unsigned short x, y, width, height;
	int id;								// Game state values
	char blocked;
	char game_finished;
	int board[];
};

PlayerADT init_player(int argc, char **argv) {

    if (argc < 3) {
        printf("Usage: ./player.out [width] [height]");

        return NULL;
    }

    unsigned int width = atoi(argv[1]);
    unsigned int height = atoi(argv[2]);

	PlayerADT p = malloc(sizeof(struct PlayerCDT) + sizeof(int) * width * height);
    if (p == NULL) {
        fprintf(stderr, "Error: no se pudo asignar memoria para PlayerADT\n");
        return NULL;
    }
	p->width = width;
	p->height = height;
	p->blocked = 0;
	p->id = -1;

	return p;
}

int init_shm(PlayerADT p) {

    p->game_state = open_game_state(p->width, p->height);
    p->game_sync = open_game_sync();

	reader_enter(p->game_sync);
	// Chequear id de la game state
	if (p->id == -1) {
		// Obtenerlo
		pid_t my_pid = getpid();
		for (unsigned int i = 0; p->id < 0 && i < p->game_state->player_count; i++) {
			if (p->game_state->players[i].pid == my_pid) {
				p->id = i;
			}
		}
	}
	reader_leave(p->game_sync);

	return 0;
}

void get_state_snapshot(PlayerADT p) {

	reader_enter(p->game_sync);
	
	p->x = p->game_state->players[p->id].x;
	p->y = p->game_state->players[p->id].y;
	p->blocked = p->game_state->players[p->id].blocked;
	p->game_finished = p->game_state->finished;
	
	// TODO: Usar memcpy
	for (int i = 0; i < p->height; i++) {
		for (int j = 0; j < p->width; j++) {
			p->board[i*p->width + j] = p->game_state->board[i*p->width + j];
		}
	}

	reader_leave(p->game_sync);
}

bool still_playing(PlayerADT p) {
	return !p->game_finished || !p->blocked;
}

unsigned int get_x(PlayerADT p) {
    return p->x;
}

unsigned int get_y(PlayerADT p) {
    return p->y;
}

unsigned int get_width(PlayerADT p) {
    return p->width;
}

unsigned int get_height(PlayerADT p) {
    return p->height;
}

int get_id(PlayerADT p) {
    return p->id;
}


unsigned int get_player_count(PlayerADT p) {
    if (p == NULL || p->game_state == NULL)
        return 0;
    return p->game_state->player_count;
}


int send_movement(PlayerADT p, unsigned char move) {

    // Esperar permiso para enviar movimiento
    sem_wait(&p->game_sync->player_can_move[p->id]);

    if (write(STDOUT_FILENO, &move, 1) != 1) {
        perror("PLAYER::SEND_MOVEMENT: Write error");
            
		return -1;
    }

	return 0;
}

// posicion dentro del tablero
bool is_valid_position(PlayerADT p, int x, int y) {
    return x >= 0 && x < p->width && y >= 0 && y < p->height;
}

//valor de la posicion del tablero
int get_board_cell(PlayerADT p, int x, int y) {
    if (!is_valid_position(p, x, y)) {
        return -1; // Valor inválido para posiciones fuera del tablero
    }
    return p->board[y * p->width + x];
}

// posicion dentro del tablero y libre
bool is_cell_free(PlayerADT p, int x, int y) {
    int cell_value = get_board_cell(p, x, y);
    return cell_value >= 1 && cell_value <= 9;
}

void direction_to_offset(unsigned char dir, int *dx, int *dy) {
    switch (dir) {
        case 0: // arriba
            *dx = 0;  *dy = -1; break;
        case 1: // arriba-derecha
            *dx = 1;  *dy = -1; break;
        case 2: // derecha
            *dx = 1;  *dy = 0;  break;
        case 3: // abajo-derecha
            *dx = 1;  *dy = 1;  break;
        case 4: // abajo
            *dx = 0;  *dy = 1;  break;
        case 5: // abajo-izquierda
            *dx = -1; *dy = 1;  break;
        case 6: // izquierda
            *dx = -1; *dy = 0;  break;
        case 7: // arriba-izquierda
            *dx = -1; *dy = -1; break;
        default: // dirección inválida
            *dx = 0; *dy = 0;   break;
    }
}

int count_free_neighbors(PlayerADT p, int x, int y) {
    int count = 0;
    for (unsigned char dir = 0; dir < 8; dir++) {
        int dx = 0, dy = 0;
        direction_to_offset(dir, &dx, &dy);
        int nx = x + dx;
        int ny = y + dy;
        if (is_cell_free(p, nx, ny)) {
            count++;
        }
    }
    return count;
}

int calculate_depth(PlayerADT p, int dx, int dy, int max_depth) {
    int depth = 0;
    int x = p->x + dx;
    int y = p->y + dy;

    while (depth < max_depth && is_cell_free(p, x, y)) {
        depth++;
        x += dx;
        y += dy;
    }

    return depth;
}

bool has_nearby_players(PlayerADT p, int x, int y, int my_id) {
    
    reader_enter(p->game_sync);
    for (int i = 0; (unsigned int) i < p->game_state->player_count; i++) {
        if (i == my_id || p->game_state->players[i].blocked) continue;

        int player_x = p->game_state->players[i].x;
        int player_y = p->game_state->players[i].y;

        int dx = abs(x - player_x);
        int dy = abs(y - player_y);

        if (dx <= 2 && dy <= 2 && (dx + dy) <= 3) {
            reader_leave(p->game_sync);
            return true;
        }

    }

    reader_leave(p->game_sync);
    return false;
}

bool is_potential_trap(PlayerADT p, int x, int y) {
    int free_neighbors = count_free_neighbors(p, x, y);

    if (free_neighbors == 1) {
        for (unsigned char dir = 0; dir < 8; dir++) {
            int dx = 0, dy = 0;
            direction_to_offset(dir, &dx, &dy);
            int nx = x + dx;
            int ny = y + dy;

            if (is_cell_free(p, nx, ny)) {
                int next_neighbors = count_free_neighbors(p, nx, ny);
                return next_neighbors <= 2;
            }
        }
    }

    return free_neighbors == 0;
}

bool is_endgame(PlayerADT p) {
    int total_cells = p->width * p->height;
    int free_cells = 0;
    
    for (int i = 0; i < total_cells; i++) {
        if (p->board[i] >= 1 && p->board[i] <= 9) {
            free_cells++;
        }
    }

    return (free_cells * 100 / total_cells) < 20; // Si menos del 20% del tablero está libre, es endgame
}






