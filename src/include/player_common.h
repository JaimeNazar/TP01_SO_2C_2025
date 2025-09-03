#ifndef PLAYER_COMMON_H
#define PLAYER_COMMON_H

#include "common.h"
#include <stdbool.h>

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
void reader_enter(GameSync* sync);
void reader_leave(GameSync* sync);

#endif // PLAYER_COMMON_H
