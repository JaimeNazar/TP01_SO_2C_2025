COMPILER = gcc
COMPILER_FLAGS = -D_POSIX_C_SOURCE=200112L --std=c99 -pedantic -pedantic-errors -Wall -Wextra -Werror -Wno-unused-parameter -Wno-implicit-fallthrough -I./include

OBJECTS := $(wildcard *.o)

all: view dummy_player greedy_player free_player

view: view.c
	$(COMPILER) $< -lncurses $(COMPILER_FLAGS) -o ../$@.out

dummy_player: dummy_player.c
	$(COMPILER) $< $(COMPILER_FLAGS) -o ../$@.out

greedy_player: greedy_player.c
	$(COMPILER) $< $(COMPILER_FLAGS) -o ../$@.out

free_player: free_player.c
	$(COMPILER) $< $(COMPILER_FLAGS) -o ../$@.out

clean:
	@rm -rf $(OBJECTS)
	@rm -rf ../view.out ../dummy_player.out ../greedy_player.out ../free_player.out

.PHONY: all clean
