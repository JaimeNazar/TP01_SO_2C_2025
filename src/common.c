// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "common.h"

void writer_enter(GameSync* sync) {
	sem_wait(&sync->master_mutex);
	sem_wait(&sync->state_mutex);
}

void writer_leave(GameSync* sync) {
	sem_post(&sync->state_mutex);
	sem_post(&sync->master_mutex);
}

void reader_enter(GameSync* sync) {
    // Para evitar inanicion del master
    sem_wait(&sync->master_mutex); // Si esta el master esperando, no entrar a la memoria

	sem_wait(&sync->reader_count_mutex);
	
	// Si soy el unico lector, verificar que el master no este escribiendo
	if (sync->reader_count == 0) {
        sem_wait(&sync->state_mutex);
	}

	// Actualizar contador
    sync->reader_count++;
	
	// Liberar variable
	sem_post(&sync->reader_count_mutex);

    // Volver a subir el semaforo, este se bloquea solo si el master hace wait 
    sem_post(&sync->master_mutex); // Permite que otros lectores entren si no hay escritores
}

void reader_leave(GameSync* sync) {
    // Actualizar variable
	sem_wait(&sync->reader_count_mutex);
	sync->reader_count--;

	// Si nadie mas esta leyendo, notificar al master que puede escribir
	if (sync->reader_count == 0) {
	    sem_post(&sync->state_mutex);
	}

	// Liberar variable
	sem_post(&sync->reader_count_mutex);
}

GameState* open_game_state(unsigned int width, unsigned int height) {

	// Tener en consideracion el tablero
	int state_size = sizeof(GameState) + sizeof(int) * width * height; 

    int fd = shm_open(GAME_STATE_SHM, O_RDONLY, 0666);   // Abrir memoria compartida
    if (fd == -1) {
        perror("COMMON::GET_GAME_STATE: Error opening shmem\n");
    }

    return mmap(0, state_size, PROT_READ, MAP_SHARED, fd, 0); // Mapear la memoria

}

GameSync* open_game_sync() {
	int fd = shm_open(GAME_SYNC_SHM, O_RDWR, 0666); 
    if (fd == -1) {
        perror("COMMON::GET_GAME_SYNC: Error opening shmem\n");
    }

    return mmap(0, sizeof(GameSync), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
}
