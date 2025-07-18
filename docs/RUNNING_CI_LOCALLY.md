# Running CI/CD Locally

This guide explains how to run the GitHub Actions workflows locally.

## Recommended Approach: Docker-based Testing

For reliable local CI testing, especially for build verification, use our Docker-based script:

```bash
# Run all CI tests locally
./tools/run_ci_locally_docker.sh
```

This script runs:
- Workspace validation
- Python tools validation  
- Build environment test
- CMake configuration for pico2_w
- Static analysis checks

## Alternative: Using Act (v0.2.79+)

Act can simulate GitHub Actions locally. With version 0.2.79+, workspace mounting issues on macOS are fixed!

### Prerequisites

1. Docker Desktop installed and running
2. Act v0.2.79 or newer: `brew install act` or `brew upgrade act`
3. Sufficient disk space (workflows may download large images)

### What Works

✅ **Workspace files are now accessible** (fixed in v0.2.79)  
✅ Basic workflow validation  
✅ Shell commands and scripts  
✅ File operations  

### What Doesn't Work

❌ External GitHub Actions (require authentication)  
❌ RP2350 builds (act containers have older ARM GCC)  
❌ Hardware-specific tests  

## Running Workflows

### Basic Usage

```bash
# List all available workflows and jobs
./run_workflows_locally.sh list

# Run the build job for all boards
./run_workflows_locally.sh build

# Run a specific board build
./run_workflows_locally.sh pico2_w

# Dry run to see what would be executed
./run_workflows_locally.sh dry
```

## Available Jobs

Our CI pipeline includes the following jobs:

| Job | Description | Dependencies |
|-----|-------------|--------------|
| `build` | Builds bootloader for all boards with static analysis | None |
| `sim` | Runs Renode simulation tests | build |
| `unit-tests` | Runs unit tests | build |
| `security` | Security analysis with CodeQL | None |
| `pio-test` | PlatformIO tests (nightly only) | build, sim |
| `hardware-test` | Hardware tests (manual trigger) | sim, unit-tests |
| `release` | Creates release artifacts | build, sim, unit-tests, security |

## Common Commands

### Run specific jobs:
```bash
# Build only
act -j build

# Simulation tests only
act -j sim

# Security analysis
act -j security
```

### Run with specific matrix values:
```bash
# Build for pico only
act -j build --matrix board:pico

# Build for pico2_w only
act -j build --matrix board:pico2_w
```

### Apple Silicon (M1/M2/M3) Users:
```bash
# Specify architecture to avoid warnings
act -j build --container-architecture linux/arm64

# Or add to your .actrc file:
echo "--container-architecture linux/arm64" >> .actrc
```

## Advanced Usage

### Verbose output:
```bash
act -j build --verbose
```

### Don't reuse containers:
```bash
act -j build --rm
```

### Use specific event:
```bash
# Simulate a pull request
act pull_request

# Simulate a push to main
act push

# Simulate scheduled run
act schedule
```

### Pass secrets:
```bash
act -s GITHUB_TOKEN=your_token_here
```

### Use different runner image:
```bash
# Use larger image with more tools
act --platform ubuntu-latest=catthehacker/ubuntu:full-latest
```

## Configuration

The `.actrc` file contains default configuration:
- Uses medium-sized runner image
- Sets CI environment variables
- Enables container reuse for faster runs
- Provides dummy secrets for testing

## Limitations

When running locally with act:

1. **Hardware tests** - Won't work unless you have physical hardware connected
2. **Renode in Docker** - May have issues with GUI/display
3. **Network access** - Some GitHub Actions may expect internet access
4. **Secrets** - Real secrets aren't available locally
5. **Artifacts** - Upload/download artifact actions work differently

## Troubleshooting

### "Docker daemon not running"
```bash
# Start Docker Desktop or:
sudo systemctl start docker
```

### "Permission denied"
```bash
# Add user to docker group
sudo usermod -aG docker $USER
# Log out and back in
```

### "Container architecture" warning on Apple Silicon
```bash
# Use ARM64 architecture
act --container-architecture linux/arm64
```

### Out of disk space
```bash
# Clean up Docker images
docker system prune -a
```

## Tips

1. **Start small** - Test individual jobs before running the entire workflow
2. **Use dry run** - Check what will be executed with `act -n`
3. **Cache wisely** - Container reuse speeds up iterations
4. **Check logs** - Use `--verbose` for debugging
5. **Matrix builds** - Test specific configurations with `--matrix`

## Example Workflow

```bash
# 1. First, do a dry run
./run_workflows_locally.sh dry

# 2. Build for your target board
./run_workflows_locally.sh pico2_w

# 3. Run simulation tests
act -j sim

# 4. If everything passes, run the full pipeline
act

# 5. Clean up when done
docker system prune
```

## Integration with Development

Use act to:
- Verify CI changes before pushing
- Debug failing workflows
- Test matrix configurations
- Validate workflow syntax
- Speed up development iteration

Remember: act simulates GitHub Actions but isn't identical. Always verify critical changes with a real GitHub push. 

## Known Limitations

### External Actions Authentication

Act cannot download external GitHub Actions without authentication. Solutions:

1. **Use workflows without external actions** (recommended)
2. Set up GitHub token:
```bash
export GITHUB_TOKEN=your_token_here
act -s GITHUB_TOKEN=$GITHUB_TOKEN
```

### Build Environment Compatibility

The act containers use older versions of ARM GCC (10.3.1) that may not be compatible with RP2350 development. For build testing, use:

1. **Docker directly** (recommended):
```bash
./tools/run_ci_locally_docker.sh
```

2. **GitHub Codespaces** or push to GitHub for real CI builds

### Apple Silicon Issues

On M-series Macs, specify architecture:
```bash
act --container-architecture linux/amd64
``` 