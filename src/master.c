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
#include <errno.h>

#define MAX_STR_LEN 20

#define DEFAULT_WIDTH 10
#define DEFAULT_HEIGHT 10
#define DEFAULT_DELAY 200
#define MAX_PLAYERS 9
#define MIN_PLAYERS 1

// NOTE: Usar malloc para los strings?
typedef struct {
	int width, height, delay, seed;	// TODO: sacar width y height, meterlos en game state(mismo para player_count)
	char view_path[MAX_STR_LEN];
	char player_path[MAX_PLAYERS][MAX_STR_LEN];
	int player_count;
	GameState *game_state;
	GameSync *game_sync;
	int game_state_fd, game_sync_fd;
	int pipes[MAX_PLAYERS];		// fd de la salida de los pipes de cada jugador
} MasterCDT;

typedef MasterCDT* MasterADT;

// TODO: Hacer que retornen algo diferente de 0 en caso de error y manejar errores

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

	m->game_state->width = m->width;
	m->game_state->height = m->height;
	m->game_state->player_count = m->player_count;
	m->game_state->finished = 0;

	// Llenar el tablero
	srand(m->seed);

	for (int i = 0; i < m->height; i++) {
		for (int j = 0; j < m->width; j++) {

			m->game_state->board[i * m->width + j] = 1 + rand() % 9;	// Numero entre 1 y 9

		}
	}

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
	int state_size = sizeof(GameState) + m->width * m->height * sizeof(int);
	
	m->game_state_fd = shm_open(GAME_STATE_SHM, O_CREAT | O_RDWR, 0644);  
	ftruncate(m->game_state_fd, state_size);	// Agregar tambien el espacio que ocupara board  
	m->game_state = mmap(0, state_size, PROT_WRITE | PROT_READ, MAP_SHARED, m->game_state_fd, 0);

	// Game sync
	m->game_sync_fd = shm_open(GAME_SYNC_SHM, O_CREAT | O_RDWR, 0666);  
	ftruncate(m->game_sync_fd, sizeof(GameSync));  
	m->game_sync = mmap(0, sizeof(GameSync), PROT_WRITE | PROT_READ, MAP_SHARED, m->game_sync_fd, 0);

	return 0;
}

static int init_childs(MasterADT m) {

	// Preparar argumentos
	char arg1[MAX_STR_LEN];
	char arg2[MAX_STR_LEN];

	char* argv[] = {m->view_path, arg1, arg2, NULL}; // Debe terminar en un puntero a NULL
	
	sprintf(arg1, "%d", m->width);
	sprintf(arg2, "%d", m->height);

	// Primero la vista(antes del pipe)
	int view_pid = fork();
	if (view_pid == 0) {
		return execve(m->view_path, argv, NULL); 
	}

	// Ahora preparar los pipes
	int pipe_fd[2]; // Aca se guardan los dos extremos

	// Luego los jugadores
	int player_pid = -1;
	
	for (int i = 0; i < m->player_count; i++) {
		if (pipe(pipe_fd) == -1) {
			printf("MASTER::INIT_CHILDS: Error creating pipe\n");

			return -1;
		}

		argv[0] = m->player_path[i]; // El primer argumento es el nombre del programa

		player_pid = fork();
		
		if (player_pid == 0) {
			
			// Configurar los fd del jugador
			close(pipe_fd[0]);	// Cerrar salida del pipe
			dup2(pipe_fd[1], 1); // Cerrar stdout y duplicar entrada del pipe a stdout
			close(pipe_fd[1]);	// Ya no necesita este fd 

			return execve(m->player_path[i], argv, NULL);
		}

		// Agregar al game state
		m->game_state->players[i].pid = player_pid;

		// Si llegue aca estoy en master, configurar pipe
		close(pipe_fd[1]); // Este no lo necesito
		m->pipes[i] = pipe_fd[0];
	}

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

	for (int i = 0; i < m->player_count; i++) {
		close(m->pipes[i]);
	}

	return 0;
}

static int check_player(MasterADT m, int player_id) {

	sem_post(&m->game_sync->player_can_move[player_id]);	
	sem_post(&m->game_sync->state_mutex);

	
	sem_post(&m->game_sync->master_mutex);
	char c = -1;	
	// TODO: Error check read bytes
	read(m->pipes[player_id], &c, 1);	// Recibir movimiento del pipe del jugador


	switch(c) {
		case 0:

			// NOTE: Temporal, para que ande
			
		//	m->game_state->players[player_id].y++;

			break;
		default:
			m->game_state->players[player_id].invalid_reqs++;

	};

	// Actualizar board
	int x = m->game_state->players[player_id].x;
	int y = m->game_state->players[player_id].y;

	// NOTE: No es seguro
	m->game_state->board[y*m->game_state->width + x] = -1 * player_id;	

	return 0;
}

static int game_start(MasterADT m) {

	while (!m->game_state->finished) {
		// Sincronizacion vista
		sem_post(&m->game_sync->state_change);

		struct timespec ts;
		if (clock_gettime(CLOCK_REALTIME, &ts) == -1)
		{
			/* handle error */
			return -1;
		}

		ts.tv_sec += 1;
		int s;
		s = sem_timedwait(&m->game_sync->render_done, &ts);
		
		// Verificar resultado del wait
		if (s == -1)
		{
			if (errno == ETIMEDOUT)
				printf("View must sem_post(render_done)\n");
			else
				perror("sem_timedwait");

			// NOTE: sem_post del mutex?
			m->game_state->finished = 1;
			return -1;
		} 		
		
		// Termino de leer game state
		m->game_sync->reader_count--;
		sem_post(&m->game_sync->reader_count_mutex);


		// Sincronizacion jugador
		// TODO: Elegir los jugadores con select() y sin repetir el mismo jugador
		check_player(m, 0);

		// Vuelve a leer el estado
		sem_wait(&m->game_sync->reader_count_mutex);
		m->game_sync->reader_count++;
	}

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
					0,
					{ 0 }};

	if (parse_args(&m, argc, argv)) {
		printf("Usage: ./ChompChamps [-w width] [-h height] [-d delay] [-t timeout] [-s seed] [-v view] [-p player1 player2...]\n");

		return -1;
	}

	// Setup inicial
	init_shm(&m); // Se debe llamar primero
	init_state(&m);
	init_sync(&m);
	// TODO: Imprimir informacion del juego como el ejemplo

	if (init_childs(&m) == -1) {
		printf("MASTER::INIT_CHILDS: Error with the forking and piping\n");

		return -1;
	}

	// EXPERIMENTAL - main loop
	game_start(&m);	

	// Una vez termino todo, liberar recursos
    cleanup(&m);

	return 0;
}
