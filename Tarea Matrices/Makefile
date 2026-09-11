CC = gcc
CFLAGS = -std=c11 -O2 -Wall -Wextra -Wpedantic

matrices: matrices.c
	$(CC) $(CFLAGS) matrices.c -o matrices

run: matrices
	./matrices $(ARGS)

clean:
	$(RM) matrices matrices.exe

.PHONY: run clean
