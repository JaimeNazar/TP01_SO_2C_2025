#include <semaphore.h>  

typedef struct { 
    char name[16]; // Nombre del jugador
    unsigned int score; // Puntaje
    unsigned int invalid_reqs; // Cantidad de solicitudes de movimientos inválidas realizadas
    unsigned int valid_reqs; // Cantidad de solicitudes de movimientos válidas realizadas
    unsigned short x, y; // Coordenadas x e y en el tablero
    pid_t pid; // Identificador de proceso
    bool is_blocked; // Indica si el jugador está bloqueado
} Player;

typedef struct {
    unsigned short width; // Ancho del tablero
    unsigned short height; // Alto del tablero
    unsigned int player_count; // Cantidad de jugadores
    Player player_list[9]; // Lista de jugadores
    bool is_finished; // Indica si el juego se ha terminado
    int board[]; // Puntero al comienzo del tablero. fila-0, fila-1, ..., fila-n-1
} GameState;

// Cambienlos dsp
typedef struct {
    sem_t state_change; // El máster le indica a la vista que hay cambios por imprimir
    sem_t render_done; // La vista le indica al máster que terminó de imprimir
    sem_t m_can_access; // Mutex para evitar inanición del máster al acceder al estado
    sem_t game_state_busy; // Mutex para el estado del juego
    sem_t next_var; // Mutex para la siguiente variable
    unsigned int concurrent_players; // Cantidad de jugadores leyendo el estado
    sem_t can_move[9]; // Le indican a cada jugador que puede enviar 1 movimiento
} GameSync;
