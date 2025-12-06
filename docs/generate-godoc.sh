#!/bin/bash
# Generate Go package documentation locally
# This script mimics what the CI workflow does

set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$( cd "$SCRIPT_DIR/.." && pwd )"
cd "$PROJECT_ROOT"

echo "=========================================="
echo "  Generating Go Package Documentation"
echo "=========================================="
echo ""

# Check if godoc is installed
if ! command -v godoc >/dev/null 2>&1; then
    echo "Installing godoc..."
    go install golang.org/x/tools/cmd/godoc@latest
    export PATH="$HOME/go/bin:$PATH"
fi

# Check if wget is installed
if ! command -v wget >/dev/null 2>&1; then
    echo "Error: wget is required but not installed"
    echo "Install with: sudo apt-get install wget (Linux) or brew install wget (macOS)"
    exit 1
fi

# Create output directory
mkdir -p docs/_build/html/backend/godoc

# Change to backend directory so godoc can find the Go modules
cd backend

echo "Starting godoc server..."
# Start godoc server in background
godoc -http=:6060 &
GODOC_PID=$!

# Wait for server to start and verify it's running
echo "Waiting for godoc server to start..."
for i in {1..10}; do
    if curl -s http://localhost:6060/ > /dev/null 2>&1; then
        echo "✓ Godoc server is running"
        break
    fi
    if [ $i -eq 10 ]; then
        echo "✗ Godoc server failed to start"
        kill $GODOC_PID 2>/dev/null || true
        exit 1
    fi
    sleep 1
done

# Go back to project root
cd "$PROJECT_ROOT"

# Download all package pages using wget
# Create lib directory for assets
mkdir -p docs/_build/html/backend/godoc/lib

# Download CSS and JS assets
echo "Downloading godoc assets..."
wget -q -P docs/_build/html/backend/godoc/lib http://localhost:6060/lib/godoc.js || echo "Warning: Failed to download godoc.js"
wget -q -P docs/_build/html/backend/godoc/lib http://localhost:6060/lib/jquery.js || echo "Warning: Failed to download jquery.js"
wget -q -P docs/_build/html/backend/godoc/lib http://localhost:6060/lib/jquery.treeview.css || echo "Warning: Failed to download jquery.treeview.css"
wget -q -P docs/_build/html/backend/godoc/lib http://localhost:6060/lib/jquery.treeview.js || echo "Warning: Failed to download jquery.treeview.js"
wget -q -P docs/_build/html/backend/godoc/lib http://localhost:6060/lib/playground.js || echo "Warning: Failed to download playground.js"

# Download main pages
echo "Downloading godoc pages..."
wget -q -P docs/_build/html/backend/godoc http://localhost:6060/ --output-document=index.html || echo "Warning: Failed to download index.html"
wget -q -P docs/_build/html/backend/godoc http://localhost:6060/pkg/ --output-document=pkg.html || echo "Warning: Failed to download pkg.html"

# Download our backend packages recursively
echo "Downloading backend package documentation..."
wget -q \
    --recursive \
    --level=10 \
    --no-parent \
    --no-host-directories \
    --cut-dirs=1 \
    --convert-links \
    --adjust-extension \
    --page-requisites \
    --directory-prefix=docs/_build/html/backend/godoc \
    http://localhost:6060/pkg/github.com/zfogg/sidechain/backend/ || echo "Warning: Failed to download some package pages"

# Fix paths in downloaded HTML files to use relative paths
echo "Fixing paths in downloaded HTML files..."
find docs/_build/html/backend/godoc -name "*.html" -type f 2>/dev/null | while read f; do
    sed -i \
        -e 's|href="/lib/|href="lib/|g' \
        -e 's|src="/lib/|src="lib/|g' \
        -e 's|href="/pkg/|href="pkg/|g' \
        -e 's|action="/search|action="search|g' \
        "$f" || true
done

# Stop godoc
echo "Stopping godoc server..."
kill $GODOC_PID || true
wait $GODOC_PID 2>/dev/null || true

# Verify we got the files (check for any HTML files in the godoc directory)
HTML_COUNT=$(find docs/_build/html/backend/godoc -name "*.html" 2>/dev/null | wc -l)
if [ "$HTML_COUNT" -gt 0 ]; then
    echo ""
    echo "=========================================="
    echo "  ✓ Go documentation generated successfully!"
    echo "=========================================="
    echo ""
    echo "Output location: docs/_build/html/backend/godoc/"
    echo ""
    # Count downloaded files
    HTML_COUNT=$(find docs/_build/html/backend/godoc -name "*.html" 2>/dev/null | wc -l)
    echo "Downloaded $HTML_COUNT HTML files"
    echo ""
    echo "Note: Static assets (JS/CSS) may be missing but HTML pages should work"
    echo ""
    echo "To view:"
    echo "  open docs/_build/html/backend/godoc/index.html"
    echo "  or: open docs/_build/html/backend/godoc/pkg/github.com/zfogg/sidechain/backend/index.html"
else
    echo ""
    echo "=========================================="
    echo "  ✗ Failed to generate Go documentation"
    echo "=========================================="
    exit 1
fi

