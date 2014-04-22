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
WARN = -Wall -Wextra -Wdouble-promotion -Wformat=2 -Winit-self -Wmissing-include-dirs         \
       -Wtrampolines -Wfloat-equal -Wshadow -Wmissing-prototypes -Wmissing-declarations       \
       -Wredundant-decls -Wnested-externs -Winline -Wno-variadic-macros -Wsign-conversion     \
       -Wswitch-default -Wconversion -Wsync-nand -Wunsafe-loop-optimizations -Wcast-align     \
       -Wstrict-overflow -Wundef -Wbad-function-cast -Wcast-qual -Wwrite-strings -Wpacked     \
       -Wlogical-op -Waggregate-return -Wstrict-prototypes -Wold-style-definition             \
       -Wvector-operation-performance -Wunsuffixed-float-constants -Wsuggest-attribute=const  \
       -Wsuggest-attribute=noreturn -Wsuggest-attribute=pure -Wsuggest-attribute=format       \
       -Wnormalized=nfkc
F_OPTS = -ftree-vrp -fstrict-aliasing -fipa-pure-const -fstack-usage -fstrict-overflow        \
         -funsafe-loop-optimizations -fno-builtin
# Not used:
#  -pedantic -Wdeclaration-after-statement
X = 

FLAGS = $(OPTIMISE) -std=$(STD) $(WARN) $(F_OPTS) $(X) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS)


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

