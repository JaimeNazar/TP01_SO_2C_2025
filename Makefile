
all: check-dependencies 
	cd src; make all

check-dependencies:
# Verificar si ncurses esta instalado, si no instalarlo.
# Solo para distros que usan apt

ifeq ($(shell test -f /usr/include/ncurses.h && echo yes || echo no), no)
	@echo "ncurses not found. Installing..."
	apt install ncurses-dev
else
	@echo "ncurses found."
endif

clean:
	cd src; make clean
	
format:
	clang-format -style=file --sort-includes --Werror -i ./src/*.c ./src/include/*.h

check:
	cppcheck --quiet --enable=all --force --inconclusive .
	pvs-studio-analyzer trace -- make
	pvs-studio-analyzer analyze
	plog-converter -a '64:1,2,3;GA:1,2,3;OP:1,2,3' -t tasklist -o report.tasks PVS-Studio.log

.PHONY: all clean format check
