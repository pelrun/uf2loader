# Local Testing Guide

This guide shows you how to run all CI/CD tests locally before pushing to GitHub.

## Quick Start: Run Everything

```bash
# Set up GitHub authentication (one time)
export GITHUB_TOKEN=$(gh auth token)

# Run all tests
./tools/run_all_tests_locally.sh
```

## Important: Apple Silicon (M1/M2/M3) Users

Due to architecture limitations with act and GitHub Actions on ARM64:

### Recommended: Use Docker Build Tests
```bash
# This is the most reliable approach for Apple Silicon
./tools/run_all_tests_locally.sh --docker-only

# Or directly:
./tools/run_ci_locally_docker.sh
```

### Why Docker Tests are Better on Apple Silicon

1. **Native ARM64 execution** - No emulation overhead
2. **Proper tool versions** - Installs ARM64 versions of all tools
3. **No architecture conflicts** - Avoids x86_64/ARM64 binary issues
4. **Matches GitHub CI** - Uses same build steps as real CI

### Act Limitations on ARM64

When using act on Apple Silicon, you may encounter:
- **Rosetta errors**: Some GitHub Actions install x86_64 binaries
- **Missing ARM64 images**: Not all container images support ARM64
- **Tool compatibility**: Some actions don't support ARM64 properly

To use act despite limitations:
```bash
# Force x86_64 emulation (slower but more compatible)
act -j build --container-architecture linux/amd64

# Or update .actrc to use x86_64:
# --container-architecture linux/amd64
```

## Individual Test Options

### 1. Quick Validation (< 1 minute)
```bash
# Just validate workflows and Python tools
./tools/run_all_tests_locally.sh --quick
```

### 2. Act Workflows Only
```bash
# Run only act-based tests
./tools/run_all_tests_locally.sh --act-only

# Or manually:
act -n  # Dry run validation
act -W .github/workflows/build.yml -j build --secret GITHUB_TOKEN=$GITHUB_TOKEN
```

### 3. Docker Build Tests Only
```bash
# Run only Docker-based build tests
./tools/run_all_tests_locally.sh --docker-only

# Or manually:
./tools/run_ci_locally_docker.sh
```

### 4. Specific Workflow Jobs
```bash
# List all available jobs
act -l

# Run specific job
act -j unit-tests
act -j build-pico2_w
act -j simulation-tests
```

### 5. Local Build and Test
```bash
# Build locally
cmake -S . -B build -DPICO_BOARD=pico2_w
cmake --build build

# Run tests
cd build && ctest --output-on-failure
```

## Test Types Explained

### Workflow Validation (act -n)
- Checks YAML syntax
- Validates job dependencies
- Ensures actions exist
- Very fast (seconds)

### Act Build Tests
- Runs actual workflow steps
- May fail due to container limitations
- Good for testing logic flow
- Medium speed (minutes)

### Docker Build Tests
- Full build environment
- Includes proper ARM GCC
- Most accurate local test
- Slower (5-10 minutes)

### Unit Tests
- C/C++ tests via CTest
- Python tests via pytest
- Fast (seconds to minutes)

## Common Commands

```bash
# Before pushing to GitHub
export GITHUB_TOKEN=$(gh auth token)
./tools/run_all_tests_locally.sh

# Quick check before commit
./tools/run_all_tests_locally.sh --quick

# After making workflow changes
act -n

# After changing build configuration
./tools/run_ci_locally_docker.sh

# After changing test code
cd build && ctest --rerun-failed --output-on-failure
```

## Troubleshooting

### "GITHUB_TOKEN not set"
```bash
# Option 1: GitHub CLI
gh auth login
export GITHUB_TOKEN=$(gh auth token)

# Option 2: Personal Access Token
export GITHUB_TOKEN=ghp_your_token_here
```

### Act build fails
This is often expected due to container limitations. Check if the failure is due to:
- Missing tools (Node.js, newer GCC) - Expected
- Actual code issues - Fix these

### Docker tests fail
These should match GitHub CI closely. Common issues:
- Out of disk space: `docker system prune -a`
- Network issues: Check internet connection
- Permission issues: Ensure Docker Desktop is running

## Best Practices

1. **Before every push**: Run quick validation
   ```bash
   ./tools/run_all_tests_locally.sh --quick
   ```

2. **After significant changes**: Run full test suite
   ```bash
   ./tools/run_all_tests_locally.sh
   ```

3. **For workflow debugging**: Use act with specific jobs
   ```bash
   act -j problematic-job --verbose
   ```

4. **For build issues**: Use Docker tests
   ```bash
   ./tools/run_ci_locally_docker.sh
   ```

## Summary

The combination of tools gives you comprehensive local testing:

- **act**: Workflow validation and logic testing
- **Docker script**: Full build environment testing
- **Local builds**: Direct testing and debugging
- **All-in-one script**: Runs everything for you

This ensures your code is tested before pushing to GitHub! 