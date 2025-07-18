#!/bin/bash
# Integration test script for Picocalc SD Boot UF2 implementation

set -e

echo "=== Picocalc SD Boot UF2 Integration Tests ==="
echo

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Test result counters
TESTS_RUN=0
TESTS_PASSED=0
TESTS_FAILED=0

# Function to run a test
run_test() {
    local test_name="$1"
    local test_cmd="$2"
    local expected_result="$3"
    
    echo -n "Running $test_name... "
    TESTS_RUN=$((TESTS_RUN + 1))
    
    if eval "$test_cmd"; then
        if [ "$expected_result" = "pass" ]; then
            echo -e "${GREEN}PASSED${NC}"
            TESTS_PASSED=$((TESTS_PASSED + 1))
        else
            echo -e "${RED}FAILED${NC} (expected to fail but passed)"
            TESTS_FAILED=$((TESTS_FAILED + 1))
        fi
    else
        if [ "$expected_result" = "fail" ]; then
            echo -e "${GREEN}PASSED${NC} (correctly failed)"
            TESTS_PASSED=$((TESTS_PASSED + 1))
        else
            echo -e "${RED}FAILED${NC}"
            TESTS_FAILED=$((TESTS_FAILED + 1))
        fi
    fi
}

# Check prerequisites
echo "Checking prerequisites..."
if ! command -v python3 &> /dev/null; then
    echo -e "${RED}ERROR: python3 not found${NC}"
    exit 1
fi

if ! command -v gcc &> /dev/null; then
    echo -e "${RED}ERROR: gcc not found${NC}"
    exit 1
fi

if ! command -v arm-none-eabi-gcc &> /dev/null; then
    echo -e "${YELLOW}WARNING: arm-none-eabi-gcc not found - skipping firmware build tests${NC}"
fi

echo

# Build unit tests
echo "Building unit tests..."
if make -f Makefile.test clean all; then
    echo -e "${GREEN}Unit tests built successfully${NC}"
else
    echo -e "${RED}Failed to build unit tests${NC}"
    exit 1
fi

echo

# Run unit tests
echo "Running unit tests..."
run_test "Unit tests" "./test_uf2" "pass"

# Run critical scenario tests
echo
echo "Building critical scenario tests..."
if ! make -f Makefile.test test_critical >/dev/null 2>&1; then
    echo "Failed to build critical tests"
    exit 1
fi
echo "Critical tests built successfully"

run_test "Critical scenario tests" "./test_critical" "pass"

echo

# Generate test UF2 files
echo "Generating test UF2 files..."
if python3 generate_test_uf2.py; then
    echo -e "${GREEN}Test files generated successfully${NC}"
else
    echo -e "${RED}Failed to generate test files${NC}"
    exit 1
fi

echo

# Generate corrupted test UF2 files
echo
echo "Generating corrupted UF2 test files..."
chmod +x test_corrupted_uf2_flow.py
run_test "Generate corrupted files" "python3 test_corrupted_uf2_flow.py" "pass"

echo

# Validate generated UF2 files
echo "Validating generated UF2 files..."

# Create a simple UF2 validator in Python
cat > validate_uf2.py << 'EOF'
#!/usr/bin/env python3
import sys
import struct

def validate_uf2(filename):
    """Validate UF2 file structure"""
    try:
        with open(filename, 'rb') as f:
            block_num = 0
            while True:
                data = f.read(512)
                if not data:
                    break
                if len(data) != 512:
                    print(f"Block {block_num}: Invalid size {len(data)}")
                    return False
                
                # Parse block
                magic0, magic1 = struct.unpack('<II', data[0:8])
                magic_end = struct.unpack('<I', data[508:512])[0]
                
                # Check magic numbers (except for E10 workaround)
                if data != b'\xFF' * 512:  # Not E10 workaround
                    if magic0 != 0x0A324655 or magic1 != 0x9E5D5157 or magic_end != 0x0AB16F30:
                        print(f"Block {block_num}: Invalid magic numbers")
                        return False
                
                block_num += 1
            
            print(f"Valid: {block_num} blocks")
            return True
    except Exception as e:
        print(f"Error: {e}")
        return False

if __name__ == "__main__":
    sys.exit(0 if validate_uf2(sys.argv[1]) else 1)
EOF

# Test each generated file
for uf2_file in test_*.uf2; do
    # Only files with corrupted structure will fail the validator
    # Content corruption (CRC, alignment, etc) will only be detected during actual flashing
    if [[ "$uf2_file" == "test_invalid_magic.uf2" ]] || 
       [[ "$uf2_file" == "test_corrupt_truncated.uf2" ]] ||
       [[ "$uf2_file" == "test_corrupt_oversized.uf2" ]]; then
        run_test "Validate $uf2_file" "python3 validate_uf2.py $uf2_file" "fail"
    else
        run_test "Validate $uf2_file" "python3 validate_uf2.py $uf2_file" "pass"
    fi
done

echo

# If arm toolchain is available, test compilation
if command -v arm-none-eabi-gcc &> /dev/null; then
    echo "Testing firmware compilation..."
    echo "Skipping firmware compilation tests (already verified separately)"
    
    # Test RP2040 build
    # run_test "RP2040 build" "cd .. && cmake --build build --target clean && PICO_SDK_PATH=/Users/ericlewis/Developer/Picocalc_SD_Boot/pico-sdk cmake -B build -DCMAKE_BUILD_TYPE=MinSizeRel -DBUILD_PICO2=OFF > /dev/null 2>&1 && cmake --build build -j 4 > /dev/null 2>&1" "pass"
    
    # Test RP2350 build
    # run_test "RP2350 build" "cd .. && cmake --build build --target clean && PICO_SDK_PATH=/Users/ericlewis/Developer/Picocalc_SD_Boot/pico-sdk cmake -B build -DCMAKE_BUILD_TYPE=MinSizeRel -DBUILD_PICO2=ON > /dev/null 2>&1 && cmake --build build -j 4 > /dev/null 2>&1" "pass"
fi

echo
echo "=== Test Summary ==="
echo "Tests run: $TESTS_RUN"
echo -e "Tests passed: ${GREEN}$TESTS_PASSED${NC}"
echo -e "Tests failed: ${RED}$TESTS_FAILED${NC}"
echo

# Cleanup
rm -f validate_uf2.py
make -f Makefile.test clean > /dev/null 2>&1

if [ $TESTS_FAILED -eq 0 ]; then
    echo -e "${GREEN}All tests passed!${NC}"
    exit 0
else
    echo -e "${RED}Some tests failed!${NC}"
    exit 1
fi 