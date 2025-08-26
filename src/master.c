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
} MasterCDT;

typedef MasterCDT* MasterADT;

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

	char player_flag = 0; // El unico que puede recibir mas de un parametro es -p

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

static int init_sem(MasterADT m) {

	return 0;
}

static int init_state(MasterADT m) {

	return 0;
}

static int init_sync(MasterADT m) {
	

	// Create semaphores
	sem_init(&m->game_sync->state_change, 1, 0);
	//m->game_sync->render_done = *sem_open("/sem_render_done", O_CREAT, 0644, 0);	
	//m->game_sync->master_mutex = *sem_open("/sem_master_mutex", O_CREAT, 0644, 0);	
	//m->game_sync->state_mutex = *sem_open("/sem_state_mutex", O_CREAT, 0644, 0);	
	//m->game_sync->reader_count_mutex = *sem_open("/sem_reader_count_mutex", O_CREAT, 0644, 0);
	// Create the player movement sem with a unique name for each
	//char player_sem[21] = { 0 };
	//for (int i = 0; i < m->player_count; i++) {
		// TODO: Se puede cambiar el string que le pase a sem_open?	
	//	sprintf(player_sem, "/sem_player_can_move", i);
	//	m->game_sync->player_can_move[i] = *sem_open(player_sem, O_CREAT, 0644, 0);	
	//}

	return 0;
}

static int init_shm(MasterADT m) {

	// TODO: Error check, investigar flags, son las de ChompChamps sacadas con strace
	// Game state
	int fd = shm_open(GAME_STATE_SHM, O_CREAT | O_RDWR | O_NOFOLLOW | O_CLOEXEC, 0644);  
	ftruncate(fd, sizeof(GameState));  
	m->game_state = mmap(0, sizeof(GameState), PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0);

	// Game sync
	fd = shm_open(GAME_SYNC_SHM, O_CREAT | O_RDWR | O_NOFOLLOW | O_CLOEXEC, 0666);  
	ftruncate(fd, sizeof(GameSync));  
	m->game_sync = mmap(0, sizeof(GameSync), PROT_READ | PROT_READ, MAP_SHARED, fd, 0);

	return 0;
}

int main (int argc, char *argv[]) {

	MasterCDT m = { DEFAULT_WIDTH,
					DEFAULT_HEIGHT,
					DEFAULT_DELAY,
					time(NULL),
					0,
					0,
					0 };

	if (parse_args(&m, argc, argv)) {
		printf("Usage: ./ChompChamps [-w width] [-h height] [-d delay] [-t timeout] [-s seed] [-v view] [-p player1 player2...]\n");

		return -1;
	}

	// Initialization
	init_shm(&m);
	init_state(&m);
	init_sync(&m);

	return 0;
}
