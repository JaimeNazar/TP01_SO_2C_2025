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
#define DEFAULT_TIMEOUT 0
#define MAX_PLAYERS 9
#define MIN_PLAYERS 1

// NOTE: Usar malloc para los strings?
typedef struct {
	int delay, seed, timeout;	// TODO: sacar width y height, meterlos en game state(mismo para player_count)
	char view_path[MAX_STR_LEN];
	char player_path[MAX_PLAYERS][MAX_STR_LEN];
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

static int parse_args(MasterADT m, int argc, char *argv[], unsigned int* width, unsigned int* height, unsigned int *player_count) {

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
						if (*player_count >= MAX_PLAYERS) {
							printf("MASTER::PARSE: Too many players, max is %d\n", MAX_PLAYERS);
							return -1;
						}

						if (str_copy(m->player_path[*player_count], argv[i])) {
							printf("MASTER::PARSE: Invalid player path\n");
							return -1;
						}
						
						(*player_count)++;
						i++;
					}

					break;
                case 'd':
                    i++;
                    if (i >= argc) {
                        printf("MASTER::PARSE: Missing value for -d\n");
                        return -1;
                    }
                    m->delay = atoi(argv[i]);
                    i++;
                    break;

                case 't':
                    i++;
                    if (i >= argc) {
                        printf("MASTER::PARSE: Missing value for -t\n");
                        return -1;
                    }
                    m->timeout = atoi(argv[i]);
                    i++;
                    break;

                case 'w':
                    i++;
                    *width = atoi(argv[i]);
                    i++;
                    break;

                case 'h':
                    i++;
                    *height = atoi(argv[i]);
                    i++;
                    break;

                case 's':
                    i++;
                    m->seed = atoi(argv[i]);
                    i++;
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
	if(*player_count < MIN_PLAYERS) {
		printf("MASTER::PARSE: At least one player must be specified\n");
		return -1;
	}

	if (*width <= 0)
		*width = DEFAULT_WIDTH;

	if (*height <= 0)
		*height = DEFAULT_HEIGHT;

	return 0;
}

void esperar_delay(int delay_ms) {
    struct timespec ts;
    ts.tv_sec  = delay_ms / 1000;
    ts.tv_nsec = (delay_ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);  //usamos nanoslepp pues usleep no funciona
}


MasterADT init_master(int seed, unsigned int delay, unsigned int timeout) {
	
	MasterADT m = calloc(0, sizeof(MasterCDT));

	if (m == NULL) {
		return NULL;
	}

	m->delay = delay;
	m->seed = seed;
	m->timeout = timeout;

	return m;
}

static int init_state(MasterADT m, unsigned int width, unsigned int height, unsigned int player_count) {

	// Inicializar memoria compartida
	int state_size = sizeof(GameState) + width * height * sizeof(int); // Agregar tambien el espacio que ocupara board  
	
	m->game_state_fd = shm_open(GAME_STATE_SHM, O_CREAT | O_RDWR, 0644);  
	if (m->game_state_fd == -1) {
		perror("MASTER::INIT_SHM: Failed to open state shared memory");	// Usar perror porque devuelvo informacion del errno

		return -1;
	}

	ftruncate(m->game_state_fd, state_size);	
	m->game_state = mmap(0, state_size, PROT_WRITE | PROT_READ, MAP_SHARED, m->game_state_fd, 0);

	// Inicializar valores
	GameState* gs = m->game_state;	

	gs->finished = 0;
	gs->width = width;
	gs->height = height;
	gs->player_count = player_count;

	// Llenar el tablero
	srand(m->seed);

	for (unsigned int i = 0; i < gs->height; i++) {
		for (int j = 0; j < gs->width; j++) {

			gs->board[i * gs->width + j] = 1 + rand() % 9;	// Numero entre 1 y 9

		}
	}

	// Inicializar jugadores, sino estan en un estado inconsistente
	for (unsigned int i = 0; i < gs->player_count; i++) {
		// TODO: Hacer que empiecen en un lugar random

		 gs->players[i].score = 0;
		 gs->players[i].invalid_reqs = 0;
		 gs->players[i].valid_reqs = 0;
		 gs->players[i].x = 0;
		 gs->players[i].y = 0;
		 gs->players[i].pid = -1;
		 gs->players[i].blocked = 0;
	}

	return 0;
}

static int init_sync(MasterADT m) {
	
	// Inicializar memoria compartida
	m->game_sync_fd = shm_open(GAME_SYNC_SHM, O_CREAT | O_RDWR, 0666);  
	ftruncate(m->game_sync_fd, sizeof(GameSync));  
	m->game_sync = mmap(0, sizeof(GameSync), PROT_WRITE | PROT_READ, MAP_SHARED, m->game_sync_fd, 0);
	if (m->game_sync_fd == -1) {
		perror("MASTER::INIT_SHM: Failed to open sync shared memory\n");

		return -1;
	}

	// Crear semaforos
	sem_init(&m->game_sync->state_change, 1, 0); 
	sem_init(&m->game_sync->render_done, 1, 0);
	sem_init(&m->game_sync->master_mutex, 1, 0);
	sem_init(&m->game_sync->state_mutex, 1, 0);
	sem_init(&m->game_sync->reader_count_mutex, 1, 1);	

	for (unsigned int i = 0; i < m->game_state->player_count; i++) {
		sem_init(&m->game_sync->player_can_move[i], 1, 0);
	}

	return 0;
}

static int init_childs(MasterADT m) {

	GameState* gs = m->game_state;

	// Preparar argumentos
	char arg1[MAX_STR_LEN];
	char arg2[MAX_STR_LEN];

	char* argv[] = {m->view_path, arg1, arg2, NULL}; // Debe terminar en un puntero a NULL
	
	sprintf(arg1, "%d", gs->width);
	sprintf(arg2, "%d", gs->height);

	// Primero la vista(antes del pipe)
	int view_pid = fork();
	if (view_pid == 0) {
		return execve(m->view_path, argv, NULL); 
	}

	// Ahora preparar los pipes
	int pipe_fd[2]; // Aca se guardan los dos extremos

	// Luego los jugadores
	int player_pid = -1;
	
	for (unsigned int i = 0; i < m->game_state->player_count; i++) {
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

	for (unsigned int i = 0; i < m->game_state->player_count; i++) {
		close(m->pipes[i]);
	}

	// Liberar memoria compartida
	close_shm(m);

	free(m);

	return 0;
}

/*
 * Procesa el estado de un jugador.
 * En caso de recibir un id invalido retorna 01.
 */
static int check_player(MasterADT m, int player_id) {

	sem_post(&m->game_sync->player_can_move[player_id]);	
	sem_post(&m->game_sync->state_mutex);

	sem_post(&m->game_sync->master_mutex);
	char c = -1;	

	// TODO: Error check read bytes
	read(m->pipes[player_id], &c, 1);	// Recibir movimiento del pipe del jugador
	
	// Guardar posiciones de inicio para luego modificarlas en el switch
	unsigned short x = m->game_state->players[player_id].x;
	unsigned short y = m->game_state->players[player_id].y;

	// TODO: Estos son los codigos de movimientos correctos?
	switch(c) {
		case 0:	// Arriba	
			y--;	

			break;
		case 1:	// Arriba-derecha		
			y--;
			x++;

			break;
		case 2: // Derecha
			x++;

			break;
		case 3: // Abajo-derecha
			y++;
			x++;

			break;
		case 4: // abajo
			y++;

			break;
		case 5:	// Abajo-izquierda
			y++;
			x--;

			break;
		case 6: // Izquierda
			x--;	

			break;
		case 7:	// Arriba-izquierda
			y--;
			x--;

			break;
		default:

			break;
	};

	// Chequear si la nueva posicion es valida
	if (x >= m->game_state->width || y >= m->game_state->height
    || m->game_state->board[y*m->game_state->width + x] <= 0) {

		m->game_state->players[player_id].invalid_reqs++;

		// TODO: Verificar cuando queda bloqueado
	} else {
		m->game_state->players[player_id].valid_reqs++;

		// Actualizar estado
		m->game_state->players[player_id].x = x;
		m->game_state->players[player_id].y = y;

		m->game_state->board[y*m->game_state->width + x] = -1 * player_id;	
	} 

	return 0;
}

static int game_start(MasterADT m) {

	while (!m->game_state->finished) {

		// Leer sync
		sem_wait(&m->game_sync->reader_count_mutex);
		m->game_sync->reader_count++;

		// Timeout para que sea mas humana la velocidad
		sleep(1);

		// Sincronizacion vista
		sem_post(&m->game_sync->state_change);

		sem_wait(&m->game_sync->render_done);
        esperar_delay(m->delay);
		// Termino de leer game state
		m->game_sync->reader_count--;
		sem_post(&m->game_sync->reader_count_mutex);


		// Sincronizacion jugador
		// TODO: Elegir los jugadores con select() y sin repetir el mismo jugador
		check_player(m, 0);

	}

	return 0;
}



int main (int argc, char *argv[]) {

	// Inicializar con valores default
	MasterADT m = init_master(time(NULL), DEFAULT_DELAY, DEFAULT_TIMEOUT);

	if (m == NULL) {
		perror("MASTER::INIT_MASTER: Error allocating memory");

		return -1;
	}

	// Guadar altura y ancho por separado pues las necesitamos para crear game state y se guardan en game state
	unsigned int width = 0;
	unsigned int height = 0;
	unsigned int player_count = 0;

	// Procesar argumentos
	if (parse_args(m, argc, argv, &width, &height, &player_count)) {
		printf("Usage: ./ChompChamps [-w width] [-h height] [-d delay] [-t timeout] [-s seed] [-v view] [-p player1 player2...]\n");

		cleanup(m);
		return -1;
	}

		// Setup inicial
	if (init_state(m, width, height, player_count) == -1)
		return -1;

	init_sync(m);

	if (init_childs(m) == -1) {
		printf("MASTER::INIT_CHILDS: Error with the forking and piping\n");

		cleanup(m);
		return -1;
	}
	// TODO: Imprimir informacion del juego como el ejemplo
	//
	// Main loop
	game_start(m);	

	// Una vez termino t odo , liberar recursos
    cleanup(m);
    // todo : imprimir resultados
	return 0;
}
