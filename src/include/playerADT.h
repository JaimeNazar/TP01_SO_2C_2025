#ifndef PLAYER_ADT_H
#define PLAYER_ADT_H

#include <fcntl.h>  
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>  
#include <stdlib.h>
#include <stdbool.h>

#include "common.h"

typedef struct PlayerCDT* PlayerADT;

// TODO: Documentar!!

PlayerADT init_player(int argc, char **argv);

int p_init_shm(PlayerADT p);

void get_state_snapshot(PlayerADT p);

bool still_playing(PlayerADT p);

int get_board_value(PlayerADT p, unsigned int x, unsigned int y);

unsigned int get_x(PlayerADT p);
unsigned int get_y(PlayerADT p);

unsigned int get_width(PlayerADT p);
unsigned int get_height(PlayerADT p);

unsigned int get_player_count(PlayerADT p);

int get_id(PlayerADT p);
GameState* get_game_state(PlayerADT p);
GameSync* get_game_sync(PlayerADT p);

int send_movement(PlayerADT p, unsigned char move);


///// Commons

bool is_valid_position(PlayerADT p, int x, int y);

// Devuelve el valor de la celda en (x, y), o -1 si es inválida
int get_board_cell(PlayerADT p, int x, int y);

// Verifica si la celda en (x, y) está libre (valor entre 1 y 9)
bool is_cell_free(PlayerADT p, int x, int y);

// Convierte una dirección a desplazamientos dx, dy
void direction_to_offset(unsigned char dir, int *dx, int *dy);

// Cuenta la cantidad de celdas libres alrededor de (x, y)
int count_free_neighbors(PlayerADT p, int x, int y);

int calculate_depth(PlayerADT p, int dx, int dy, int max_depth);

bool has_nearby_players(PlayerADT p, int x, int y, int my_id);

bool is_potential_trap(PlayerADT p, int x, int y);

bool is_endgame(PlayerADT p);

#endif // PLAYER_ADT_H
