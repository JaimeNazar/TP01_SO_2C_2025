all: view dummy_player

view: view.c
	gcc -Wall -lncurses $< -o $@

dummy_player: dummy_player.c
	gcc -Wall -lncurses $< -o $@

clean:
	rm -f view

.PHONY: all clean