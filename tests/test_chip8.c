#include <stdio.h>

#include "../chip-8/src/chip8.h"
#include "unity/unity.h"

chip8_t chip8;
char *rom_path = "./tests/test_roms/IBM_Logo.ch8";
static FILE *original_stderr = NULL;

void setUp(void) {
    initialize(&chip8);

#ifdef _WIN32
    original_stderr = freopen("nul", "w", stderr);
#else
    original_stderr = freopen("/dev/null", "w", stderr);
#endif
}

void tearDown(void) {
    if (original_stderr) {
        freopen("/dev/tty", "w", stderr);  // Redirect stderr back to terminal
    }
}

void test_should_zero_initialize(void) {
    initialize(&chip8);

    TEST_ASSERT_EQUAL_HEX(0x200, chip8.pc);
    TEST_ASSERT_EQUAL_HEX(0x0, chip8.opcode);
    TEST_ASSERT_EQUAL_HEX(0x0, chip8.idx);
    TEST_ASSERT_EQUAL_HEX(0x0, chip8.sp);

    TEST_ASSERT_EACH_EQUAL_UINT8(0, chip8.V, 16);
    TEST_ASSERT_EACH_EQUAL_UINT16(0, chip8.stack, 16);
    TEST_ASSERT_EACH_EQUAL_UINT8(0, chip8.display, 64 * 32);

    // Test memory except the fontset memory range
    TEST_ASSERT_EACH_EQUAL_UINT8(0, &chip8.memory[0], FONT_START);
    TEST_ASSERT_EACH_EQUAL_UINT8(0, &chip8.memory[FONT_END],
                                 MEMORY_SIZE - FONT_END);

    TEST_ASSERT_NULL(chip8.sdl.window);
    TEST_ASSERT_NULL(chip8.sdl.renderer);

    TEST_ASSERT_EQUAL(chip8.state, RUNNING);
}

void test_should_get_correct_rom_file_size(void) {
    FILE *test_rom = fopen(rom_path, "rb");
    long rom_size = get_rom_size(test_rom);
    TEST_ASSERT_EQUAL(132, rom_size);
}

void test_should_read_rom_into_memory(void) {
    unsigned char buf[MAX_ROM_SIZE];
    read_rom(buf, rom_path);
    read_rom(&chip8.memory[PC_START], rom_path);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(&buf, &chip8.memory[PC_START], 132);
}

void test_should_pass_on_valid_rom_path(void) {
    unsigned char buf[132];  // Define a buffer so `read_rom` can be called
    TEST_ASSERT_TRUE(read_rom(buf, rom_path));
}

void test_should_fail_on_invalid_rom_path(void) {
    unsigned char buf[132];
    TEST_ASSERT_FALSE(read_rom(buf, "invalid/rom/path"));
}

void test_should_setup_sdl(void) {
    setup_sdl(&chip8.sdl);
    TEST_ASSERT_NOT_NULL(chip8.sdl.window);
    TEST_ASSERT_NOT_NULL(chip8.sdl.renderer);
}

void test_should_cleanup_sdl(void) {
    setup_sdl(&chip8.sdl);
    cleanup(&chip8.sdl);
    TEST_ASSERT_NULL(chip8.sdl.window);
    TEST_ASSERT_NULL(chip8.sdl.renderer);
}

// Change 'main' to 'SDL_main' to avoid conflict with SDL2's entry point
int SDL_main(int argc, char *argv[]) {
    // To avoid unused parameter warnings
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_should_zero_initialize);
    RUN_TEST(test_should_get_correct_rom_file_size);
    RUN_TEST(test_should_read_rom_into_memory);
    RUN_TEST(test_should_pass_on_valid_rom_path);
    RUN_TEST(test_should_fail_on_invalid_rom_path);
    RUN_TEST(test_should_setup_sdl);
    RUN_TEST(test_should_cleanup_sdl);
    return UNITY_END();
}