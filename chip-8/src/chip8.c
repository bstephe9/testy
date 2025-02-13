#include "chip8.h"

#include <SDL_mixer.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

unsigned char chip8_fontset[FONT_MEMORY_SIZE] = {
    0xF0, 0x90, 0x90, 0x90, 0xF0,  // 0
    0x20, 0x60, 0x20, 0x20, 0x70,  // 1
    0xF0, 0x10, 0xF0, 0x80, 0xF0,  // 2
    0xF0, 0x10, 0xF0, 0x10, 0xF0,  // 3
    0x90, 0x90, 0xF0, 0x10, 0x10,  // 4
    0xF0, 0x80, 0xF0, 0x10, 0xF0,  // 5
    0xF0, 0x80, 0xF0, 0x90, 0xF0,  // 6
    0xF0, 0x10, 0x20, 0x40, 0x40,  // 7
    0xF0, 0x90, 0xF0, 0x90, 0xF0,  // 8
    0xF0, 0x90, 0xF0, 0x10, 0xF0,  // 9
    0xF0, 0x90, 0xF0, 0x90, 0x90,  // A
    0xE0, 0x90, 0xE0, 0x90, 0xE0,  // B
    0xF0, 0x80, 0x80, 0x80, 0xF0,  // C
    0xE0, 0x90, 0x90, 0x90, 0xE0,  // D
    0xF0, 0x80, 0xF0, 0x80, 0xF0,  // E
    0xF0, 0x80, 0xF0, 0x80, 0x80   // F
};

void initialize(chip8_t *chip8) {
    // Registers
    chip8->pc = 0x200;
    chip8->opcode = 0x0;
    chip8->idx = 0x0;
    chip8->sp = 0x0;

    // V-Registers, stack, memory, keypad
    memset(chip8->V, 0, sizeof(chip8->V));
    memset(chip8->stack, 0, sizeof(chip8->stack));
    memset(chip8->memory, 0, sizeof(chip8->memory));
    memset(chip8->keypad, false, sizeof(chip8->keypad));

    // Graphics
    memset(chip8->display, 0, sizeof(chip8->display));
    memset(&chip8->sdl, 0, sizeof(chip8->sdl));

    // Load fontset into memory
    for (int i = 0; i < FONT_MEMORY_SIZE; i++) {
        chip8->memory[FONT_START + i] = chip8_fontset[i];
    }

    // Set initial state
    chip8->state = RUNNING;
    chip8->draw = false;

    // Seed random number generator
    srand(time(NULL));
}

bool read_rom(uint8_t *buffer, const char *rom_path) {
    FILE *rom = fopen(rom_path, "rb");
    if (!rom) {
        fprintf(stderr, "Unable to open rom\n");
        return false;
    }

    long rom_size = get_rom_size(rom);
    if (rom_size > MAX_ROM_SIZE) {
        fprintf(stderr, "Error: Rom size too large\n");
        return false;
    }

    // Read ROM into buffer
    if (fread(buffer, 1, rom_size, rom) == 0) {
        fprintf(stderr, "Error: Could not load ROM into memory\n");
        return false;
    }

    return true;
}

long get_rom_size(FILE *fp) {
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    rewind(fp);
    return size;
}

bool setup_sdl(sdl_t *sdl) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) == -1) {
        printf("Could not initialize SDL: %s\n", SDL_GetError());
        return false;
    }

    sdl->window = SDL_CreateWindow("CHIP-8 Emulator", WINDOW_X, WINDOW_Y,
                                   DISPLAY_WIDTH * WINDOW_SCALE,
                                   DISPLAY_HEIGHT * WINDOW_SCALE, 0);
    if (sdl->window == NULL) {
        printf("Could not initialize SDL_Window: %s\n", SDL_GetError());
        return false;
    }

    sdl->renderer =
        SDL_CreateRenderer(sdl->window, -1, SDL_RENDERER_ACCELERATED);
    if (sdl->renderer == NULL) {
        printf("Could not initialize SDL_Renderer: %s\n", SDL_GetError());
        return false;
    }

#ifndef UNIT_TEST
    // Initialize playback audio device
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 1, 2048) == -1) {
        printf("Could not initialize audio device: %s\n", Mix_GetError());
        return false;
    }

    // Load beep sound
    sdl->sound = Mix_LoadWAV(SOUND_PATH);
    if (sdl->sound == NULL) {
        printf("Could not load sound file: %s\n", SDL_GetError());
        return false;
    }
#endif

    return true;
}

void emulate_cycle(chip8_t *chip8) {
    // Fetch opcode
    chip8->opcode =
        chip8->memory[chip8->pc] << 8 | chip8->memory[chip8->pc + 1];

    // Increment program counter to next instruction
    chip8->pc += 2;

    uint16_t X = (chip8->opcode & 0x0F00) >> 8;
    uint16_t Y = (chip8->opcode & 0x00F0) >> 4;

    uint32_t N = chip8->opcode & 0x000F;
    uint32_t NN = chip8->opcode & 0x00FF;
    uint32_t NNN = chip8->opcode & 0x0FFF;

    bool key_pressed = false;
    uint8_t n = 0;
    uint8_t random_num = 0;

    // Decode and execute opcode
    switch (chip8->opcode & 0xF000) {
        case 0x0000:
            switch (chip8->opcode & 0x000F) {
                case 0x0000:  // 00E0; Clears the screen.
                    memset(chip8->display, 0, sizeof(chip8->display));
                    break;
                case 0x000E:  // 00EE: Returns from a subroutine.
                    chip8->sp--;
                    chip8->pc = chip8->stack[chip8->sp];
                    break;
                default: break;
            }
            break;
        case 0x1000:  // 1NNN; Jumps to address NNN.
            chip8->pc = NNN;
            break;
        case 0x2000:  // 2NNN; Calls subroutine at NNN.
            chip8->stack[chip8->sp] = chip8->pc;
            chip8->sp++;
            chip8->pc = NNN;
            break;
        case 0x3000:  // 3XNN; Skips the next instruction if VX equals NN.
            if (chip8->V[X] == NN)
                chip8->pc += 2;
            break;
        case 0x4000:  // 4XNN; Skips the next instruction if VX does not equal
                      // NN.
            if (chip8->V[X] != NN)
                chip8->pc += 2;
            break;
        case 0x5000:  // 5XY0; Skips the next instruction if VX equals VY.
            if (chip8->V[X] == chip8->V[Y])
                chip8->pc += 2;
            break;
        case 0x6000:  // 6XNN; Sets VX to NN.
            chip8->V[X] = NN;
            break;
        case 0x7000:  // 7XNN; Adds NN to VX.
            chip8->V[X] += NN;
            break;
        case 0x8000:
            switch (chip8->opcode & 0x000F) {
                case 0x0000:  // 8XY0; Sets VX to the value of VY.
                    chip8->V[X] = chip8->V[Y];
                    break;
                case 0x0001:  // 8XY1; Sets VX to VX or VY.
                    chip8->V[X] |= chip8->V[Y];
                    break;
                case 0x0002:  // 8XY2; Sets VX to VX and VY.
                    chip8->V[X] &= chip8->V[Y];
                    break;
                case 0x0003:  // 8XY3; Sets VX to VX xor VY.
                    chip8->V[X] ^= chip8->V[Y];
                    break;
                case 0x0004:  // 8XY4; Adds VY to VX. VF is set to 1 when
                              // there's an overflow, and to 0 when there is
                              // not.
                    chip8->V[0xF] = (chip8->V[X] + chip8->V[Y]) > 0xFF;
                    chip8->V[X] += chip8->V[Y];
                    break;
                case 0x0005:  // 8XY5; VY is subtracted from VX. VF is set to 0
                              // when there's an underflow, and 1 when there is
                              // not.
                    chip8->V[0xF] = chip8->V[X] < chip8->V[Y];
                    chip8->V[X] -= chip8->V[Y];
                    break;
                case 0x0006:  // 8XY6; Shifts VX to the right by 1, then stores
                              // the least significant bit of VX prior to the
                              // shift into VF.
                    chip8->V[0xF] = chip8->V[X] & 0x0001;
                    chip8->V[X] >>= 1;
                    break;
                case 0x0007:  // 8XY7; Sets VX to VY minus VX. VF is set to 0
                              // when there's an underflow, and 1 when there is
                              // not.
                    chip8->V[0xF] = chip8->V[Y] < chip8->V[X];
                    chip8->V[X] = chip8->V[Y] - chip8->V[X];
                    break;
                case 0x000E:  // 8XYE; Shifts VX to the left by 1, then sets VF
                              // to 1 if the most significant bit of VX prior to
                              // that shift was set, or to 0 if it was unset.
                    chip8->V[0xF] = (chip8->V[X] & 0x80) >> 7;
                    chip8->V[X] <<= 1;
                    break;
                default: break;
            }
            break;
        case 0x9000:  // 9XY0; Skips the next instruction if VX does not equal
                      // VY.
            if (chip8->V[X] != chip8->V[Y])
                chip8->pc += 2;
            break;
        case 0xA000:  // ANNN; Sets I to the address NNN.
            chip8->idx = NNN;
            break;
        case 0xB000:  // BNNN; Jumps to the address NNN plus V0.
            chip8->pc = chip8->V[0x0] + NNN;
            break;
        case 0xC000:  // CXNN; Sets VX to the result of a bitwise AND operation
                      // on a random number and NN.
            random_num = rand() % 256;  // Range: [0, 255]
            chip8->V[X] = random_num & NN;
            break;
        case 0xD000:  // DXYN; Draws a sprite at coordinate (VX, VY) that has a
                      // width of 8 pixels and a height of N pixels.

            // Initialize carry flag
            chip8->V[0xF] = false;
            chip8->draw = true;

            // Loop through each row of the sprite
            for (uint32_t i = 0; i < N; i++) {
                // Get row (byte)
                uint8_t sprite_row = chip8->memory[chip8->idx + i];

                // For each bit in row (starting from the leftmost bit), XOR
                // sprite pixel with display pixel
                for (int j = 0; j <= 7; j++) {
                    uint8_t current_bit_position = 1 << (7 - j);
                    uint8_t current_bit = sprite_row & current_bit_position;

                    bool sprite_pixel = (bool)current_bit;

                    uint8_t display_x = chip8->V[X] + j;
                    uint8_t display_y = chip8->V[Y] + i;
                    uint16_t display_pixel_index =
                        (DISPLAY_WIDTH * display_y) + display_x;
                    bool *display_pixel = &chip8->display[display_pixel_index];

                    // Set carry flag if current display pixel will be unset
                    if (*display_pixel && sprite_pixel) {
                        chip8->V[0xF] = true;
                    }

                    *display_pixel ^= sprite_pixel;
                }
            }
            break;
        case 0xE000:
            switch (chip8->opcode & 0x00F0) {
                case 0x0090:  // EX9E; Skips the next instruction if the key
                              // stored in VX is pressed.
                    if (chip8->keypad[chip8->V[X]])
                        chip8->pc += 2;
                    break;
                case 0x00A0:  // EXA1; Skips the next instruction if the key
                              // stored in VX is not pressed.
                    if (!chip8->keypad[chip8->V[X]])
                        chip8->pc += 2;
                    break;
                default: break;
            }
            break;
        case 0xF000:
            switch (chip8->opcode & 0x00FF) {
                case 0x0007:  // FX07; Sets VX to the value of the delay timer.
                    chip8->V[X] = chip8->delay_timer;
                    break;
                case 0x000A:  // FX0A; A key press is awaited, and then stored
                              // in VX.

                    // Loop through keypad to see if a key has been pressed in
                    // this frame.
                    for (size_t i = 0; i < sizeof(chip8->keypad); i++) {
                        if (chip8->keypad[i]) {
                            chip8->V[X] = chip8->keypad[i];
                            key_pressed = true;
                        }
                    }

                    // Go back to this instruction if a key hasn't been pressed
                    if (!key_pressed)
                        chip8->pc -= 2;

                    break;
                case 0x0015:  // FX15; Sets the delay timer to VX.
                    chip8->delay_timer = chip8->V[X];
                    break;
                case 0x0018:  // FX18; Sets the sound timer to VX.
                    chip8->sound_timer = chip8->V[X];
                    break;
                case 0x001E:  // FX1E; Adds VX to I.
                    chip8->idx += chip8->V[X];
                    break;
                case 0x0029:  // FX29; Sets I to the location of the sprite for
                              // the character in VX.
                    chip8->idx = FONT_START + (chip8->V[X] * FONT_HEIGHT);
                    break;
                case 0x0033:  // FX33; Stores the binary-coded decimal
                              // representation of VX in I.
                    n = chip8->V[X];
                    chip8->memory[chip8->idx + 2] = n % 10;  // Hundreds digit
                    n /= 10;
                    chip8->memory[chip8->idx + 1] = n % 10;  // Tens digit
                    chip8->memory[chip8->idx] = n / 10;      // Ones digit
                    break;
                case 0x0055:  // FX55; Stores from V0 to VX (including VX) in
                              // memory, starting at address I.
                    for (size_t i = 0; i <= X; i++) {
                        chip8->memory[chip8->idx + i] = chip8->V[i];
                    }
                    break;
                case 0x0065:  // FX65; Fills from V0 to VX (including VX) with
                              // values from memory, starting at address I.
                    for (size_t i = 0; i <= X; i++) {
                        chip8->V[i] = chip8->memory[chip8->idx + i];
                    }
                    break;
                default: break;
            }
            break;
        default: break;
    }
}

void handle_input(chip8_t *chip8) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT: chip8->state = QUIT; return;
            case SDL_KEYDOWN:
                switch (event.key.keysym.sym) {
                    case SDLK_ESCAPE: chip8->state = QUIT; return;
                    case SDLK_1: chip8->keypad[0x1] = true; break;
                    case SDLK_2: chip8->keypad[0x2] = true; break;
                    case SDLK_3: chip8->keypad[0x3] = true; break;
                    case SDLK_4: chip8->keypad[0x4] = true; break;
                    case SDLK_q: chip8->keypad[0x4] = true; break;
                    case SDLK_w: chip8->keypad[0x5] = true; break;
                    case SDLK_e: chip8->keypad[0x6] = true; break;
                    case SDLK_r: chip8->keypad[0xD] = true; break;
                    case SDLK_a: chip8->keypad[0x7] = true; break;
                    case SDLK_s: chip8->keypad[0x8] = true; break;
                    case SDLK_d: chip8->keypad[0x9] = true; break;
                    case SDLK_f: chip8->keypad[0xE] = true; break;
                    case SDLK_z: chip8->keypad[0xA] = true; break;
                    case SDLK_x: chip8->keypad[0x0] = true; break;
                    case SDLK_c: chip8->keypad[0xB] = true; break;
                    case SDLK_v: chip8->keypad[0xF] = true; break;
                    default: break;
                }
                break;
            case SDL_KEYUP:
                switch (event.key.keysym.sym) {
                    case SDLK_1: chip8->keypad[0x1] = false; break;
                    case SDLK_2: chip8->keypad[0x2] = false; break;
                    case SDLK_3: chip8->keypad[0x3] = false; break;
                    case SDLK_4: chip8->keypad[0x4] = false; break;
                    case SDLK_q: chip8->keypad[0x4] = false; break;
                    case SDLK_w: chip8->keypad[0x5] = false; break;
                    case SDLK_e: chip8->keypad[0x6] = false; break;
                    case SDLK_r: chip8->keypad[0xD] = false; break;
                    case SDLK_a: chip8->keypad[0x7] = false; break;
                    case SDLK_s: chip8->keypad[0x8] = false; break;
                    case SDLK_d: chip8->keypad[0x9] = false; break;
                    case SDLK_f: chip8->keypad[0xE] = false; break;
                    case SDLK_z: chip8->keypad[0xA] = false; break;
                    case SDLK_x: chip8->keypad[0x0] = false; break;
                    case SDLK_c: chip8->keypad[0xB] = false; break;
                    case SDLK_v: chip8->keypad[0xF] = false; break;
                    default: break;
                }
                break;
            default: break;
        }
    }
}

void update_display(chip8_t *chip8) {
    SDL_Rect rect = {.x = 0, .y = 0, .w = WINDOW_SCALE, .h = WINDOW_SCALE};

    // Draw each rectangle of the CHIP-8 display to the screen
    for (size_t i = 0; i < sizeof(chip8->display); i++) {
        rect.x = (i % DISPLAY_WIDTH) * WINDOW_SCALE;
        rect.y = (i / DISPLAY_WIDTH) * WINDOW_SCALE;

        // Set draw color
        uint8_t r = 0, g = 0, b = 0;
        if (chip8->display[i]) {
            r = 255, g = 255, b = 255;
        }

        SDL_SetRenderDrawColor(chip8->sdl.renderer, r, g, b, SDL_ALPHA_OPAQUE);
        SDL_RenderFillRect(chip8->sdl.renderer, &rect);
    }

    SDL_RenderPresent(chip8->sdl.renderer);
}

void update_timers(chip8_t *chip8) {
    if (chip8->delay_timer > 0) {
        chip8->delay_timer--;
    }

    if (chip8->sound_timer > 0) {
        chip8->sound_timer--;
        if (!Mix_Playing(-1)) {
            Mix_PlayChannel(-1, chip8->sdl.sound, -1);
        }
    } else {
        Mix_HaltChannel(-1);
    }
}

void cleanup(sdl_t *sdl) {
    SDL_DestroyRenderer(sdl->renderer);
    SDL_DestroyWindow(sdl->window);
    sdl->window = NULL;
    sdl->renderer = NULL;
    Mix_FreeChunk(sdl->sound);
    Mix_CloseAudio();
    SDL_Quit();
}
