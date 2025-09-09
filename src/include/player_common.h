#ifndef PLAYER_COMMON_H
#define PLAYER_COMMON_H

#include <fcntl.h>  
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>  

#include "common.h"
#include <stdbool.h>

typedef struct PlayerCDT* PlayerADT;

// TODO: Documentar!!

PlayerADT init_player(int argc, char **argv);

int init_shm(PlayerADT p);

void get_state_snapshot(PlayerADT p);

bool still_playing(PlayerADT p);

int send_movement(PlayerADT p, unsigned char move);


///// Commons

bool is_valid_position(GameState *state, int x, int y);

// Devuelve el valor de la celda en (x, y), o -1 si es inválida
int get_board_cell(GameState *state, int x, int y);

// Verifica si la celda en (x, y) está libre (valor entre 1 y 9)
bool is_cell_free(GameState *state, int x, int y);

// Convierte una dirección a desplazamientos dx, dy
void direction_to_offset(unsigned char dir, int *dx, int *dy);

// Cuenta la cantidad de celdas libres alrededor de (x, y)
int count_free_neighbors(GameState *state, int x, int y);

int calculate_depth(GameState *state, int start_x, int start_y, int dx, int dy, int max_depth);

bool has_nearby_players(GameState *state, int x, int y, int my_id);

bool is_potential_trap(GameState *state, int x, int y);

bool is_endgame(GameState *state);

// Para la sincronizacion
void reader_enter(PlayerADT p);
void reader_leave(PlayerADT p);

#endif // PLAYER_COMMON_H
