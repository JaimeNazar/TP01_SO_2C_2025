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
