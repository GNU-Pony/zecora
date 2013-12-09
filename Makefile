OPTIMISE = -Os
STD = gnu11


.PHONY: all
all: bin/zecora

obj/%.o: src/%.c
	@mkdir -p obj
	$(CC) $(OPTIMISE) -std=$(STD) -Wall -Wextra -pedantic -c -o $@ $<

bin/zecora: obj/frames.o obj/zecora.o
	@mkdir -p bin
	$(CC) $(OPTIMISE) -std=$(STD) -Wall -Wextra -pedantic -o $@ $^


.PHONY: clean
clean:
	-rm -r obj bin

