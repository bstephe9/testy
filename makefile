# Directories
CHIP8_DIR = chip-8
SRC_DIR = $(CHIP8_DIR)/src
BUILD_DIR = $(CHIP8_DIR)/build
TESTS_DIR = tests
UNITY_DIR = $(TESTS_DIR)/unity
PUBLIC_DIR = public

# Variables and flags
CC = gcc
CFLAGS = -Wall -Wextra -g
LDFLAGS = `sdl2-config --cflags --libs` -lSDL2_Mixer
EMFLAGS = -sUSE_SDL=2 -sUSE_SDL_MIXER=2  --embed-file roms --embed-file $(CHIP8_DIR)/data

# Files
TARGET = main
SRC_FILES := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRC_FILES))
CLEAN_FILES = *.exe *.o *.html *.wasm
TEST_FILE = $(TESTS_DIR)/test_chip8.c
TEST_TARGET = test_chip8
EMCC_OUTPUT = chip8

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(BUILD_DIR)  # Ensure the build directory exists
	$(CC) -c $< -o $@ $(CFLAGS) $(LDFLAGS)

tests: $(TEST_TARGET)

$(TEST_TARGET): $(TEST_FILE)
	$(CC) $(CFLAGS) -o $@ $(SRC_DIR)/chip8.c $(UNITY_DIR)/unity.c $^ $(LDFLAGS) -DUNIT_TEST
	./$(TEST_TARGET)
	rm $(TEST_TARGET)

# Generate emcc output and move to public/
web: $(SRC_FILES)
	emcc $(SRC_FILES) -o $(EMCC_OUTPUT).html $(CFLAGS) $(EMFLAGS)
	mv $(EMCC_OUTPUT)* public/

clean:
	rm -rf $(BUILD_DIR) $(TARGET)

.PHONY: all clean debug
