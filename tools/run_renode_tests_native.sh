#!/bin/bash
# Run Renode tests natively on macOS (without Docker)

set -e

# Check if we're in the project root
if [ ! -f "CMakeLists.txt" ] || [ ! -d "bootloader" ]; then
    echo "Error: This script must be run from the Picocalc_SD_Boot project root"
    exit 1
fi

# Check if Renode is installed
RENODE_FOUND=0
if command -v renode &> /dev/null; then
    echo "Found Renode in PATH"
    RENODE_FOUND=1
elif [ -f "/Applications/Renode.app/Contents/MacOS/bin/Renode.exe" ]; then
    echo "Found Renode.app in Applications"
    RENODE_FOUND=1
else
    echo "Error: Renode is not installed"
    echo ""
    echo "To install Renode on macOS:"
    echo "1. Download the latest macOS package from https://github.com/renode/renode/releases"
    echo "2. Install the .dmg file"
    echo ""
    echo "Or install via Homebrew:"
    echo "   brew install renode"
    exit 1
fi

# Build the bootloader
echo "Building bootloader..."
mkdir -p build
cd build
cmake ..
make -j$(sysctl -n hw.ncpu)
cd ..

# Check if bootloader was built successfully
if [ ! -f "build/picocalc_sd_boot.elf" ]; then
    echo "Error: Bootloader build failed"
    exit 1
fi

# Check if picotool is available for UF2 generation
if [ -f "build/_deps/picotool-build/picotool" ]; then
    echo "Generating UF2 file..."
    cd build
    ./_deps/picotool-build/picotool uf2 convert picocalc_sd_boot.elf picocalc_sd_boot.uf2
    cd ..
else
    echo "Warning: picotool not found, skipping UF2 generation"
fi

# Generate test files
echo "Generating test files..."
python3 tests/generate_test_files.py

# Run the simple Renode emulation test
echo "Running Renode emulation tests..."
cd tests
python3 simple_renode_test.py
cd ..

# Check exit code
if [ $? -eq 0 ]; then
    echo ""
    echo "==================================="
    echo "✅ All Renode tests passed!"
    echo "==================================="
else
    echo ""
    echo "==================================="
    echo "❌ Renode tests failed!"
    echo "==================================="
    exit 1
fi 