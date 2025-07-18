#!/bin/bash
# Run Renode tests using Docker container

set -e

# Check if we're in the project root
if [ ! -f "CMakeLists.txt" ] || [ ! -d "bootloader" ]; then
    echo "Error: This script must be run from the Picocalc_SD_Boot project root"
    exit 1
fi

# Build the Docker image if it doesn't exist
echo "Building Docker image..."
docker build -t picocalc-test-env docker/

# Clean build directory to avoid CMake cache conflicts
echo "Cleaning build directory..."
rm -rf build
mkdir -p build

# First, build the bootloader in Docker
echo "Building bootloader in Docker..."
docker run --rm \
    -v "$(pwd):/workspace" \
    -w /workspace \
    picocalc-test-env \
    -c "cd build && cmake .. && make -j4"

# Check if bootloader was built successfully
if [ ! -f "build/picocalc_sd_boot.elf" ]; then
    echo "Error: Bootloader build failed"
    exit 1
fi

# Generate UF2 file from ELF using picotool
echo "Generating UF2 file..."
docker run --rm \
    -v "$(pwd):/workspace" \
    -w /workspace \
    picocalc-test-env \
    -c "cd build && ./_deps/picotool-build/picotool uf2 convert picocalc_sd_boot.elf picocalc_sd_boot.uf2"

# Generate test files
echo "Generating test files..."
docker run --rm \
    -v "$(pwd):/workspace" \
    -w /workspace \
    picocalc-test-env \
    -c "cd tests && python3 generate_test_files.py"

# Run the simple Renode emulation test  
echo "Running Renode emulation tests..."
docker run --rm \
    -v "$(pwd):/workspace" \
    -w /workspace \
    --env TERM=dumb \
    picocalc-test-env \
    -c "cd /workspace/tests && python3 simple_renode_test.py"

# Check exit code
if [ $? -eq 0 ]; then
    echo -e "\n✓ All Renode emulation tests passed!"
else
    echo -e "\n✗ Some Renode emulation tests failed"
    exit 1
fi 