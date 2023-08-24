CC = gcc
CFLAGS = -ansi -Wall -g -O0 -Wwrite-strings -Wshadow \
         -pedantic-errors -fstack-protector-all 
PROGS = d8sh

.PHONY: all clean

all: $(PROGS)

clean:
	rm -f *.o $(PROGS) *.tmp

d8sh: lexer.o parser.tab.o executor.o d8sh.o
	$(CC) lexer.o parser.tab.o executor.o d8sh.o -o d8sh -lreadline

lexer.o: lexer.c parser.tab.h
	$(CC) $(CFLAGS) -c lexer.c

parser.tab.o: parser.tab.c command.h parser.tab.h
	$(CC) $(CFLAGS) -c parser.tab.c

executor.o: executor.c command.h executor.h
	$(CC) $(CFLAGS) -c executor.c

d8sh.o: d8sh.c lexer.h executor.h
	$(CC) $(CFLAGS) -c d8sh.c
