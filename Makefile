# oscar64 build for F256 Switcharoo
ROOT     := $(CURDIR)
BUILD    := $(ROOT)/build_oscar64
BIN      := $(ROOT)/bin

OSCAR64  ?= $(ROOT)/../oscar64/bin/oscar64
F256LIB  := $(ROOT)/../f256lib-oscar64/f256lib

OSCAR_FLAGS := -tm=f256k -n -Os -dNOFLOAT -dWITHOUT_KEYBOARD -i=$(F256LIB) -i=$(ROOT)/src

TARGET_NAME := switcheroo
TARGET_PGZ  := $(BUILD)/$(TARGET_NAME).pgz
PUZZLE_BIN  := $(ROOT)/assets/generated/puzzle_data.bin

SRCS := \
	$(ROOT)/src/main.c \
	$(ROOT)/src/system.c \
	$(ROOT)/src/board.c \
	$(ROOT)/src/game_state.c \
	$(ROOT)/src/input.c \
	$(ROOT)/src/input_handler.c \
	$(ROOT)/src/render.c \
	$(ROOT)/src/video.c \
	$(ROOT)/src/text_display.c \
	$(ROOT)/src/timer.c \
	$(ROOT)/src/puzzle_data.c \
	$(ROOT)/src/ai_agent.c \
	$(ROOT)/src/achievements.c \
	$(ROOT)/src/achievements_screen.c \
	$(ROOT)/src/file_io.c \
	$(ROOT)/src/sram_assets.c \
	$(ROOT)/src/dma_copy.c \
	$(ROOT)/src/sound.c \
	$(ROOT)/src/playsid.c \
	$(ROOT)/src/mouse_pointer.c \
	$(ROOT)/src/ui_progress.c \
	$(ROOT)/src/freeplay_history.c \
	$(ROOT)/src/exit_screen.c

.PHONY: all clean dirs puzzles

all: puzzles dirs $(TARGET_PGZ)
	@cp $(TARGET_PGZ) $(BIN)/$(TARGET_NAME).pgz
	@echo "Built: $(BIN)/$(TARGET_NAME).pgz"

dirs:
	@mkdir -p $(BUILD) $(BIN)

puzzles:
	@if [ ! -f "$(PUZZLE_BIN)" ]; then \
		python3 $(ROOT)/scripts/convert_puzzles.py; \
	fi

$(TARGET_PGZ): $(SRCS) $(wildcard $(ROOT)/src/*.h)
	$(OSCAR64) $(OSCAR_FLAGS) $(SRCS) -o=$(TARGET_PGZ)

clean:
	rm -rf $(BUILD)
	rm -f $(BIN)/$(TARGET_NAME).pgz
