OPTIMISE = -Os -Wl,--gc-sections -fdata-sections -ffunction-sections -s \
           -ffast-math -fomit-frame-pointer
STD = gnu11
WARNS = -Wall -Wextra -pedantic
STATIC = -static -fwhole-program
X = 


.PHONY: all
all: bin/zecora

obj/%.o: src/frames.h

obj/%.o: src/%.c src/%.h src/types.h
	@mkdir -p obj
	$(CC) $(OPTIMISE) -std=$(STD) $(WARNS) $(X) -c -o $@ $<

bin/zecora: obj/frames.o obj/zecora.o
	@mkdir -p bin
	$(CC) $(OPTIMISE) -std=$(STD) $(WARNS) $(STATIC) $(X) -o $@ $^
	upx --best --ultra-brute $@


.PHONY: clean
clean:
	-rm -r obj bin

