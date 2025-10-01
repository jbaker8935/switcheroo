#!/bin/bash
# Build wrapper for F256 Switcharoo
# Copies necessary headers to .builddir after processing but before compilation

# First, let f256build.sh process the source files
echo "Processing source files..."
../llvm-mos/f256dev/f256build.sh ../f256_switch preprocess 2>/dev/null || true

# Now copy headers to .builddir
echo "Copying headers to .builddir..."
mkdir -p .builddir
cp include/game/*.h .builddir/ 2>/dev/null || true
cp include/platform/*.h .builddir/ 2>/dev/null || true

# Run the actual build
echo "Running full build..."
../llvm-mos/f256dev/f256build.sh ../f256_switch "$@"
