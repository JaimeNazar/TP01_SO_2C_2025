#include "playerADT.h"

unsigned char choose_move(PlayerADT p) {
    int dx, dy;
    direction_to_offset(0, &dx, &dy);// 0 es la celda arriba del player

        int new_x = get_x(p) + dx;
        int new_y = get_y(p) + dy;

        if (is_cell_free(p, new_x, new_y)) {
            return 0; // siempre arriba
        }
        return -1;
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
