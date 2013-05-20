OPTIMISE = -Os
STD = gnu11

all:
	mkdir -p bin
	$(CC) $(OPTIMISE) -std=$(STD) -Wall -Wextra -pedantic -c src/frames.c -o bin/frames.o
	$(CC) $(OPTIMISE) -std=$(STD) -Wall -Wextra -pedantic -o bin/zecora src/zecora.c bin/frames.o

