#!/bin/bash
#
# Sidechain Dependency Installation Script
# Supports: Arch Linux, Ubuntu/Debian, macOS
#
# Usage: ./scripts/install-deps.sh
#

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

print_status() {
    echo -e "${BLUE}==>${NC} $1"
}

print_success() {
    echo -e "${GREEN}[OK]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Detect OS
detect_os() {
    if [[ "$OSTYPE" == "darwin"* ]]; then
        echo "macos"
    elif [ -f /etc/arch-release ]; then
        echo "arch"
    elif [ -f /etc/debian_version ]; then
        echo "debian"
    elif [ -f /etc/fedora-release ]; then
        echo "fedora"
    else
        echo "unknown"
    fi
}

# Install dependencies for Arch Linux
install_arch() {
    print_status "Installing dependencies for Arch Linux..."

    # Core build tools
    sudo pacman -S --needed --noconfirm \
        base-devel \
        cmake \
        ninja \
        git \
        clang \
        lld \
        ccache

    # JUCE dependencies
    sudo pacman -S --needed --noconfirm \
        alsa-lib \
        freetype2 \
        libx11 \
        libxcursor \
        libxext \
        libxinerama \
        libxrandr \
        libxrender \
        webkit2gtk \
        gtk3 \
        curl \
        pkg-config

    # libkeyfinder dependencies
    sudo pacman -S --needed --noconfirm \
        fftw \
        catch2

    # Go (for backend)
    sudo pacman -S --needed --noconfirm go

    # FFmpeg (for audio processing)
    sudo pacman -S --needed --noconfirm ffmpeg

    # PostgreSQL client (for backend development)
    sudo pacman -S --needed --noconfirm postgresql-libs

    print_success "Arch Linux dependencies installed"
}

# Install dependencies for Ubuntu/Debian
install_debian() {
    print_status "Installing dependencies for Ubuntu/Debian..."

    # Update package list
    sudo apt-get update

    # Core build tools
    sudo apt-get install -y \
        build-essential \
        cmake \
        ninja-build \
        git \
        clang \
        lld \
        ccache \
        pkg-config

    # JUCE dependencies
    sudo apt-get install -y \
        libasound2-dev \
        libfreetype6-dev \
        libx11-dev \
        libxcursor-dev \
        libxext-dev \
        libxinerama-dev \
        libxrandr-dev \
        libxrender-dev \
        libwebkit2gtk-4.0-dev \
        libgtk-3-dev \
        libcurl4-openssl-dev \
        ladspa-sdk

    # libkeyfinder dependencies
    sudo apt-get install -y \
        libfftw3-dev

    # Catch2 might not be in older Ubuntu repos, install via cmake if needed
    if apt-cache show catch2 &>/dev/null; then
        sudo apt-get install -y catch2
    else
        print_warning "Catch2 not in repos, will be fetched by CMake"
    fi

    # Go (for backend) - use official repo for latest version
    if ! command -v go &> /dev/null; then
        print_status "Installing Go..."
        sudo apt-get install -y golang-go || {
            print_warning "golang-go package old, consider installing from golang.org"
        }
    fi

    # FFmpeg (for audio processing)
    sudo apt-get install -y ffmpeg

    # PostgreSQL client (for backend development)
    sudo apt-get install -y libpq-dev

    print_success "Ubuntu/Debian dependencies installed"
}

# Install dependencies for Fedora
install_fedora() {
    print_status "Installing dependencies for Fedora..."

    # Core build tools
    sudo dnf install -y \
        @development-tools \
        cmake \
        ninja-build \
        git \
        clang \
        lld \
        ccache

    # JUCE dependencies
    sudo dnf install -y \
        alsa-lib-devel \
        freetype-devel \
        libX11-devel \
        libXcursor-devel \
        libXext-devel \
        libXinerama-devel \
        libXrandr-devel \
        libXrender-devel \
        webkit2gtk3-devel \
        gtk3-devel \
        libcurl-devel

    # libkeyfinder dependencies
    sudo dnf install -y \
        fftw-devel \
        catch2-devel

    # Go (for backend)
    sudo dnf install -y golang

    # FFmpeg (for audio processing)
    sudo dnf install -y ffmpeg ffmpeg-devel

    # PostgreSQL client
    sudo dnf install -y postgresql-devel

    print_success "Fedora dependencies installed"
}

# Install dependencies for macOS
install_macos() {
    print_status "Installing dependencies for macOS..."

    # Check for Homebrew
    if ! command -v brew &> /dev/null; then
        print_error "Homebrew not found. Please install from https://brew.sh"
        exit 1
    fi

    # Core build tools
    brew install cmake ninja ccache

    # libkeyfinder dependencies
    brew install fftw catch2

    # Go (for backend)
    brew install go

    # FFmpeg (for audio processing)
    brew install ffmpeg

    # PostgreSQL client
    brew install postgresql

    print_success "macOS dependencies installed"
    print_warning "Note: Xcode Command Line Tools are required. Run: xcode-select --install"
}

# Build libkeyfinder
build_libkeyfinder() {
    print_status "Building libkeyfinder..."

    SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
    LIBKEYFINDER_DIR="$PROJECT_ROOT/deps/libkeyfinder"

    if [ ! -d "$LIBKEYFINDER_DIR" ]; then
        print_error "libkeyfinder not found at $LIBKEYFINDER_DIR"
        print_status "Initializing git submodules..."
        cd "$PROJECT_ROOT"
        git submodule update --init --recursive
    fi

    cd "$LIBKEYFINDER_DIR"

    # Create build directory
    mkdir -p build
    cd build

    # Configure with CMake
    cmake -DCMAKE_BUILD_TYPE=Release \
          -DBUILD_TESTING=OFF \
          -DBUILD_SHARED_LIBS=OFF \
          ..

    # Build
    cmake --build . --parallel $(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

    print_success "libkeyfinder built successfully"
}

# Initialize git submodules
init_submodules() {
    print_status "Initializing git submodules..."

    SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

    cd "$PROJECT_ROOT"
    git submodule update --init --recursive

    print_success "Git submodules initialized"
}

# Main
main() {
    echo ""
    echo "========================================"
    echo "  Sidechain Dependency Installer"
    echo "========================================"
    echo ""

    OS=$(detect_os)
    print_status "Detected OS: $OS"

    case $OS in
        arch)
            install_arch
            ;;
        debian)
            install_debian
            ;;
        fedora)
            install_fedora
            ;;
        macos)
            install_macos
            ;;
        *)
            print_error "Unsupported operating system"
            echo "Supported: Arch Linux, Ubuntu/Debian, Fedora, macOS"
            exit 1
            ;;
    esac

    echo ""
    init_submodules

    echo ""
    build_libkeyfinder

    echo ""
    echo "========================================"
    print_success "All dependencies installed!"
    echo "========================================"
    echo ""
    echo "Next steps:"
    echo "  1. Build the plugin:  make plugin"
    echo "  2. Build the backend: make backend"
    echo "  3. Run tests:         make test"
    echo ""
}

main "$@"
