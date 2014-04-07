# Copying and distribution of this file, with or without modification,
# are permitted in any medium without royalty provided the copyright
# notice and this notice are preserved.  This file is offered as-is,
# without any warranty.


ifeq ($(DEBUG),yes)
OPTIMISE = -DDEBUG -O0 -Og -g
else
OPTIMISE = -Os -Wl,--gc-sections -fdata-sections -ffunction-sections -s \
           -ffast-math -fomit-frame-pointer
USE_UPX = yes
STATIC = -static -fwhole-program
endif
STD = gnu11
WARN = -Wall -Wextra -pedantic -Wdouble-promotion -Wformat=2 -Winit-self -Wmissing-include-dirs \
       -Wfloat-equal -Wmissing-prototypes -Wmissing-declarations -Wtrampolines -Wnested-externs \
       -Wno-variadic-macros -Wdeclaration-after-statement -Wundef -Wpacked -Wunsafe-loop-optimizations \
       -Wbad-function-cast -Wwrite-strings -Wlogical-op -Wstrict-prototypes -Wold-style-definition \
       -Wvector-operation-performance -Wstack-protector -Wunsuffixed-float-constants -Wcast-align \
       -Wsync-nand -Wshadow -Wredundant-decls -Winline -Wcast-qual -Wsign-conversion -Wstrict-overflow
X = 

FLAGS = $(OPTIMISE) -std=$(STD) $(WARN) $(X) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS)


.PHONY: all
all: bin/zecora

obj/%.o: src/frames.h

obj/%.o: src/%.c src/%.h src/types.h
	@mkdir -p obj
	$(CC) $(FLAGS) -c -o $@ $<

bin/zecora: obj/frames.o obj/zecora.o
	@mkdir -p bin
	$(CC) $(FLAGS) -o $@ $^
ifeq ($(USE_UPX),yes)
	upx --best --ultra-brute $@
endif


.PHONY: clean
clean:
	-rm -r obj bin

