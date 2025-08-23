#include<stdio.h>
#include<string.h>

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

int main (int argc, char *argv[]) {

	MasterCDT m = { 0 };

	if (parse_args(&m, argc, argv)) {
		printf("Usage: ./ChompChamps [-w width] [-h height] [-d delay] [-t timeout] [-s seed] [-v view] [-p player1 player2...]\n");

		return -1;
	}
	

	return 0;
}
