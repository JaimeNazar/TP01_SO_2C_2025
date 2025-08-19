COMPILER = gcc
COMPILER_FLAGS = -c -g --std=c99 -pedantic -pedantic-errors -Wall -Wextra -Werror -Wno-unused-parameter -Wno-implicit-fallthrough -D_POSIX_C_SOURCE=200809L -I./include

all: view dummy_player

view: view.c
	$(COMPILER) $< -lncurses $(COMPILER_FLAGS) -o ../$@

dummy_player: dummy_player.c
	$(COMPILER) $< $(COMPILER_FLAGS) -o ../$@

clean:
	@rm -rf $(OBJECTS)
	@rm -rf ../view ../dummy_player

.PHONY: all clean view dummy_player
