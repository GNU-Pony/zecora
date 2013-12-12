ifeq ($(DEBUG),yes)
OPTIMISE = -DDEBUG -O0 -Og -g
else
OPTIMISE = -Os -Wl,--gc-sections -fdata-sections -ffunction-sections -s \
           -ffast-math -fomit-frame-pointer
USE_UPX=yes
STATIC = -static -fwhole-program
endif
STD = gnu11
WARNS = -Wall -Wextra -pedantic
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
ifeq ($(USE_UPX),yes)
	upx --best --ultra-brute $@
endif


.PHONY: clean
clean:
	-rm -r obj bin

