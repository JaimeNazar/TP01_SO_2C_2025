#include "playerADT.h"

unsigned char choose_move(PlayerADT p) {
    return 0; // siempre arriba
}

int main(int argc, char **argv) {
    PlayerADT p = init_player(argc, argv);
    if (p == NULL)
        return -1;

    if (init_shm(p) == -1)
        return -1;

    while (1) {
        get_state_snapshot(p);

        if (!still_playing(p))
            break;

        unsigned char move = choose_move(p);
        send_movement(p, move);
    }

    return 0;
}
