.POSIX:

MAKEFLAGS = -j8

S = src
CFLAGS = -O3 -march=native -mtune=native -pipe -Wall -Wextra -Wpedantic

target = pinocchio
objs = $S/lexer.o $S/main.o $S/parser.o $S/wrapper.o

all: $(target)
$(target): $(objs)
	$(CC) $(CFLAGS) -o $@ $(objs)

$S/lexer.o:  $S/lexer.c  $S/lexer.h $S/parser.h
$S/main.o:   $S/main.c   $S/lexer.h $S/parser.h $S/pinocchio.h $S/wrapper.h
$S/parser.o: $S/parser.c $S/lexer.h $S/parser.h $S/pinocchio.h $S/wrapper.h

$S/lexer.c $S/lexer.h: $S/lexer.l
	flex --header-file=$S/lexer.h -o $S/lexer.c $<

$S/parser.c $S/parser.h: $S/parser.y
	bison -dvo $S/parser.c $<

clean:
	rm -f $$(git ls-files -oi --exclude-standard)
