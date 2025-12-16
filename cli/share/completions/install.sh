#!/bin/bash

# Installation script for sidechain-cli shell completions
# This script installs the appropriate completion file for your shell

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Detect shell
detect_shell() {
    if [ -n "$BASH_VERSION" ]; then
        echo "bash"
    elif [ -n "$ZSH_VERSION" ]; then
        echo "zsh"
    elif [ -n "$FISH_VERSION" ] || command -v fish >/dev/null 2>&1; then
        echo "fish"
    else
        echo ""
    fi
}

# Install bash completion
install_bash() {
    local source="$SCRIPT_DIR/bash/sidechain-cli"
    local dest

    if [[ "$OSTYPE" == "darwin"* ]]; then
        dest="/usr/local/etc/bash_completion.d/sidechain-cli"
    else
        dest="/etc/bash_completion.d/sidechain-cli"
    fi

    if [ ! -f "$source" ]; then
        echo -e "${RED}Error: Bash completion file not found at $source${NC}"
        return 1
    fi

    if sudo cp "$source" "$dest"; then
        echo -e "${GREEN}✓ Bash completion installed to $dest${NC}"
        echo -e "${YELLOW}Reload your shell with: exec bash${NC}"
        return 0
    else
        echo -e "${RED}Failed to install bash completion${NC}"
        return 1
    fi
}

# Install zsh completion
install_zsh() {
    local source="$SCRIPT_DIR/zsh/_sidechain-cli"
    local dest

    if [[ "$OSTYPE" == "darwin"* ]]; then
        dest="/usr/local/share/zsh/site-functions/_sidechain-cli"
    else
        dest="/usr/share/zsh/site-functions/_sidechain-cli"
    fi

    if [ ! -f "$source" ]; then
        echo -e "${RED}Error: Zsh completion file not found at $source${NC}"
        return 1
    fi

    if sudo cp "$source" "$dest"; then
        echo -e "${GREEN}✓ Zsh completion installed to $dest${NC}"
        echo -e "${YELLOW}Reload your shell with: exec zsh${NC}"
        return 0
    else
        echo -e "${RED}Failed to install zsh completion${NC}"
        return 1
    fi
}

# Install fish completion
install_fish() {
    local source="$SCRIPT_DIR/fish/sidechain-cli.fish"
    local dest="$HOME/.config/fish/completions/sidechain-cli.fish"

    if [ ! -f "$source" ]; then
        echo -e "${RED}Error: Fish completion file not found at $source${NC}"
        return 1
    fi

    mkdir -p "$HOME/.config/fish/completions"

    if cp "$source" "$dest"; then
        echo -e "${GREEN}✓ Fish completion installed to $dest${NC}"
        echo -e "${YELLOW}Reload your shell with: exec fish${NC}"
        return 0
    else
        echo -e "${RED}Failed to install fish completion${NC}"
        return 1
    fi
}

# Main installation
main() {
    echo "Sidechain CLI - Shell Completion Installer"
    echo "==========================================="
    echo ""

    local shell="${1:-$(detect_shell)}"

    if [ -z "$shell" ]; then
        echo -e "${RED}Could not detect your shell. Please specify: bash, zsh, or fish${NC}"
        echo "Usage: $0 [bash|zsh|fish]"
        exit 1
    fi

    case "$shell" in
        bash)
            install_bash
            ;;
        zsh)
            install_zsh
            ;;
        fish)
            install_fish
            ;;
        *)
            echo -e "${RED}Unknown shell: $shell${NC}"
            echo "Supported shells: bash, zsh, fish"
            exit 1
            ;;
    esac
}

main "$@"
