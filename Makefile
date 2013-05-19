OPTIMIZE = -Os
STD = gnu11

all:
	mkdir -p bin
	$(CC) $(OPTIMIZE) -std=$(STD) -c src/frames.c -o bin/frames.o
	$(CC) $(OPTIMIZE) -std=$(STD) -o bin/zecora src/zecora.c bin/frames.o

