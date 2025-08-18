#include <ncurses.h>
int main(){ initscr(); printw("hello"); getch(); endwin(); return 0; }