F256DEV_ROOT ?= /home/john/code/llvm-mos/f256dev
MOS_BIN      ?= $(F256DEV_ROOT)/llvm-mos/bin
MOS_CONFIG   ?= $(MOS_BIN)/mos-f256.cfg

MOS_CC      ?= $(MOS_BIN)/mos-f256-clang
MOS_AR      ?= $(MOS_BIN)/llvm-ar
MOS_OBJCOPY ?= $(MOS_BIN)/llvm-objcopy

PROJECT_NAME := f256_switcharoo
OUT_DIR      := build
OBJ_DIR      := $(OUT_DIR)/obj
BIN          := $(OUT_DIR)/$(PROJECT_NAME).prg
MAP          := $(OUT_DIR)/$(PROJECT_NAME).map

SRC_DIRS     ?= src
INCLUDE_DIRS ?= include $(F256DEV_ROOT)/include $(F256DEV_ROOT)/f256lib
LIB_DIRS     ?= $(F256DEV_ROOT)/llvm-mos/lib

SRC := $(shell find $(SRC_DIRS) -name '*.c')
OBJ := $(patsubst %.c,$(OBJ_DIR)/%.o,$(SRC))

CFLAGS := --target=mos --mcpu=65816 -Os -ffreestanding -fdata-sections -ffunction-sections \
          --config=$(MOS_CONFIG) \
          $(addprefix -I,$(INCLUDE_DIRS))

LDFLAGS := --target=mos --mcpu=65816 -fuse-ld=lld --config=$(MOS_CONFIG) -Wl,-gc-sections \
           -Wl,-Map=$(MAP) $(addprefix -L,$(LIB_DIRS))

LIBS := -lf256    # Provided by f256lib archive (configure path in toolchain.mk)

-include toolchain.mk

.PHONY: all clean assets dirs print-toolchain

all: dirs $(BIN)

print-toolchain:
	@echo "Toolchain root: $(F256DEV_ROOT)"
	@echo "Compiler:      $(MOS_CC)"
	@echo "Archiver:      $(MOS_AR)"
	@echo "Objcopy:       $(MOS_OBJCOPY)"
	@echo "Config:        $(MOS_CONFIG)"

dirs:
	@mkdir -p $(OUT_DIR) $(OBJ_DIR)

$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(MOS_CC) $(CFLAGS) -c $< -o $@

$(BIN): $(OBJ)
	$(MOS_CC) $(CFLAGS) $(OBJ) $(LDFLAGS) $(LIBS) -o $(BIN)
	$(MOS_OBJCOPY) -O binary $(BIN) $(OUT_DIR)/$(PROJECT_NAME).bin

clean:
	rm -rf $(OUT_DIR)
