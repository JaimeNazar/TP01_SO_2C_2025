#include <stdlib.h>
#include "include/player_common.h"
#include "include/common.h"

struct PlayerCDT {
	GameState* game_state;
	GameSync* game_sync;
	unsigned short x, y, width, height;
	int id;								// Game state values
	char blocked;
	char game_finished;
	int board[];	// Demasiado? Copiar todo el tablero?
};

PlayerADT init_player(unsigned short width, unsigned short height) {
	PlayerADT p = malloc(sizeof(struct PlayerCDT) + sizeof(int) * width * height);
	p->width = width;
	p->height = height;
	p->blocked = 0;
	p->id = -1;

	return p;
}

int init_shm(PlayerADT p) {

	// Game state
    int fd = shm_open(GAME_STATE_SHM, O_RDONLY, 0666);   // Open shared memory object
    if (fd == -1) {
        perror("PLAYER::INIT_SHM: Error game state\n");
		return -1;
    }
    p->game_state = mmap(0, sizeof(GameState), PROT_READ, MAP_SHARED, fd, 0);
																		   
	// Game sync
    fd = shm_open(GAME_SYNC_SHM, O_RDWR, 0666); 
    if (fd == -1) {
        perror("PLAYER::INIT_SHM: Error game sync\n");
		return -1;
    }

    p->game_sync = mmap(0, sizeof(GameSync), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

	return 0;
}

void get_state_snapshot(PlayerADT p) {

	reader_enter(p);

	// Chequear si conocemos nuestro id
	if (p->id == -1) {

		// Obtenerlo
		pid_t my_pid = getpid();
		for (unsigned int i = 0; p->id < 0 && i < p->game_state->player_count; i++) {
			if (p->game_state->players[i].pid == my_pid) {
				p->id = i;
			}
		}

	}
	
	p->x = p->game_state->players[p->id].x;
	p->y = p->game_state->players[p->id].y;
	p->blocked = p->game_state->players[p->id].blocked;
	p->game_finished = p->game_state->finished;
	
	// TODO: Usar memcpy
	for (int i = 0; i < p->height; i++) {
		for (int j = 0; i < p->width; j++) {
			p->board[i*p->width + j] = p->game_state->board[i*p->width + j];
		}
	}

	reader_leave(p);
}

bool still_playing(PlayerADT p) {
	return p->game_finished || p->blocked;
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

bool is_valid_position(GameState *state, int x, int y) {
    return x >= 0 && x < state->width && y >= 0 && y < state->height;
}

int get_board_cell(GameState *state, int x, int y) {
    if (!is_valid_position(state, x, y)) {
        return -1; // Valor inválido para posiciones fuera del tablero
    }
    return state->board[y * state->width + x];
}

bool is_cell_free(GameState *state, int x, int y) {
    int cell_value = get_board_cell(state, x, y);
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

int count_free_neighbors(GameState *state, int x, int y) {
    int count = 0;
    for (unsigned char dir = 0; dir < 8; dir++) {
        int dx = 0, dy = 0;
        direction_to_offset(dir, &dx, &dy);
        int nx = x + dx;
        int ny = y + dy;
        if (is_cell_free(state, nx, ny)) {
            count++;
        }
    }
    return count;
}

int calculate_depth(GameState *state, int start_x, int start_y, int dx, int dy, int max_depth) {
    int depth = 0;
    int x = start_x + dx;
    int y = start_y + dy;

    while (depth < max_depth && is_cell_free(state, x, y)) {
        depth++;
        x += dx;
        y += dy;
    }

    return depth;
}

bool has_nearby_players(GameState *state, int x, int y, int my_id) {
    for (int i = 0; (unsigned int) i < state->player_count; i++) {
        if (i == my_id || state->players[i].blocked) continue;

        int player_x = state->players[i].x;
        int player_y = state->players[i].y;

        int dx = abs(x - player_x);
        int dy = abs(y - player_y);

        if (dx <= 2 && dy <= 2 && (dx + dy) <= 3) {
            return true;
        }

    }
    return false;
}

bool is_potential_trap(GameState *state, int x, int y) {
    int free_neighbors = count_free_neighbors(state, x, y);

    if (free_neighbors == 1) {
        for (unsigned char dir = 0; dir < 8; dir++) {
            int dx = 0, dy = 0;
            direction_to_offset(dir, &dx, &dy);
            int nx = x + dx;
            int ny = y + dy;

            if (is_cell_free(state, nx, ny)) {
                int next_neighbors = count_free_neighbors(state, nx, ny);
                return next_neighbors <= 2;
            }
        }
    }

    return free_neighbors == 0;
}

bool is_endgame(GameState *state) {
    int total_cells = state->width * state->height;
    int free_cells = 0;
    
    for (int i = 0; i < total_cells; i++) {
        if (state->board[i] >= 1 && state->board[i] <= 9) {
            free_cells++;
        }
    }

    return (free_cells * 100 / total_cells) < 20; // Menos del 15% libre = endgame
}

void reader_enter(PlayerADT p) {
	sem_wait(&p->game_sync->reader_count_mutex);

	// Actualizar contador
	p->game_sync->reader_count++;
	
	// Si soy el unico lector, verificar que el master no este escribiendo
	if (p->game_sync->reader_count == 1) {
		sem_wait(&p->game_sync->master_mutex);
	}
	
	// Liberar variable
	sem_post(&p->game_sync->reader_count_mutex);

	// Esperar a que se pueda acceder
	sem_wait(&p->game_sync->state_mutex);
}

// NOTE: Esto tmb le sirve a la vista, mas tarde unificarlo en common.h
void reader_leave(PlayerADT p) {

	// Liberar game state
	sem_post(&p->game_sync->state_mutex);
	
	// Actualizar variable
	sem_wait(&p->game_sync->reader_count_mutex);
	p->game_sync->reader_count--;

	// Si nadie mas esta leyendo, notificar al master que puede escribir
	if (p->game_sync->reader_count == 0) {
		sem_post(&p->game_sync->master_mutex);
	}
	
	// Liberar variable
	sem_post(&p->game_sync->reader_count_mutex);
}




