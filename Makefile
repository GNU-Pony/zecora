OPTIMISE = -Os
STD = gnu11
WARNS = -Wall -Wextra -pedantic
LDFLAGS = -static
X = 


.PHONY: all
all: bin/zecora

obj/%.o: src/frames.h

obj/%.o: src/%.c src/%.h src/types.h
	@mkdir -p obj
	$(CC) $(OPTIMISE) -std=$(STD) $(WARNS) $(X) -c -o $@ $<

bin/zecora: obj/frames.o obj/zecora.o
	@mkdir -p bin
	$(CC) $(OPTIMISE) -std=$(STD) $(WARNS) $(LDFLAGS) $(X) -o $@ $^


.PHONY: clean
clean:
	-rm -r obj bin

