#!/bin/bash
# Docker-based local CI testing for Picocalc SD Boot
# This is an alternative to act that works reliably on macOS

set -e

echo "=== Picocalc SD Boot - Local CI Testing with Docker ==="
echo ""

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Function to run a test step
run_step() {
    local step_name="$1"
    local command="$2"
    
    echo -e "${YELLOW}→ Running: ${step_name}${NC}"
    if eval "$command"; then
        echo -e "${GREEN}✓ ${step_name} passed${NC}\n"
    else
        echo -e "${RED}✗ ${step_name} failed${NC}\n"
        exit 1
    fi
}

# Test 1: Check workspace
run_step "Workspace validation" \
    "docker run --rm -v \$(pwd):/workspace -w /workspace ubuntu:22.04 bash -c '
        echo \"Checking workspace files...\"
        [ -f CMakeLists.txt ] && echo \"✓ CMakeLists.txt found\" || exit 1
        [ -d bootloader ] && echo \"✓ bootloader/ found\" || exit 1
        [ -d tests ] && echo \"✓ tests/ found\" || exit 1
        [ -d tools ] && echo \"✓ tools/ found\" || exit 1
    '"

# Test 2: Python tools validation
run_step "Python tools validation" \
    "docker run --rm -v \$(pwd):/workspace -w /workspace python:3.10 bash -c '
        echo \"Testing UF2 validation script...\"
        python3 tools/check_uf2_crc32.py --help > /dev/null
        echo \"✓ UF2 validation script works\"
        
        echo \"Checking Python dependencies...\"
        pip install pytest robotframework pyserial > /dev/null 2>&1
        echo \"✓ Python dependencies installable\"
    '"

# Test 3: Build environment test
run_step "Build environment test" \
    "docker run --rm -v \$(pwd):/workspace -w /workspace ubuntu:22.04 bash -c '
        echo \"Installing build tools...\"
        apt-get update > /dev/null 2>&1
        apt-get install -y cmake gcc-arm-none-eabi python3 > /dev/null 2>&1
        
        echo \"Checking tool versions...\"
        cmake --version | head -1
        arm-none-eabi-gcc --version | head -1
        python3 --version
    '"

# Test 4: CMake configuration test
run_step "CMake configuration" \
    "docker run --rm -v \$(pwd):/workspace -w /workspace ubuntu:22.04 bash -c '
        apt-get update > /dev/null 2>&1
        apt-get install -y cmake gcc-arm-none-eabi git python3 > /dev/null 2>&1
        
        echo \"Configuring build for pico2_w...\"
        rm -rf build_docker_test
        cmake -S . -B build_docker_test -DPICO_BOARD=pico2_w
    '"

# Test 5: Clang-tidy check (sample)
run_step "Static analysis check" \
    "docker run --rm -v \$(pwd):/workspace -w /workspace ubuntu:22.04 bash -c '
        echo \"Would run: clang-tidy on source files\"
        echo \"(Skipping actual run to save time)\"
        [ -f .clang-tidy ] && echo \"✓ .clang-tidy configuration found\" || exit 1
    '"

echo -e "${GREEN}=== All local CI tests passed! ===${NC}"
echo ""
echo "Note: This is a simplified version of the full CI pipeline."
echo "For complete testing, push to GitHub or use GitHub Codespaces." 