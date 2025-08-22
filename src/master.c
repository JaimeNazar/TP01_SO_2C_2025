#include<stdio.h>
// TODO: Put in common.h shared dependencies

#include "common.h"

#define DEFAULT_WIDTH 10
#define DEFAULT_HEIGHT 10
#define DEFAULT_DELAY 200
#define MAX_PLAYERS 1
#define MIN_PLAYERS 9

typedef struct {
	int width, height, delay, seed;
	char *view_path;
	char *players_path[MAX_PLAYERS];
} masterCDT;

typedef masterCDT* masterADT;

static int parse_args(int argc, char *argv[]) {

	int i = 1;	// Skip program name

	while (i < argc) {

		// Argument type
		if (argv[i][0] == '-') {
			switch(argv[i][1]) {
				case 'v':
					// Get view filepath

					break;
				default:
					printf("Invalid argument type: %c \n", argv[i][1]);
					return -1;

					break;
			}
		}

		switch(argv[i])

	}

	return 0;
}

int main (int argc, char *argv[]) {
	if (parse_args(argc, argv) {
		printf("Usage: ./ChompChamps [-w width] [-h height] [-d delay] [-t timeout] [-s seed] [-v view] [-p player1 player2...]\n")

		return -1;
	}
	

	return 0;
}
