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

# Step 0: Check for virtual environment (optional)
if [ -d ".venv" ]; then
    echo "Step 0: Using virtual environment (.venv)"
    source .venv/bin/activate
    echo "✓ Virtual environment activated"
else
    echo "Step 0: No virtual environment found - using system Python"
    echo "  (To use a venv, create it with: python3 -m venv .venv && .venv/bin/pip install -r requirements.txt)"
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
