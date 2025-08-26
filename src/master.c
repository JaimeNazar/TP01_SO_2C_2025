#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/mman.h> 
#include <sys/stat.h>       
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <semaphore.h>  

// TODO: Put in common.h shared dependencies

#include "common.h"

#define MAX_STR_LEN 20

#define DEFAULT_WIDTH 10
#define DEFAULT_HEIGHT 10
#define DEFAULT_DELAY 200
#define MAX_PLAYERS 9
#define MIN_PLAYERS 1

// NOTE: Usar malloc para los strings?
typedef struct {
	int width, height, delay, seed;
	char view_path[MAX_STR_LEN];
	char player_path[MAX_PLAYERS][MAX_STR_LEN];
	int player_count;
	GameState *game_state;
	GameSync *game_sync;
	int game_state_fd, game_sync_fd;
} MasterCDT;

typedef MasterCDT* MasterADT;

// TODO: Hacer que retornen algo diferente de 0 en caso de error

/* Funcion auxiliar, copia contenidos de un string a otro. 
 * Realizando verificaciones correspondientes. 
 */
static int str_copy(char *s1, char *s2) {
	int len = strlen(s2);

	if (len >= MAX_STR_LEN) {
		printf("MASTER::STR_CPY: String too long for copy\n");
		return -1;
	}

	strcpy(s1, s2);

	return 0;
}

static int parse_args(MasterADT m, int argc, char *argv[]) {

	int i = 1;	// Saltearse nombre del programa

	while (i < argc) {

		// Tipo de argumento
		if (argv[i][0] == '-') {
			switch(argv[i][1]) {
				case 'v':
					// Obtener el file path
					i++;

					if (str_copy(m->view_path, argv[i])) {
						printf("MASTER::PARSE: Invalid view path\n");
						return -1;
					}
					
					i++;
					break;
				case 'p':
					i++;
							
					// TODO: Modularizar, son muchas cosas anidadas
					// Guardar jugadores hasta el proximo argumento
					while (i < argc && argv[i][0] != '-') {
						if (m->player_count >= MAX_PLAYERS) {
							printf("MASTER::PARSE: Too many players, max is %d\n", MAX_PLAYERS);
							return -1;
						}

						if (str_copy(m->player_path[m->player_count], argv[i])) {
							printf("MASTER::PARSE: Invalid player path\n");
							return -1;
						}
						
						m->player_count++;
						i++;
					}

					break;
				default:
					printf("MASTER::PARSE: Invalid argument type: %c \n", argv[i][1]);
					return -1;

					break;
			}
		} else {
			printf("MASTER::PARSE: Invalid argument: %s\n", argv[i]);

			return -1;
		}

	}

	// Si no se registro ningun jugador al llegar aca, notificar al usuario
	if(m->player_count < MIN_PLAYERS) {
		printf("MASTER::PARSE: At least one player must be specified\n");
		return -1;
	}

	return 0;
}

static int init_state(MasterADT m) {
	
	//m->game_state->board = malloc(sizeof(int) * m->width * m->height);
	//if (m->game_state->board == NULL) {
	//	printf("MASTER::INIT_STATE: Failed to reserve memory\n");
	//	return -1;
	//}

	// Llenar el tablero

	return 0;
}

static int init_sync(MasterADT m) {
	

	// Crear semaforos
	sem_init(&m->game_sync->state_change, 1, 0);
	sem_init(&m->game_sync->render_done, 1, 0);
	sem_init(&m->game_sync->master_mutex, 1, 0);
	sem_init(&m->game_sync->state_mutex, 1, 0);
	sem_init(&m->game_sync->reader_count_mutex, 1, 0);	

	for (int i = 0; i < m->player_count; i++) {
		sem_init(&m->game_sync->player_can_move[i], 1, 0);
	}

	return 0;
}

static int init_shm(MasterADT m) {

	// TODO: Error check, investigar flags, son las de ChompChamps sacadas con strace
	// Game state
	m->game_state_fd = shm_open(GAME_STATE_SHM, O_CREAT | O_RDWR | FD_CLOEXEC, 0644);  
	ftruncate(m->game_state_fd, sizeof(GameState));  
	m->game_state = mmap(0, sizeof(GameState), PROT_WRITE | PROT_READ, MAP_SHARED, m->game_state_fd, 0);

	// Game sync
	m->game_sync_fd = shm_open(GAME_SYNC_SHM, O_CREAT | O_RDWR | FD_CLOEXEC, 0666);  
	ftruncate(m->game_sync_fd, sizeof(GameSync));  
	m->game_sync = mmap(0, sizeof(GameSync), PROT_READ | PROT_READ, MAP_SHARED, m->game_sync_fd, 0);

	return 0;
}

static int close_shm(MasterADT m) {
	munmap(m->game_state, sizeof(GameState)); // Unmap del proceso
	close(m->game_state_fd); // Cerrar el fd
	shm_unlink(GAME_STATE_SHM); // Remover el objeto de memoria compartida, solo cuando nadie mas lo utilice
	
	// Hacer lo mismo para GameSync
	munmap(m->game_sync, sizeof(GameSync));
	close(m->game_sync_fd);
	shm_unlink(GAME_SYNC_SHM);

	return 0;
}

static int cleanup(MasterADT m) {

	// Liberar memoria compartida
	close_shm(m);

	// Liberar memoria tablero
	//free(m->game_state->board);

	return 0;
}

int main (int argc, char *argv[]) {

	MasterCDT m = { DEFAULT_WIDTH,
					DEFAULT_HEIGHT,
					DEFAULT_DELAY,
					time(NULL),
					{ 0 },
					{ {0} },
					0,
					0,
					0,
					0,
					0 };

	if (parse_args(&m, argc, argv)) {
		printf("Usage: ./ChompChamps [-w width] [-h height] [-d delay] [-t timeout] [-s seed] [-v view] [-p player1 player2...]\n");

		return -1;
	}

	// Setup inicial
	init_shm(&m); // Se debe llamar primero
	init_state(&m);
	init_sync(&m);

	// Una vez termino todo, liberar recursos
    cleanup(&m);

	return 0;
}
