COMPILER = gcc
COMPILER_FLAGS = -D_POSIX_C_SOURCE=200112L --std=c99 -pedantic -pedantic-errors -Wall -Wextra -Werror -Wno-unused-parameter -Wno-implicit-fallthrough -I./include

OBJECTS := $(wildcard *.o)

all: view master dummy_player greedy_player free_player v2 v1 always_up

view: view.c
	$(COMPILER) $< -lncurses $(COMPILER_FLAGS) -o ../$@.out

master: master.c
	$(COMPILER) $< $(COMPILER_FLAGS) -o ../$@.out

greedy_player: greedy_player.c
dummy_player: dummy_player.c player_common.c
	$(COMPILER) $^ $(COMPILER_FLAGS) -o ../$@.out

greedy_player: greedy_player.c player_common.c
	$(COMPILER) $^ $(COMPILER_FLAGS) -o ../$@.out

free_player: free_player.c player_common.c
	$(COMPILER) $^ $(COMPILER_FLAGS) -o ../$@.out

v2: v2.c player_common.c
	$(COMPILER) $^ $(COMPILER_FLAGS) -o ../$@.out

v1: v1.c player_common.c
	$(COMPILER) $^ $(COMPILER_FLAGS) -o ../$@.out

always_up: always_up.c player_common.c
	$(COMPILER) $^ $(COMPILER_FLAGS) -o ../$@.out

clean:
	@rm -rf $(OBJECTS)
	@rm -rf ../view.out ../dummy_player.out ../greedy_player.out ../free_player.out ../v2.out ../v1.out

.PHONY: all clean
