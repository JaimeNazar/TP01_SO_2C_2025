all: view dummy_player

view: view.c
	gcc $< -lncurses -Wall -o $@

dummy_player: dummy_player.c
	gcc $< -Wall -o $@

clean:
	rm -f view dummy_player

.PHONY: all clean