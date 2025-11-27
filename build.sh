#!/bin/bash
# Build wrapper for F256 Switcharoo
# Run the actual build
echo "Running full build..."
../llvm-mos/f256dev/f256build.sh ../switcheroo "$@"
