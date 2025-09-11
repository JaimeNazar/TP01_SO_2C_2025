#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/mman.h> 
#include <sys/stat.h>  
#include <sys/select.h>
#include <fcntl.h>
#include <stdbool.h>
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
#define DEFAULT_TIMEOUT 10
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
	int pipes_max_fd;
	fd_set pipes_set;
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

void wait_delay(int delay_ms) {
    struct timespec ts;
    ts.tv_sec  = delay_ms / 1000;
    ts.tv_nsec = (delay_ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);  //usamos nanoslepp pues usleep no funciona
}

MasterADT init_master(int seed, unsigned int delay, unsigned int timeout) {
	
	MasterADT m = calloc(1, sizeof(MasterCDT));

	if (m == NULL) {
		return NULL;
	}

	m->delay = delay;
	m->seed = seed;
	m->timeout = timeout;
	FD_ZERO(&m->pipes_set);
	m->pipes_max_fd = -1;

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

		// Calcula un rectángulo central (60% del tablero, centrado)
	int rect_w = (int)(gs->width * 0.6);
	int rect_h = (int)(gs->height * 0.6);
	int rect_x0 = (gs->width - rect_w) / 2;
	int rect_y0 = (gs->height - rect_h) / 2;

	// Calcula la cantidad de celdas del borde del rectángulo
	int border_cells = 2 * (rect_w + rect_h) - 4;

	// Guardar todas las posiciones del borde en un array
	int border_x[border_cells];
	int border_y[border_cells];
	int idx = 0;

	// Borde superior (izq a der)
	for (int x = 0; x < rect_w; x++) {
	    border_x[idx] = rect_x0 + x;
	    border_y[idx++] = rect_y0;
	}
	// Borde derecho (arriba a abajo, sin repetir esquina)
	for (int y = 1; y < rect_h - 1; y++) {
	    border_x[idx] = rect_x0 + rect_w - 1;
	    border_y[idx++] = rect_y0 + y;
	}
	// Borde inferior (der a izq)
	for (int x = rect_w - 1; x >= 0; x--) {
	    border_x[idx] = rect_x0 + x;
	    border_y[idx++] = rect_y0 + rect_h - 1;
	}
	// Borde izquierdo (abajo a arriba, sin repetir esquina)
	for (int y = rect_h - 2; y > 0; y--) {
	    border_x[idx] = rect_x0;
	    border_y[idx++] = rect_y0 + y;
	}

	// Ahora ubicamos a los jugadores equidistantes
	for (unsigned int i = 0; i < gs->player_count; i++) {
	    int pos = (i * border_cells) / gs->player_count;
	    int px = border_x[pos];
	    int py = border_y[pos];

	    gs->players[i].score = 0;
	    gs->players[i].invalid_reqs = 0;
	    gs->players[i].valid_reqs = 0;
	    gs->players[i].x = px;
	    gs->players[i].y = py;
	    gs->players[i].pid = -1;
	    gs->players[i].blocked = 0;

	    // Marcar la celda como ocupada por el jugador
	    gs->board[py * gs->width + px] = -1 * (int)i;
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
	sem_init(&m->game_sync->master_mutex, 1, 1);
	sem_init(&m->game_sync->state_mutex, 1, 1);
	sem_init(&m->game_sync->reader_count_mutex, 1, 1);	

	for (unsigned int i = 0; i < m->game_state->player_count; i++) {
		sem_init(&m->game_sync->player_can_move[i], 1, 1);
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
		FD_SET(pipe_fd[0], &m->pipes_set);

		m->pipes[i] = pipe_fd[0];

		// Obtener el maximo para la llamada al select()
		if (pipe_fd[0] > m->pipes_max_fd) {
			m->pipes_max_fd = pipe_fd[0];
		}
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
static bool check_player(MasterADT m, int player_id) {

	char c = -1;	

	// TODO: Error check read bytes
	read(m->pipes[player_id], &c, 1);	// Recibir movimiento del pipe del jugador
	
	reader_enter(m->game_sync);

	// Guardar posiciones de inicio para luego modificarlas en la escritura al game state
	// No pueden cambiar pues el master es el unico escritor, es seguro
	unsigned short x = m->game_state->players[player_id].x;
	unsigned short y = m->game_state->players[player_id].y;

	reader_leave(m->game_sync);

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

	// Alias para game state
	GameState *gs = m->game_state;

	// Entra escritor
	writer_enter(m->game_sync);

	bool is_invalid_move = false;
	// Chequear si la nueva posicion es valida
	if (x >= gs->width || y >= gs->height
    || gs->board[y*gs->width + x] <= 0) {

		gs->players[player_id].invalid_reqs++;
        is_invalid_move =true;
		// TODO: Verificar cuando queda bloqueado
	} else {
		gs->players[player_id].valid_reqs++;

		// Actualizar estado
		gs->players[player_id].x = x;
		gs->players[player_id].y = y;
		gs->players[player_id].score += gs->board[y*gs->width + x];

		gs->board[y*gs->width + x] = -1 * player_id;	
	}

	// Sale escritor	
	writer_leave(m->game_sync);

	// Notificar al jugador que puede enviar otro movimiento
	sem_post(&m->game_sync->player_can_move[player_id]);	

	return is_invalid_move;
}

static void pipe_set_blocked(MasterADT m, int id) {

	m->pipes[id] = -1; // Marcarlo como invalido

	// Buscar nuevo maximo
	m->pipes_max_fd = -1;
	
	for (unsigned int i = 0; i < m->game_state->player_count; i++) {
		if (m->pipes[i] > m->pipes_max_fd) {
			m->pipes_max_fd = m->pipes[i];
		}
	}
}

static const int DX[8] = { 0, 1, 1, 1, 0,-1,-1,-1 };
static const int DY[8] = {-1,-1, 0, 1, 1, 1, 0,-1 };

// TODO: WTF, solo chequea si blocked = true
// Funcion que pregunta si todos los jugadores pueden moverse o no, lo hace chequeando todas las celdas adyacentes a los jugadores
bool no_player_can_move(MasterADT m) {
    const GameState *st = m->game_state;

    for (size_t i = 0; i < st->player_count; i++) {
        const Player *p = &st->players[i];

        // bloqueado => cuenta como "sin movimientos", pasamos al siguiente
        if (p->blocked) continue;

        const int x = p->x, y = p->y;

        // revisar las 8 adyacencias con salida temprana
        for (int d = 0; d < 8; d++) {
            const int nx = x + DX[d];
            const int ny = y + DY[d];

            if (nx < 0 || ny < 0 || nx >= st->width || ny >= st->height)
                continue;

            const int cell = st->board[ny * st->width + nx];
            if (cell > 0) {
                // hay al menos un movimiento posible → no terminar
                return false;
            }
        }
        // este jugador no tiene adyacentes libres; seguimos con el próximo
    }

    // nadie pudo moverse (o todos estaban bloqueados)
    return true;
}

typedef struct {
    time_t start;
    unsigned int duration; // en segundos
    bool active;
} Timeout;

void start_timeout(Timeout *t, unsigned int seconds) {
    t->start = time(NULL);
    t->duration = seconds;
    t->active = true;
}

bool timeout_expired(Timeout *t) {
    if (!t->active) return false; // no se inició
    time_t now = time(NULL);
    return (now - t->start) >= t->duration;
}

void stop_timeout(Timeout *t) {
    t->active = false;
}

static int game_start(MasterADT m) {
    Timeout t = {0, m->timeout, false};

	while (!m->game_state->finished) {

		// Sincronizacion vista
		sem_post(&m->game_sync->state_change);

		sem_wait(&m->game_sync->render_done);

        wait_delay(m->delay);
		// Seleccionar siguiente jugador, pipes_set queda solo con los fd que no estan bloqueados
		int ready = select(m->pipes_max_fd + 1, &m->pipes_set, NULL, NULL, NULL);

        if(no_player_can_move(m) || timeout_expired(&t)) {
            m->game_state->finished = 1;
            continue;
        }

		if (ready == -1) {
            perror("MASTER::GAME_START: Error with select");
            return -1;
        } else if (ready >= 0) {
			for (unsigned int i = 0; i < m->game_state->player_count; i++) {

				if (m->pipes[i] == -1 || m->game_state->players[i].blocked)	// Ignorar
					continue;

				if (FD_ISSET(m->pipes[i], &m->pipes_set)) {
                    // LUEGO leer el movimiento
                    if(!check_player(m, i)){//si es valido
                        start_timeout(&t, m->timeout);
                    }
                } else {
					// Esta bloqueado, actualizar data
					pipe_set_blocked(m, i);
				}

			}
		}

	}

	return 0;
}

void print_final_results(const MasterADT m) {
    const GameState *st = m->game_state;

    // limpiar pantalla
    printf("\033[2J\033[H");

    // Líneas simples, sin trucos
    puts("View exited");  // puts agrega '\n' por sí mismo

    for (unsigned i = 0; i < st->player_count; i++) {
        const Player *p = &st->players[i];
        // Usamos \r\n por si la TTY quedó sin traducción de NL
        printf("Player %s (%u) with a score of %u / %u / %u\r\n",
               p->name, i, p->score, p->valid_reqs, p->invalid_reqs);
    }

    fflush(stdout);

	// Restaurar la terminal a un estado "sano" por si algún hijo la dejó rota
	system("stty sane");
}

void show_game_info(const MasterADT m) {
	const GameState *st = m->game_state;
	// limpiar pantalla
	printf("\033[2J\033[H");

	printf("width: %u\n", st->width);
	printf("height: %u\n", st->height);
	printf("delay: %d\n", m->delay);
	printf("timeout: %d\n", m->timeout);
	printf("seed: %d\n", m->seed);
	printf("view: %s\n", m->view_path);
	printf("num_players: %u\n", st->player_count);
	for (unsigned i = 0; i < st->player_count; i++) {
		printf("  %s\n", m->player_path[i]);
	}
	sleep(2); // Pausa para que el usuario pueda leer
	fflush(stdout);
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
	if (init_state(m, width, height, player_count) == -1){
		return -1;
	}

	init_sync(m);

	show_game_info(m);

	if (init_childs(m) == -1) {
		printf("MASTER::INIT_CHILDS: Error with the forking and piping\n");

		cleanup(m);
		return -1;
	}
	// Main loop
	game_start(m);

	//limpiar pantalla y mostrar resultados
	print_final_results(m);

// Una vez termino t odo , liberar recursos
	cleanup(m);
	return 0;
}
