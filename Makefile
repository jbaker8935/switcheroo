F256DEV_ROOT ?= /home/john/code/llvm-mos/f256dev
MOS_BIN      ?= $(F256DEV_ROOT)/llvm-mos/bin
MOS_CONFIG   ?= $(MOS_BIN)/mos-f256.cfg

MOS_CC      ?= $(MOS_BIN)/mos-f256-clang
MOS_AR      ?= $(MOS_BIN)/llvm-ar
MOS_OBJCOPY ?= $(MOS_BIN)/llvm-objcopy
MOS_NM      ?= $(MOS_BIN)/llvm-nm
MOS_OBJDUMP ?= $(MOS_BIN)/llvm-objdump

PYTHON ?= python3

PROJECT_NAME := f256_switcharoo
OUT_DIR      := build
OBJ_DIR      := $(OUT_DIR)/obj
LINKER_DIR   := toolchain/linker
LINKER_SCRIPT := $(LINKER_DIR)/link.ld
LINK_OUT_BASE := $(OUT_DIR)/$(PROJECT_NAME)

PGZ          := $(OUT_DIR)/$(PROJECT_NAME).pgz
ELF          := $(OUT_DIR)/$(PROJECT_NAME).elf
BIN          := $(OUT_DIR)/$(PROJECT_NAME).bin
MAP          := $(OUT_DIR)/$(PROJECT_NAME).map
SYM          := $(OUT_DIR)/$(PROJECT_NAME).sym
LST          := $(OUT_DIR)/$(PROJECT_NAME).lst

PGZ_INFO_SCRIPT := scripts/pgz_thunk.py
ASSET_GEN_SCRIPT := scripts/generate_assets.py

ASSET_DIR := assets/generated
ASSET_MANIFEST := $(ASSET_DIR)/MANIFEST.txt
GENERATED_ASSETS := \
	$(ASSET_DIR)/board_bitmap.bin \
	$(ASSET_DIR)/sprite_white_normal.bin \
	$(ASSET_DIR)/sprite_white_swapped.bin \
	$(ASSET_DIR)/sprite_black_normal.bin \
	$(ASSET_DIR)/sprite_black_swapped.bin \
	$(ASSET_DIR)/sprite_move_indicator.bin \
	$(ASSET_DIR)/sprite_highlight.bin \
	$(ASSET_DIR)/icon_reset.bin \
	$(ASSET_DIR)/icon_info.bin \
	$(ASSET_DIR)/icon_difficulty.bin \
	$(ASSET_DIR)/icon_starting_board.bin \
	$(ASSET_DIR)/icon_history.bin \
	$(ASSET_DIR)/icon_exit.bin

GENERATED_SRC := src/assets/generated_assets.c
GENERATED_HEADER := include/assets/generated_assets.h

SRC_DIRS     ?= src
INCLUDE_DIRS ?= include $(F256DEV_ROOT)/include $(F256DEV_ROOT)/f256lib
LIB_DIRS     ?= $(F256DEV_ROOT)/llvm-mos/lib \
				$(F256DEV_ROOT)/llvm-mos/mos-platform/common/lib

LOCAL_SRC := src/main.c \
             src/board.c \
             src/game_state.c \
             src/input.c \
             src/input_handler.c \
             src/system.c \
             src/video.c \
             src/render.c
EXTERNAL_LIB_SRC := 
EXTERNAL_SRC := 

LOCAL_OBJ := $(patsubst %.c,$(OBJ_DIR)/%.o,$(LOCAL_SRC))
EXTERNAL_OBJ := $(addprefix $(OBJ_DIR)/f256lib/,$(EXTERNAL_LIB_SRC:.c=.o))

OBJ := $(LOCAL_OBJ) $(EXTERNAL_OBJ)

CFLAGS := -O0 -ffreestanding -fdata-sections -ffunction-sections -Wall \
		  $(addprefix -I,$(INCLUDE_DIRS))

LDFLAGS := -Wl,-gc-sections -Wl,-Map=$(MAP) $(addprefix -L,$(LIB_DIRS))

LIBS := -lm    # Additional libraries can be appended via toolchain.mk

-include toolchain.mk

.PHONY: all clean assets dirs print-toolchain

all: dirs $(PGZ) $(SYM) $(LST) $(BIN)

print-toolchain:
	@echo "Toolchain root: $(F256DEV_ROOT)"
	@echo "Compiler:      $(MOS_CC)"
	@echo "Archiver:      $(MOS_AR)"
	@echo "Objcopy:       $(MOS_OBJCOPY)"
	@echo "Config:        $(MOS_CONFIG)"
	@echo "Linker script: $(abspath $(LINKER_SCRIPT))"

dirs:
	@mkdir -p $(OUT_DIR) $(OBJ_DIR)

$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(MOS_CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/f256lib/%.o: $(F256DEV_ROOT)/f256lib/%.c
	@mkdir -p $(dir $@)
	$(MOS_CC) $(CFLAGS) -c $< -o $@

assets: $(ASSET_MANIFEST)

$(ASSET_MANIFEST): $(ASSET_GEN_SCRIPT)
	$(PYTHON) $(ASSET_GEN_SCRIPT) --output $(ASSET_DIR)

$(GENERATED_ASSETS): $(ASSET_MANIFEST)

$(OBJ_DIR)/src/assets/generated_assets.o: $(GENERATED_SRC) $(GENERATED_HEADER) $(ASSET_MANIFEST)

$(PGZ) $(ELF): $(OBJ) link.ld $(LINKER_SCRIPT) $(PGZ_INFO_SCRIPT)
	@rm -f $(PGZ) $(ELF)
	$(MOS_CC) $(CFLAGS) $(OBJ) $(LDFLAGS) $(LIBS) -o $(LINK_OUT_BASE)
	@if [ ! -f "$(ELF)" ]; then \
		echo "Linker did not emit $(ELF); please verify the llvm-mos toolchain."; \
		exit 1; \
	fi
	mv "$(LINK_OUT_BASE)" "$(PGZ)"
	$(PYTHON) $(PGZ_INFO_SCRIPT) $(PGZ) || true

$(SYM): $(ELF)
	$(MOS_NM) $(ELF) > $(SYM)

$(LST): $(ELF)
	$(MOS_OBJDUMP) --syms -d --print-imm-hex $(ELF) > $(LST)

$(BIN): $(ELF)
	$(MOS_OBJCOPY) -O binary $(ELF) $(BIN)

clean:
	rm -rf $(OUT_DIR)
	rm -rf $(ASSET_DIR)
	rm -f src/assets/generated_assets.c
	rm -f include/assets/generated_assets.h
