#include "chip8.h"
#include "SDL.h"

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#define SCREEN_WIDTH 64 * 8
#define SCREEN_HEIGHT 32 * 8
#define REFRESH_RATE 700

static int keymap[16] = 
{
    SDL_SCANCODE_0,
    SDL_SCANCODE_1,
    SDL_SCANCODE_2,
    SDL_SCANCODE_3,
    SDL_SCANCODE_4,
    SDL_SCANCODE_5,
    SDL_SCANCODE_6,
    SDL_SCANCODE_7,
    SDL_SCANCODE_8,
    SDL_SCANCODE_9,
    SDL_SCANCODE_A,
    SDL_SCANCODE_B,
    SDL_SCANCODE_C,
    SDL_SCANCODE_D,
    SDL_SCANCODE_E,
    SDL_SCANCODE_F
};

unsigned char pixel_coords[64][32];

unsigned char font[80] =
{
    0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
    0x20, 0x60, 0x20, 0x20, 0x70, // 1
    0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
    0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
    0x90, 0x90, 0xF0, 0x10, 0x10, // 4
    0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
    0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
    0xF0, 0x10, 0x20, 0x40, 0x40, // 7
    0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
    0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
    0xF0, 0x90, 0xF0, 0x90, 0x90, // A
    0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
    0xF0, 0x80, 0x80, 0x80, 0xF0, // C
    0xE0, 0x90, 0x90, 0x90, 0xE0, // D
    0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
    0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

SDLapp app;

void initialise_chip8(CHP *chip8)
{	
    chip8->PC = 0x200;
    chip8->SP = 0x000;

    chip8->DT = 0x000;
    chip8->ST = 0x000;

    chip8->I = 0x000;

    // Zero out memory
    memset(chip8->memory, 0, sizeof(chip8->memory));
    memset(chip8->stack, 0, sizeof(chip8->stack));
    memset(chip8->V, 0, sizeof(chip8->V));

    // Load the font into memory
    memcpy(chip8->memory, font, sizeof(font));
}


void load_rom(const char* rom_path, CHP *chip8)
{
    FILE *rom = fopen(rom_path, "rb");

    if (rom == NULL)
    {
        printf("Invalid ROM path: '%s'\n", rom_path);
    } else {
        // Load the program into the program space of memory
        fread(chip8->memory + 0x200, sizeof(unsigned char), sizeof(chip8->memory) - 0x200, rom);
        fclose(rom);
    }
}


unsigned short fetch(CHP *chip8)
{	

    unsigned short large = (unsigned short)chip8->memory[chip8->PC] << 8;
    unsigned short small = (unsigned short)chip8->memory[chip8->PC + 1];
	
    // Combine large and small bytes to get final opcode
    unsigned short opcode = large | small;

    return opcode;
}	


void decode(unsigned short opcode, CHP *chip8)
{
    unsigned short x;
    unsigned short y;
    unsigned short n;
    unsigned short value;

    Uint8 *keyboard;

    switch (opcode & 0xF000)
    {
        case 0x0000:
            switch (opcode)
            {
                case 0x00E0: // 00E0: Clear the screen
                    SDL_SetRenderDrawColor(app.renderer, 0, 0, 0, 255);
                    SDL_RenderClear(app.renderer);

                    break;
                case 0x00EE: // 00EE: Returning from a subroutine
                    chip8->SP -= 1;
                    chip8->PC = chip8->stack[chip8->SP];
					
                    break;
            }
            break;
        case 0x1000: // 1NNN: Jump
            chip8->PC = opcode & 0x0FFF;

            break;
        case 0x2000: // 2NNN: Calls subroutine at memory location NNN
            chip8->stack[chip8->SP] = chip8->PC;
			
            // Increase the stack pointer
            chip8->SP += 1;

            // Change value of program counter
            chip8->PC = opcode & 0x0FFF;

            break;
        case 0x3000: // 3XNN: Skip
            x = (opcode & 0x0F00) >> 8;
            value = opcode & 0x00FF;

            if (chip8->V[x] == value)
            {
                chip8->PC += 2;
            }
				
            break;
        case 0x4000: // 4XNN: Skip
            x = (opcode & 0x0F00) >> 8;
            value = opcode & 0x00FF;

            if (chip8->V[x] != value)
            {
                chip8->PC += 2;
            }

            break;
        case 0x5000: // 5XY0: Skip
            x = (opcode & 0x0F00) >> 8;
            y = (opcode & 0x00F0) >> 4;
				
            if (chip8->V[x] == chip8->V[y])
            {
                chip8->PC += 2;
            }

            break;
        case 0x6000: // 6XNN: Set register VX to NN
            x = (opcode & 0x0F00) >> 8;
            value = opcode & 0x00FF;
            chip8->V[x] = value;

            break;
        case 0x7000: // 7XNN: Add value NN to register VX
            x = (opcode & 0x0F00) >> 8;
            value = opcode & 0x00FF;
            chip8->V[x] += value;

            break;
        case 0x8000:
            x = (opcode & 0x0F00) >> 8;
            y = (opcode & 0x00F0) >> 4;

            switch (opcode & 0x000F)
            {
                case 0x0000: // 8XY0: Set VX to VY
                    chip8->V[x] = chip8->V[y];

                    break;
                case 0x0001: // 8XY1: Set VX to VX|VY OR
                    chip8->V[x] = (chip8->V[x] | chip8->V[y]);
	
                    break;
                case 0x0002: // 8XY2: Set VX to VX&VY AND
                    chip8->V[x] = (chip8->V[x] & chip8->V[y]);

                    break;
                case 0x0003: // 8XY3: Set VX to VX^VY XOR
                    chip8->V[x] = (chip8->V[x] ^ chip8->V[y]);

                    break;
                case 0x0004: // 8XY4: Set VX to VX+VY, set VF to 1 if overflow and 0 if not
                    value = chip8->V[x] + chip8->V[y];

                    chip8->V[0xF] = 0;

                    if (value > 255) {
                        chip8->V[0xF] = 1;	
                    }

                    chip8->V[x] += chip8->V[y];

                    break;
                case 0x0005: // 8XY5: Set VX to VX-VY, set VF to 0 if underflow and 1 if not
                    chip8->V[0xF] = 0;

                    if (chip8->V[x] > chip8->V[y])
                    {
                        chip8->V[0xF] = 1;
                    }
					
                    chip8->V[x] -= chip8->V[y];

                    break;
                case 0x0006: // 8XY6: Set VF to LSB of VX then shift VX to right by 1
                    chip8->V[0xF] = 0 < ((chip8->V[x] << 7) & 0xFF);

                    chip8->V[x] = (chip8->V[x] >> 1);

                    break;
                case 0x0007: // 8XY7: Set VX to VY-VX, set VF to 0 if underflow and 1 if not	
                    chip8->V[0xF] = 0;

                    if (chip8->V[x] < chip8->V[y])
                    {
                        chip8->V[0xF] = 1;
                    }

                    chip8->V[x] = chip8->V[y] - chip8->V[x];

                    break;
                case 0x000E: // 8XYE: Set VF to MSB of VX then shift VX to left by 1
                    chip8->V[0xF] = 0 < ((chip8->V[x] >> 7) & 0xFF);

                    chip8->V[x] = (chip8->V[x] << 1);

                    break;
            }

            break;
        case 0x9000: // 9XY0: Skip
            x = (opcode & 0x0F00) >> 8;
            y = (opcode & 0x00F0) >> 4;

            if (chip8->V[x] != chip8->V[y])
            {
                chip8->PC += 2;
            }

            break;
        case 0xA000: // ANNN: Set index
            value = opcode & 0x0FFF;
            chip8->I = value;

            break;
        case 0xB000: // BNNN: Jump with offset
            chip8->PC = (opcode & 0x0FFF) + chip8->V[0];

            break;
        case 0xC000: // CXNN: Random
            x = (opcode & 0x0F00) >> 8;
            value = opcode & 0x00FF;

            chip8->V[x] = rand() & value;
            break;
        case 0xD000: // DXYN: Display
            n = opcode & 0x000F;
	
            chip8->V[0xF] = 0;

            for (int i = 0; i < n; i++)
            {
                char data = chip8->memory[chip8->I + i];
				
                for (int j = 0; j < 8; j++)
                {
                    x = (chip8->V[(opcode & 0x0F00) >> 8] + j) & 63;
                    y = (chip8->V[(opcode & 0x00F0) >> 4] + i) & 31;

                    if (data & (0x80 >> j))
                    {
                        if (pixel_coords[x][y])
                        {
                            chip8->V[0xF] = 1;
                            pixel_coords[x][y] = 0; // Turn off the pixel
                            SDL_SetRenderDrawColor(app.renderer, 0, 0, 0, 255);
                            draw_pixel(x, y);
                        } else
                        {
                            pixel_coords[x][y] = 1; // Turn on the pixel 
                            SDL_SetRenderDrawColor(app.renderer, 255, 255, 255, 255);
                            draw_pixel(x, y);
                        }
                    }
                }	
            }

            break;
        case 0xE000:
            x = (opcode & 0x0F00) >> 8;
			
            printf("Need to press button: %d\n", chip8->V[x]);

            switch (opcode & 0x00FF)
            {
                case 0x009E: // EX9E: Skip if key
                    keyboard = (Uint8 *)SDL_GetKeyboardState(NULL);
				
                    if (keyboard[keymap[chip8->V[x]]])
                    {
                        chip8->PC += 2;
                    }
			
                    break;
                case 0x00A1: // EXA1: Skip if key
                    keyboard = (Uint8 *)SDL_GetKeyboardState(NULL);		

                    if (!keyboard[keymap[chip8->V[x]]])
                    {
                        chip8->PC += 2;
                    }

                    break;
            }

            break;
        case 0xF000:
            x = (opcode & 0x0F00) >> 8;

            switch (opcode & 0x00FF)
            {
                case 0x0007: // FX07: Sets VX to the current value of the delay timer
                    chip8->V[x] = chip8->DT;
                    break;
                case 0x0015: // FX15: Sets the delay timer to the value in VX
                    chip8->DT = chip8->V[x];
                    break;
                case 0x0018: // FX18: Sets the sound timer to the value in VX
                    chip8->ST = chip8->V[x];
                    break;
                case 0x001E: // FX1E: Add to index
                    chip8->I += chip8->V[x];
                    break;
                case 0x000A: // FX0A: Get key
                    keyboard = (Uint8 *)SDL_GetKeyboardState(NULL);

                    for (int i = 0; i < 0x10; i++)
                    {
                        if (keyboard[keymap[i]])
                        {
                            chip8->V[x] = i;
                            chip8->PC += 2;
                        }
                    }
					
                    break;
                case 0x0029: // FX29: Font character
                    chip8->I = chip8->V[x] * 5;
                    break;
                case 0x0033: // FX33: Binary-coded decimal conversion
                    chip8->memory[chip8->I] = chip8->V[x] / 100;
                    chip8->memory[chip8->I + 1] = (chip8->V[x] / 10) % 10;
                    chip8->memory[chip8->I + 2] = chip8->V[x] % 10;
                    break;
                case 0x0055: // FX55: Store memory
                    for (int i = 0; i <= x; i++)
                    {
                        chip8->memory[chip8->I + i] = chip8->V[i];
                    }
                    break;
                case 0x0065: // FX65: Load memory
                    for (int i = 0; i <= x; i++)
                    {
                        chip8->V[i] = chip8->memory[chip8->I + i];
                    }
                    break;
            }

            break;
        default:
            printf("Invalid instruction.\n");
            exit(-1);

            break;
    }
}


void update(CHP *chip8)
{
    if (chip8->DT > 0)
    {
        chip8->DT -= 1;
    }
    if (chip8->ST > 0)
    {
        chip8->ST -= 1;
    }

    unsigned short opcode = fetch(chip8);
	
    // Increment the program counter
    chip8->PC += 2;

    decode(opcode, chip8);

}	

void draw_pixel(unsigned int x, unsigned int y)
{
    for (int i = 0; i < 8; i++)
    {
        for (int j = 0; j < 8; j++)
        {	
            SDL_RenderDrawPoint(app.renderer, (x * 8) + i , (y * 8) + j);
        }
    }	
}

int main(int argc, char *argv[])
{
    if (argc == 1)
    {
        printf("Usage: %s <rom-path>\n", argv[0]);
        return -1;
    }

    CHP chip8;
    initialise_chip8(&chip8);
    load_rom(argv[1], &chip8);

    // Seed random values
    srand(time(NULL));

    if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_TIMER) != 0)
    {
        printf("There has been an error initialising SDL.\n%s\n", SDL_GetError());
        return -1;
    }
	
    app.window = SDL_CreateWindow(
            "CHIP-8",
            SDL_WINDOWPOS_UNDEFINED,
            SDL_WINDOWPOS_UNDEFINED,
            SCREEN_WIDTH,
            SCREEN_HEIGHT,
            0
    );

    if (!app.window)
    {
        printf("There has been an error creating the window.\n%s\n", SDL_GetError());
        return -1;
    }

    app.renderer = SDL_CreateRenderer(app.window, -1, SDL_RENDERER_SOFTWARE);

    if (!app.renderer)
    {
        printf("There has been an error creating the renderer.\n%s\n", SDL_GetError());
        return -1;
    }

    // Set initial render draw color
    SDL_SetRenderDrawColor(app.renderer, 0, 0, 0, 255);

    SDL_Event e;

    int quit = 0;
    while (!quit)
    {
        while (SDL_PollEvent(&e) != 0)
        {
            if (e.type == SDL_QUIT)
            {
                quit = 1;
            }
        }
            
        update(&chip8);

        SDL_RenderPresent(app.renderer);

        // Enforce FPS
        SDL_Delay(1000/REFRESH_RATE);
    }

    SDL_DestroyRenderer(app.renderer);
    SDL_DestroyWindow(app.window);
    SDL_Quit();

    return 0;
}