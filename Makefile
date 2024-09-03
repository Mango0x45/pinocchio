.POSIX:

MAKEFLAGS = -j8

CFLAGS = -O3 -march=native -mtune=native -pipe -Wall -Wextra -Wpedantic

target = pinocchio
objs = src/lexer.o src/main.o src/parser.o

all: $(target)
$(target): $(objs)
	$(CC) $(CFLAGS) -o $@ $(objs)

src/lexer.o:  src/lexer.c src/lexer.h src/parser.h
src/main.o:   src/main.c src/lexer.h src/parser.h src/pinocchio.h
src/parser.o: src/parser.c src/lexer.h src/parser.h src/pinocchio.h

src/lexer.c src/lexer.h: src/lexer.l
	flex --header-file=src/lexer.h -o src/lexer.c $<

src/parser.c src/parser.h: src/parser.y
	bison -dvo src/parser.c $<

clean:
	rm -f $$(git ls-files -oi --exclude-standard)
