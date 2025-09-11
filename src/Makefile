COMPILER = gcc
COMPILER_FLAGS = -D_POSIX_C_SOURCE=200112L --std=c99 -pedantic -pedantic-errors -Wall -Wextra -Werror -Wno-unused-parameter -Wno-implicit-fallthrough -I./include

OBJECTS := $(wildcard *.o)

OBJS := common.o playerADT.o
TARGETS := view master dummy_player greedy_player free_player v2 v1 always_up perfect_cell

all: $(TARGETS)

%.o: %.c
	$(COMPILER) -c $^ $(COMPILER_FLAGS)  -o $@

view: view.c $(OBJS)
	$(COMPILER) $(COMPILER_FLAGS) $^ -lncurses -o ../$@.out

master: master.c $(OBJS)
	$(COMPILER) $(COMPILER_FLAGS) common.o $< -o ../$@.out

dummy_player: dummy_player.c $(OBJS)
	$(COMPILER) $(COMPILER_FLAGS) $^ -o ../$@.out

greedy_player: greedy_player.c $(OBJS)
	$(COMPILER) $(COMPILER_FLAGS) $^ -o ../$@.out

free_player: free_player.c $(OBJS)
	$(COMPILER) $(COMPILER_FLAGS) $^ -o ../$@.out

v2: v2.c $(OBJS)
	$(COMPILER) $(COMPILER_FLAGS) $^ -o ../$@.out

v1: v1.c $(OBJS)
	$(COMPILER) $(COMPILER_FLAGS) $^ -o ../$@.out

always_up: always_up.c $(OBJS)
	$(COMPILER) $(COMPILER_FLAGS) $^ -o ../$@.out

perfect_cell: perfect_cell.c $(OBJS)
	$(COMPILER) $(COMPILER_FLAGS) $^ -o ../$@.out

clean:
	@rm -rf $(OBJECTS) $(OBJS)
	@rm -rf ../view.out ../dummy_player.out ../greedy_player.out ../free_player.out ../v2.out ../v1.out ../always_up.out ../perfect_cell.out ../master.out

.PHONY: all clean
