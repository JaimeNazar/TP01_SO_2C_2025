#include <semaphore.h>  
#include <stdbool.h>

#define GAME_STATE_SHM "/game_state"
#define GAME_SYNC_SHM "/game_sync"

typedef struct { 
    char name[16]; // Nombre del jugador
    unsigned int score; // Puntaje
    unsigned int invalid_reqs; // Cantidad de solicitudes de movimientos inválidas realizadas
    unsigned int valid_reqs; // Cantidad de solicitudes de movimientos válidas realizadas
    unsigned short x, y; // Coordenadas x e y en el tablero
    pid_t pid; // Identificador de proceso
    bool blocked; // Indica si el jugador está bloqueado
} Player;

typedef struct {
    unsigned short width; // Ancho del tablero
    unsigned short height; // Alto del tablero
    unsigned int player_count; // Cantidad de jugadores
    Player players[9]; // Lista de jugadores
    bool finished; // Indica si el juego se ha terminado
    int board[]; // Puntero al comienzo del tablero. fila-0, fila-1, ..., fila-n-1
} GameState;

// Cambienlos dsp
typedef struct {
    sem_t state_change;     // El máster le indica a la vista que hay cambios por imprimir
    sem_t render_done;      // La vista le indica al máster que terminó de imprimir
    sem_t master_mutex;     // Mutex para evitar inanición del máster al acceder al estado
    sem_t state_mutex;  // Mutex para el estado del juego
    sem_t reader_count_mutex;         // Mutex para la siguiente variable
    unsigned int reader_count; // Cantidad de jugadores leyendo el estado
    sem_t player_can_move[9];      // Le indican a cada jugador que puede enviar 1 movimiento
} GameSync;
