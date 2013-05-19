OPTIMISE = -Os
STD = gnu11

all:
	mkdir -p bin
	$(CC) $(OPTIMISE) -std=$(STD) -c src/frames.c -o bin/frames.o
	$(CC) $(OPTIMISE) -std=$(STD) -o bin/zecora src/zecora.c bin/frames.o

