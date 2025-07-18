# Setting Up GitHub Authentication for Act

This guide explains how to authenticate act with GitHub to use external actions.

## Why Authentication is Needed

Act needs to download external GitHub Actions (like `actions/checkout@v4`, `actions/setup-python@v4`). Without authentication, you'll see errors like:
```
authentication required: Support for password authentication was removed on August 13, 2021.
```

## Setting Up Authentication

### Option 1: Personal Access Token (Recommended)

1. **Create a GitHub Personal Access Token**:
   - Go to GitHub → Settings → Developer settings → Personal access tokens → Tokens (classic)
   - Click "Generate new token (classic)"
   - Give it a name like "act-local-testing"
   - Select scopes:
     - `repo` (Full control of private repositories)
     - `workflow` (Update GitHub Action workflows)
   - Generate and copy the token

2. **Set up the token for act**:
   ```bash
   # Option A: Export as environment variable
   export GITHUB_TOKEN=ghp_your_token_here
   
   # Option B: Create a .secrets file (don't commit this!)
   echo "GITHUB_TOKEN=ghp_your_token_here" > .secrets
   
   # Option C: Add to your shell profile
   echo 'export GITHUB_TOKEN=ghp_your_token_here' >> ~/.zshrc
   source ~/.zshrc
   ```

3. **Use the token with act**:
   ```bash
   # If using environment variable
   act -s GITHUB_TOKEN=$GITHUB_TOKEN
   
   # If using .secrets file
   act --secret-file .secrets
   
   # The token will be automatically used if GITHUB_TOKEN is set
   act
   ```

### Option 2: GitHub CLI Authentication

If you have GitHub CLI installed:
```bash
# Authenticate with GitHub CLI
gh auth login

# Get the token
export GITHUB_TOKEN=$(gh auth token)

# Run act
act
```

### Option 3: Use in .actrc

Add to your `.actrc` file:
```
--secret GITHUB_TOKEN=${GITHUB_TOKEN}
```

## Security Best Practices

1. **Never commit tokens**: Add `.secrets` to `.gitignore`
   ```bash
   echo ".secrets" >> .gitignore
   ```

2. **Use minimal scopes**: Only grant the permissions needed

3. **Rotate tokens regularly**: Delete and recreate tokens periodically

4. **Use read-only tokens** when possible for testing

## Testing Authentication

Create a test workflow:
```yaml
# .github/workflows/test-auth.yml
name: test-auth
on: workflow_dispatch
jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-python@v4
        with:
          python-version: '3.10'
      - run: |
          echo "✅ Authentication working!"
          python --version
```

Run it:
```bash
act -W .github/workflows/test-auth.yml --secret GITHUB_TOKEN=$GITHUB_TOKEN
```

## Troubleshooting

### Token not working?
- Ensure the token has not expired
- Check it has the required scopes
- Verify you're using the correct token format (`ghp_...`)

### Still getting authentication errors?
- Some actions may need additional permissions
- Try using a token with full `repo` scope
- Check if the action repository is public or private

### Alternative: Use Docker Instead
If authentication is problematic, use our Docker-based testing:
```bash
./tools/run_ci_locally_docker.sh
``` 