CC = gcc
CFLAGS=-std=c17 -Wall -Wextra -Werror
main: chip8.c
	$(CC) chip8.c -o chip8 $(CFLAGS) `sdl2-config --cflags --libs`