#!/bin/bash
# Rebuild documentation after code changes (especially after renaming classes)

set -e

echo "=========================================="
echo "  Rebuilding Sidechain Documentation"
echo "=========================================="
echo ""

# Get the directory where this script is located
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$SCRIPT_DIR"

# Step 0: Setup virtual environment if it doesn't exist
if [ ! -d ".venv" ]; then
    echo "Step 0: Creating virtual environment..."
    python3 -m venv .venv
    echo "✓ Virtual environment created"
    
    echo ""
    echo "Installing Python dependencies..."
    .venv/bin/pip install --upgrade pip
    .venv/bin/pip install -r requirements.txt
    echo "✓ Dependencies installed"
else
    echo "Step 0: Using existing virtual environment"
    # Activate venv for this script (though CMake will find it automatically)
    source .venv/bin/activate
fi

echo ""
echo "Step 1: Cleaning old generated files..."
rm -rf build-docs doxygen api _build
echo "✓ Cleaned"

echo ""
echo "Step 2: Running CMake configuration..."
cmake -S . -B build-docs
echo "✓ Configured"

echo ""
echo "Step 3: Building documentation..."
echo "  (This runs Doxygen → Exhale → Sphinx)"
cmake --build build-docs --target docs

echo ""
echo "=========================================="
echo "  Documentation rebuild complete!"
echo "=========================================="
echo ""
echo "Output location: build-docs/html/index.html"
echo ""
echo "To view the docs:"
echo "  macOS:   open build-docs/html/index.html"
echo "  Linux:   xdg-open build-docs/html/index.html"
echo "  Windows: start build-docs/html/index.html"
echo ""
