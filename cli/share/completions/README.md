# Shell Completions for sidechain

This directory contains shell completion scripts for `sidechain` for different shells.

## Installation

### Bash

Copy the completion file to your bash completions directory:

```bash
# For Homebrew (if using Homebrew)
cp bash/sidechain /usr/local/etc/bash_completion.d/

# For Linux (systemwide)
sudo cp bash/sidechain /etc/bash_completion.d/

# For macOS (systemwide)
sudo cp bash/sidechain /usr/local/etc/bash_completion.d/

# For user-only (add to ~/.bashrc or ~/.bash_profile)
mkdir -p ~/.bash_completion.d
cp bash/sidechain ~/.bash_completion.d/
echo 'source ~/.bash_completion.d/sidechain' >> ~/.bashrc
```

Then reload your shell:
```bash
source ~/.bashrc
```

### Zsh

Copy the completion file to your zsh completions directory:

```bash
# For Homebrew (if using Homebrew)
cp zsh/_sidechain /usr/local/share/zsh/site-functions/

# For systemwide
sudo cp zsh/_sidechain /usr/share/zsh/site-functions/

# For user-only
mkdir -p ~/.zsh/completions
cp zsh/_sidechain ~/.zsh/completions/
echo 'fpath=(~/.zsh/completions $fpath)' >> ~/.zshrc
echo 'autoload -U compinit && compinit' >> ~/.zshrc
```

Then reload your shell:
```bash
source ~/.zshrc
```

### Fish

Copy the completion file to your fish completions directory:

```bash
# Standard location
mkdir -p ~/.config/fish/completions
cp fish/sidechain.fish ~/.config/fish/completions/
```

Fish will automatically load completions from `~/.config/fish/completions/` on startup.

## Quick Load (Current Session Only)

To load completions for the current session without installing permanently:

**Bash:**
```bash
source <(sidechain completion bash)
```

**Zsh:**
```bash
source <(sidechain completion zsh)
```

**Fish:**
```bash
sidechain completion fish | source
```

## Available Completions

The completion scripts provide context-aware completions for:

- **Commands**: All available sidechain commands and subcommands
- **Flags**: All command flags with descriptions
- **Flag values**: Dynamic completion for enum-type flags where applicable
- **Command descriptions**: Help text for commands and flags

### Examples

```bash
# Complete commands
sidechain auth<TAB>      # Suggests: auth, audio, admin, etc.

# Complete subcommands
sidechain auth <TAB>     # Suggests: login, logout, me, register, etc.

# Complete flags
sidechain post --<TAB>   # Suggests: --help, --verbose, --output, etc.

# Complete flag descriptions
sidechain feed --<TAB>   # Shows descriptions for available flags
```

## Generating Completions

If you need to regenerate the completion scripts (e.g., after updating the CLI):

```bash
# Generate all completions
sidechain completion bash > share/completions/bash/sidechain
sidechain completion zsh > share/completions/zsh/_sidechain
sidechain completion fish > share/completions/fish/sidechain.fish
```

## Troubleshooting

If completions aren't working:

1. **Bash**: Make sure the completion file is in your `bash_completion.d` directory and is sourced in your `.bashrc`
2. **Zsh**: Verify that `compinit` is being called after adding the completion to `fpath`
3. **Fish**: Check that the completion file is in `~/.config/fish/completions/` and has the `.fish` extension
4. **Reload**: Run `exec $SHELL` to reload your shell after installation

## Files

- `bash/sidechain-cli` - Bash completion script
- `zsh/_sidechain-cli` - Zsh completion script (note the `_` prefix, required by zsh)
- `fish/sidechain-cli.fish` - Fish completion script
