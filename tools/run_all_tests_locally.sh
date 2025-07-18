#!/bin/bash
# Run all CI/CD tests locally
# This script runs both act workflows and Docker-based tests

set -e

# Colors for output
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}=== Picocalc SD Boot - Running All Tests Locally ===${NC}"
echo ""

# Check prerequisites
check_prerequisites() {
    echo -e "${YELLOW}Checking prerequisites...${NC}"
    
    # Check Docker
    if ! command -v docker &> /dev/null; then
        echo -e "${RED}✗ Docker not found. Please install Docker Desktop${NC}"
        exit 1
    fi
    echo -e "${GREEN}✓ Docker found${NC}"
    
    # Check act
    if ! command -v act &> /dev/null; then
        echo -e "${RED}✗ act not found. Install with: brew install act${NC}"
        exit 1
    fi
    echo -e "${GREEN}✓ act found ($(act --version))${NC}"
    
    # Check GitHub token
    if [ -z "$GITHUB_TOKEN" ]; then
        echo -e "${YELLOW}⚠ GITHUB_TOKEN not set. Trying GitHub CLI...${NC}"
        if command -v gh &> /dev/null && gh auth status &> /dev/null; then
            export GITHUB_TOKEN=$(gh auth token)
            echo -e "${GREEN}✓ GitHub token set from CLI${NC}"
        else
            echo -e "${RED}✗ No GitHub authentication. External actions won't work${NC}"
            echo "  Set up with: export GITHUB_TOKEN=ghp_your_token_here"
            echo "  Or: gh auth login"
        fi
    else
        echo -e "${GREEN}✓ GitHub token found${NC}"
    fi
    
    echo ""
}

# Run act workflow validation
run_act_validation() {
    echo -e "${BLUE}=== 1. Validating Workflows with Act ===${NC}"
    echo -e "${YELLOW}Running dry run to validate all workflows...${NC}"
    
    act -n
    
    echo -e "${GREEN}✓ Workflow validation complete${NC}"
    echo ""
}

# Run act build workflow
run_act_build() {
    echo -e "${BLUE}=== 2. Running Build Workflow with Act ===${NC}"
    
    if [ -n "$GITHUB_TOKEN" ]; then
        echo -e "${YELLOW}Running build job with act...${NC}"
        act -W .github/workflows/build.yml -j build --secret GITHUB_TOKEN=$GITHUB_TOKEN || {
            echo -e "${YELLOW}Note: Act build may fail due to missing tools in container${NC}"
            echo -e "${YELLOW}This is expected. Use Docker tests for full build validation.${NC}"
        }
    else
        echo -e "${YELLOW}Skipping act build (no GitHub token)${NC}"
    fi
    
    echo ""
}

# Run Docker-based tests
run_docker_tests() {
    echo -e "${BLUE}=== 3. Running Docker-based Build Tests ===${NC}"
    echo -e "${YELLOW}This includes full build environment with proper tools...${NC}"
    
    if [ -f "tools/run_ci_locally_docker.sh" ]; then
        ./tools/run_ci_locally_docker.sh
    else
        echo -e "${RED}✗ Docker CI script not found${NC}"
    fi
    
    echo ""
}

# Run unit tests locally
run_local_unit_tests() {
    echo -e "${BLUE}=== 4. Running Unit Tests Locally ===${NC}"
    
    # Check if we have a build directory
    if [ -d "build" ] || [ -d "build_pico2_w" ]; then
        BUILD_DIR="${BUILD_DIR:-build_pico2_w}"
        [ ! -d "$BUILD_DIR" ] && BUILD_DIR="build"
        
        echo -e "${YELLOW}Running tests in $BUILD_DIR...${NC}"
        
        # Run ctest if available
        if [ -f "$BUILD_DIR/CTestTestfile.cmake" ]; then
            cd "$BUILD_DIR"
            ctest --output-on-failure || echo -e "${YELLOW}Some tests failed${NC}"
            cd ..
        else
            echo -e "${YELLOW}No CTest configuration found${NC}"
        fi
    else
        echo -e "${YELLOW}No build directory found. Run cmake first.${NC}"
    fi
    
    echo ""
}

# Run Python tool tests
run_python_tests() {
    echo -e "${BLUE}=== 5. Testing Python Tools ===${NC}"
    
    # Test UF2 validator
    echo -e "${YELLOW}Testing UF2 validator...${NC}"
    python3 tools/check_uf2_crc32.py --help > /dev/null && \
        echo -e "${GREEN}✓ UF2 validator works${NC}" || \
        echo -e "${RED}✗ UF2 validator failed${NC}"
    
    # If pytest is available, run Python tests
    if command -v pytest &> /dev/null; then
        echo -e "${YELLOW}Running pytest...${NC}"
        pytest tests/ -v || echo -e "${YELLOW}Some Python tests failed${NC}"
    fi
    
    echo ""
}

# Summary
show_summary() {
    echo -e "${BLUE}=== Test Summary ===${NC}"
    echo ""
    echo "Tests completed! Review the output above for any failures."
    echo ""
    echo -e "${YELLOW}Next steps:${NC}"
    echo "1. Fix any failing tests"
    echo "2. Run individual test suites as needed:"
    echo "   - Workflow validation: act -n"
    echo "   - Docker build test: ./tools/run_ci_locally_docker.sh"
    echo "   - Unit tests: cd build && ctest"
    echo "3. Push to GitHub for full CI/CD validation"
    echo ""
}

# Main execution
main() {
    check_prerequisites
    
    # Allow selecting specific tests
    if [ "$1" == "--help" ] || [ "$1" == "-h" ]; then
        echo "Usage: $0 [options]"
        echo ""
        echo "Options:"
        echo "  --act-only     Run only act workflows"
        echo "  --docker-only  Run only Docker tests"
        echo "  --quick        Skip slow tests"
        echo "  --help         Show this help"
        exit 0
    fi
    
    if [ "$1" == "--act-only" ]; then
        run_act_validation
        run_act_build
    elif [ "$1" == "--docker-only" ]; then
        run_docker_tests
    elif [ "$1" == "--quick" ]; then
        run_act_validation
        run_python_tests
    else
        # Run everything
        run_act_validation
        run_act_build
        run_docker_tests
        run_local_unit_tests
        run_python_tests
    fi
    
    show_summary
}

# Run main
main "$@" 