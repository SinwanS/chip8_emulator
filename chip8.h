#ifndef CHIP8_HEADER
#define CHIP8_HEADER

#include "SDL.h"

typedef struct 
{
    // Program counter and stack pointer
    unsigned short PC;
    unsigned short SP;

    unsigned short stack[16];
    unsigned char memory[4096];

    // General purpose registers
    unsigned char V[16];
    // Delay timer and sound timer
    unsigned char DT;
    unsigned char ST;

    // Index register
    unsigned short I;
} CHP;

typedef struct
{
    SDL_Window *window;
    SDL_Renderer *renderer;
} SDLapp;

void initialise_chip8(CHP *chip8);

void load_rom(const char* rom_path, CHP *chip8);

void draw_square(unsigned int x, unsigned int y);

unsigned short fetch(CHP *chip8);

void decode(unsigned short opcode, CHP *chip8);

void update(CHP *chip8);

void draw_pixel(unsigned int x, unsigned int y);

#endif