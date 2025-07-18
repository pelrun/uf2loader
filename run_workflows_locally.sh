#!/bin/bash
# Helper script to run GitHub Actions workflows locally using act

set -e

echo "=== Running GitHub Actions Workflows Locally with Act ==="
echo ""

# Function to display usage
show_usage() {
    echo "Usage: $0 [command]"
    echo ""
    echo "Commands:"
    echo "  simple    - Run a simple test workflow"
    echo "  build     - Run the main build workflow"
    echo "  unit      - Run unit tests"
    echo "  full      - Run full CI pipeline"
    echo "  list      - List available workflows"
    echo "  dry       - Dry run (show what would execute)"
    echo "  clean     - Clean up Docker resources"
    echo "  gcc       - Test with newer ARM GCC"
    echo "  auth      - Test with GitHub authentication"
    echo "  help      - Show this help message"
    echo ""
    echo "Environment variables:"
    echo "  GITHUB_TOKEN - Set for external action authentication"
    echo ""
    echo "Examples:"
    echo "  $0 simple"
    echo "  $0 build"
    echo "  GITHUB_TOKEN=ghp_xxx $0 auth"
}

# Check if act is installed
if ! command -v act &> /dev/null; then
    echo "Error: 'act' is not installed. Install it with:"
    echo "  brew install act"
    exit 1
fi

# Default command
COMMAND="${1:-list}"
shift || true

# Handle commands
case "$COMMAND" in
    list)
        echo "Available workflows and jobs:"
        act -l
        ;;
        
    build)
        echo "Running build job for all boards..."
        act -j build
        ;;
        
    sim)
        echo "Running Renode simulation tests..."
        echo "Note: This requires Renode Docker image support"
        act -j sim
        ;;
        
    unit)
        echo "Running unit tests..."
        act -j unit-tests
        ;;
        
    all)
        echo "Running all CI jobs..."
        act
        ;;
        
    matrix)
        echo "Running matrix build for all boards (pico, pico_w, pico2_w)..."
        act -j build
        ;;
        
    pico)
        echo "Running build for pico only..."
        act -j build -m "board:pico"
        ;;
        
    pico_w)
        echo "Running build for pico_w only..."
        act -j build -m "board:pico_w"
        ;;
        
    pico2_w)
        echo "Running build for pico2_w only..."
        act -j build -m "board:pico2_w"
        ;;
        
    dry)
        echo "Dry run - showing what would be executed:"
        act -n
        ;;
        
    gcc)
        echo "Testing with newer ARM GCC..."
        act -W .github/workflows/test-act-gcc.yml -j test-newer-gcc
        ;;
        
    auth)
        echo "Testing with GitHub authentication..."
        if [ -z "$GITHUB_TOKEN" ]; then
            echo "⚠️  Warning: GITHUB_TOKEN not set"
            echo "Set it with: export GITHUB_TOKEN=ghp_your_token_here"
            echo "Or see docs/ACT_AUTHENTICATION.md for setup instructions"
            exit 1
        fi
        act --secret GITHUB_TOKEN=$GITHUB_TOKEN push
        ;;
        
    help)
        show_usage
        ;;
        
    *)
        show_usage
        ;;
esac

echo ""
echo "Tips:"
echo "- Use 'act -j <job-name> --verbose' for detailed output"
echo "- Use 'act --container-architecture linux/arm64' on Apple Silicon"
echo "- Use 'act --rm' to remove containers after run"
echo "- Check .actrc for default configuration" 