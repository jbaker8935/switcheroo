# Override these variables locally to match your environment.
# Example values assume llvm-mos toolchain is installed and f256lib archives
# are available under /opt/llvm-mos.

# MOS_PREFIX ?= /opt/llvm-mos/bin/llvm-mos
# F256LIB_PATH ?= /opt/llvm-mos/lib

ifdef F256LIB_PATH
LDFLAGS += -L$(F256LIB_PATH)
endif
