# Act Quick Start Guide

## Installation

```bash
# Install or update act
brew install act
# or upgrade if already installed
brew upgrade act

# Verify version (0.2.79+ required for workspace mounting fix)
act --version
```

## Quick Commands

```bash
# List available workflows
act -l

# Run specific workflow
act -W .github/workflows/build.yml

# Dry run (see what would execute)
act -n
```

## What Works with Act 0.2.79+

✅ **Workspace mounting** - Fixed in v0.2.79! Files are now properly accessible  
✅ **Basic shell commands** - All standard Linux commands work  
✅ **Manual dependency installation** - apt-get, pip, etc.  
✅ **File operations** - Reading, writing, and validating files  
✅ **ARM64 architecture** - Use native ARM containers on Apple Silicon  
✅ **GitHub authentication** - Works with GitHub CLI token or PAT  

## Setting Up for Success

### 1. Use ARM64 Architecture (Apple Silicon)

Your `.actrc` is already configured for ARM64:
```
--container-architecture linux/arm64
```

### 2. Set Up GitHub Authentication

```bash
# Using GitHub CLI (recommended)
export GITHUB_TOKEN=$(gh auth token)

# Or create a Personal Access Token
# See docs/ACT_AUTHENTICATION.md for details
```

### 3. Use Standard Ubuntu Images

The `.actrc` uses standard Ubuntu images which support ARM64:
```
--platform ubuntu-latest=ubuntu:22.04
```

## Limitations

❌ **Default images lack tools** - Need to install Node.js, newer GCC, etc.  
❌ **Hardware testing** - No access to physical devices  
❌ **Complex matrix builds** - Limited support for matrix strategies  

## Recommended Approach

For Picocalc SD Boot development:

1. **Quick validation**: Use act to check workflow syntax
   ```bash
   act -n
   ```

2. **Build testing**: Use our Docker script (includes all tools)
   ```bash
   ./tools/run_ci_locally_docker.sh
   ```

3. **Full CI/CD**: Push to GitHub for complete testing

## Container Options

```bash
# Clean Docker
docker system prune -a --volumes

# Use smaller images
act -P ubuntu-latest=alpine:latest
```

## Troubleshooting

### Workspace Not Mounted

By default, act mounts the workspace to a different path. To fix this:

1. Use the `-b` flag to bind mount correctly:
```bash
act -b -W .github/workflows/test-act-simple.yml
```

2. Or add to `.actrc`:
```
-b
```

3. For manual checkout in workflows:
```yaml
- name: Checkout code
  run: |
    cp -r /workspace/. .
```

### External Actions Authentication

Act can't download external GitHub Actions without authentication. Solutions:

1. Use only shell commands (no external actions)
2. Set up GitHub token:
```bash
export GITHUB_TOKEN=your_token_here
act -s GITHUB_TOKEN=$GITHUB_TOKEN
```

3. Use offline action cache (advanced)

### Apple Silicon Issues

On M-series Macs, specify architecture:
```bash
act --container-architecture linux/amd64
``` 