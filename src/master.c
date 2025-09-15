
#include <sys/wait.h>
#include <time.h>
#include <sys/select.h>
#include <ctype.h>


#include "common.h"

#define MAX_STR_LEN 20
#define DEFAULT_WIDTH 10
#define DEFAULT_HEIGHT 10
#define DEFAULT_DELAY 200
#define DEFAULT_TIMEOUT 10
#define MAX_PLAYERS 9
#define MIN_PLAYERS 1

typedef struct {
	int delay, seed, timeout;	
	char view_path[MAX_STR_LEN];
	char player_path[MAX_PLAYERS][MAX_STR_LEN];
	GameState *game_state;
	GameSync *game_sync;
	unsigned int player_count;	// Conviene guardar una copia ya que no va a cambiar mucho
	int game_state_fd, game_sync_fd;
	int pipes[MAX_PLAYERS];		// fd de la salida de los pipes de cada jugador
	int pipes_max_fd;
	fd_set pipes_set;
    pid_t view_pid;
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

bool is_number(const char *str) {
    if (str == NULL || *str == '\0')
        return false;
    for (int i = 0; str[i]; i++) {
        if (str[i] < '0' || str[i] > '9')
            return false;
    }
    return true;
}

static int parse_args(MasterADT m, int argc, char *argv[], unsigned int* width, unsigned int* height, unsigned int *player_count) {

	int i = 1;	// Saltearse nombre del programa
	while (i < argc) {

		// Tipo de argumento
		if (argv[i][0] == '-') {
			switch(argv[i++][1]) {
				case 'v': // view
					// Obtener el file path
					if (str_copy(m->view_path, argv[i++])) {
						printf("MASTER::PARSE: Invalid view path\n");
						return -1;
					}
					
					break;
				case 'p': // players
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
                case 'd': // delay
                    if (i >= argc) {
                        printf("MASTER::PARSE: Missing value for -d\n");
                        return -1;
                    }

					if (atoi(argv[i]) <= 0 || !is_number(argv[i]) ) {
						printf("MASTER::PARSE: Invalid delay value\n");
						return -1;
					}


                    m->delay = atoi(argv[i++]);
                    break;

                case 't': // timeout
                    if (i >= argc) {
                        printf("MASTER::PARSE: Missing value for -t\n");
                        return -1;
                    }

					if (atoi(argv[i]) <= 0 || !is_number(argv[i]) ) {
						printf("MASTER::PARSE: Invalid timeout value\n");
						return -1;
					}

                    m->timeout = atoi(argv[i++]);
                    break;

                case 'w': // width
					if(atoi(argv[i]) <= 9 || !is_number(argv[i]) ) {
						printf("MASTER::PARSE: Invalid width value\n");
						return -1;
					} 
                    *width = atoi(argv[i++]);
                    break;

                case 'h': // height

					if(atoi(argv[i]) <= 9 || !is_number(argv[i]) ) {
						printf("MASTER::PARSE: Invalid height value\n");
						return -1;
					}
                    *height = atoi(argv[i++]);
                    break;

                case 's': //seed
					if( !is_number(argv[i]) ) {
						printf("MASTER::PARSE: Invalid seed value\n");
						return -1;
					}
                    m->seed = atoi(argv[i++]);
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

	// Si no se pasó view, no arrancar
	if (m->view_path[0] == '\0') {
		printf("MASTER::PARSE: A view must be specified\n");
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
	m->player_count = 0;
	FD_ZERO(&m->pipes_set);
	m->pipes_max_fd = -1;
    m->view_pid = -1;

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
	m->player_count = player_count;	// Tambien guardar copia en el master, para evitar entrar en state todo el tiempo

	// Llenar el tablero
	srand(m->seed);

	for (unsigned int i = 0; i < gs->height; i++) {
		for (int j = 0; j < gs->width; j++) {

			gs->board[i * gs->width + j] = 1 + rand() % 9;	// Numero entre 1 y 9

		}
	}

	// Calcula un rectángulo interior para colocar a los jugadores 
	int rect_w = (int)(gs->width * 0.6);
	int rect_h = (int)(gs->height * 0.6);
	int rect_x0 = (gs->width - rect_w) / 2;
	int rect_y0 = (gs->height - rect_h) / 2;

	int border_cells = 2 * (rect_w + rect_h) - 4;

	// Guardar todas las posiciones del borde en un array
	int border_x[border_cells];
	int border_y[border_cells];
	int idx = 0;

	// Borde superior 
	for (int x = 0; x < rect_w; x++) {
	    border_x[idx] = rect_x0 + x;
	    border_y[idx++] = rect_y0;
	}
	// Borde derecho 
	for (int y = 1; y < rect_h - 1; y++) {
	    border_x[idx] = rect_x0 + rect_w - 1;
	    border_y[idx++] = rect_y0 + y;
	}
	// Borde inferior
	for (int x = rect_w - 1; x >= 0; x--) {
	    border_x[idx] = rect_x0 + x;
	    border_y[idx++] = rect_y0 + rect_h - 1;
	}
	// Borde izquierdo 
	for (int y = rect_h - 2; y > 0; y--) {
	    border_x[idx] = rect_x0;
	    border_y[idx++] = rect_y0 + y;
	}

	// Ubicar jugadores equidistantemente
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

	for (unsigned int i = 0; i < m->player_count; i++) {
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
	
	reader_enter(m->game_sync);
	sprintf(arg1, "%d", gs->width);
	sprintf(arg2, "%d", gs->height);
	reader_leave(m->game_sync);

	// Primero la vista(antes del pipe)
	pid_t view_pid = fork();
	if (view_pid == 0) {
		return execve(m->view_path, argv, NULL); 
	}
    m->view_pid = view_pid;

	// Ahora preparar los pipes
	int pipe_fd[2]; // Aca se guardan los dos extremos

	// Luego los jugadores
	pid_t player_pid = -1;
	char name_buff[MAX_PLAYER_NAME_SIZE];
	
	for (unsigned int i = 0; i < m->player_count; i++) {
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

		// Armar nombre
		sprintf(name_buff, "Player %c", 'A' + i);

		// Agregar al game state
		writer_enter(m->game_sync);
		strcpy(gs->players[i].name, name_buff);
		
		gs->players[i].pid = player_pid;
		writer_leave(m->game_sync);

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

	for (unsigned int i = 0; i < m->player_count; i++) {
		close(m->pipes[i]);
	}

	// Liberar memoria compartida
	close_shm(m);

	free(m);

	return 0;
}

static const int DX[8] = { 0, 1, 1, 1, 0,-1,-1,-1 };
static const int DY[8] = {-1,-1, 0, 1, 1, 1, 0,-1 };

/*
 * Verifica si un jugador esta bloqueado, devuelve verdadero si lo esta
*/
static void check_blocked(MasterADT m, int player_id) {

	GameState *gs = m->game_state;

	reader_enter(m->game_sync);

	// Chequear si esta bloqueado
	const Player *p = &gs->players[player_id];

    const int x = p->x, y = p->y;

    // Revisar las 8 adyacencias
    for (int d = 0; d < 8; d++) {
        const int nx = x + DX[d];
        const int ny = y + DY[d];

        if (nx < 0 || ny < 0 || nx >= gs->width || ny >= gs->height)
            continue;

        const int cell = gs->board[ny * gs->width + nx];
        if (cell > 0) {
            gs->players[player_id].blocked = false;
			reader_leave(m->game_sync);

			return ;
        }
    }

	gs->players[player_id].blocked = true;
	reader_leave(m->game_sync);
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
        is_invalid_move = true;
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

	check_blocked(m, player_id);

	// Notificar al jugador que puede enviar otro movimiento
	sem_post(&m->game_sync->player_can_move[player_id]);	

	return is_invalid_move;
}

static void pipe_set_blocked(MasterADT m, int id) {

	m->pipes[id] = -1; // Marcarlo como invalido

	// Buscar nuevo maximo
	m->pipes_max_fd = -1;
	
	for (unsigned int i = 0; i < m->player_count; i++) {
		if (m->pipes[i] > m->pipes_max_fd) {
			m->pipes_max_fd = m->pipes[i];
		}
	}
}

/**
 * Funcion que pregunta si todos los jugadores pueden moverse o no
 */
bool no_player_can_move(MasterADT m) {
    const GameState *gs = m->game_state;
    for (size_t i = 0; i < m->player_count; i++) {

		reader_enter(m->game_sync);
		bool is_blocked = gs->players[i].blocked;
		reader_leave(m->game_sync);

        if (!is_blocked){
			return false;
		}
    }

    // Nadie pudo moverse (o todos estaban bloqueados)
    return true;
}

static int game_start(MasterADT m) {

    struct timeval tv = {m->timeout, 0};
    time_t last_time = time(NULL);

	while (1) {

		// Sincronizacion vista
		sem_post(&m->game_sync->state_change);

		sem_wait(&m->game_sync->render_done);

        wait_delay(m->delay);

        // Actualizar timeout
        int elapsed = time(NULL) - last_time;
        tv.tv_sec = m->timeout - elapsed;

        // Seleccionar siguiente jugador, pipes_set queda solo con los fd que no estan bloqueados
		int ready = select(m->pipes_max_fd + 1, &m->pipes_set, NULL, NULL, &tv);

        if(no_player_can_move(m) || ready == 0 || elapsed >= m->timeout) {
			writer_enter(m->game_sync);
            m->game_state->finished = 1;
			writer_leave(m->game_sync);
			
			// Notificar a la vista, para que no se quede esperando
			sem_post(&m->game_sync->state_change);

			return 0;
        }

		if (ready < 0) {
			writer_enter(m->game_sync);
            m->game_state->finished = 1;
			writer_leave(m->game_sync);

			sem_post(&m->game_sync->state_change);

            perror("MASTER::GAME_START: Error with select");
            return -1;

        }

		// Si llegue aca entonces value > 0
		for (unsigned int i = 0; i < m->player_count; i++) {

			reader_enter(m->game_sync);
			bool is_blocked = m->game_state->players[i].blocked;
			reader_leave(m->game_sync);

			if (m->pipes[i] == -1 || is_blocked)	// Ignorar
				continue;

			if (FD_ISSET(m->pipes[i], &m->pipes_set)) {
                // LUEGO leer el movimiento
                if(!check_player(m, i)){//si es valido
                    last_time = time(NULL); // resetear timeout
                }
            } else {
				// Esta bloqueado, actualizar data
				pipe_set_blocked(m, i);
			}
		}
	}

	return 0;
}

void print_final_results(const MasterADT m) {
    const GameState *gs = m->game_state;

    // limpiar pantalla
    printf("\033[2J\033[H");

    // Avisar a la vista que se termino
    sem_post(&m->game_sync->state_change);
    sem_wait(&m->game_sync->render_done);

    int v_status = 0;
    waitpid(m->view_pid, &v_status, 0);
    printf("View exited (%d)\n", v_status);

	int winner_id = -1;
	char winner_name[MAX_PLAYER_NAME_SIZE] = {0};

	// Data de cada jugador
	char player_name[MAX_PLAYER_NAME_SIZE] = {0};
	pid_t pid;
	unsigned int score, valid_reqs, invalid_reqs;

	for (unsigned i = 0; i < m->player_count; i++) {

		// Leer los datos necesarios del state
		reader_enter(m->game_sync);

		const Player *p = &gs->players[i];

		pid = p->pid;
		score = p->score;
		valid_reqs = p->valid_reqs;
		invalid_reqs = p->invalid_reqs;
		strcpy(player_name, p->name);
		
		reader_leave(m->game_sync);

		sem_post(&m->game_sync->player_can_move[i]); // Por si algun jugador quedo esperando
		int status = 0;

		// Esperar a que termine el proceso hijo
		waitpid(pid, &status, 0);

		// Usamos \r\n por si la TTY quedó sin traducción de NL
		printf("[%s] %s (%u) PID(%u) exited(%u) with a score of %u / %u / %u\r\n",
			   player_name, m->player_path[i], i, pid, status, score, valid_reqs, invalid_reqs);

		reader_enter(m->game_sync);
		if (
			winner_id == -1 || (score > gs->players[winner_id].score) ||
			(score == gs->players[winner_id].score && valid_reqs > gs->players[winner_id].valid_reqs) ||
			(score == gs->players[winner_id].score && valid_reqs == gs->players[winner_id].valid_reqs && invalid_reqs < gs->players[winner_id].invalid_reqs)
		) {
			winner_id = i;
			strcpy(winner_name, player_name);
		}
		reader_leave(m->game_sync);

	}

	printf("\nChompChamp Champion: [%s] %s\n", winner_name, m->player_path[winner_id]);
}

static void show_game_info(const MasterADT m) {
	const GameState *st = m->game_state;
	// limpiar pantalla
	printf("\033[2J\033[H");

	reader_enter(m->game_sync);

	printf("width: %u\n", st->width);
	printf("height: %u\n", st->height);
	printf("delay: %d\n", m->delay);
	printf("timeout: %d\n", m->timeout);
	printf("seed: %d\n", m->seed);
	printf("view: %s\n", m->view_path);
	printf("num_players: %u\n", st->player_count);
	for (unsigned i = 0; i < st->player_count; i++) {
		printf("[Player: %c]  %s\n", '0' + i, m->player_path[i]);
	}

	reader_leave(m->game_sync);

	sleep(2); // Pausa para que el usuario pueda leer
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
		printf("Usage: ./master.out [-w width] [-h height] [-d delay] [-t timeout] [-s seed] [-v view] [-p player1 player2...]\n");

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
	if (game_start(m) == -1)
		printf("MASTER::MAIN: Main loop returned with error\n");

	//limpiar pantalla y mostrar resultados
	print_final_results(m);

	// Una vez termino todo, liberar recursos
	cleanup(m);
	return 0;
}
