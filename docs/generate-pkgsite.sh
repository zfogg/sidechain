#!/bin/bash
# Generate Go package documentation using pkgsite (pkg.go.dev style)
# Uses wget to mirror the pkgsite server output as static HTML

set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$( cd "$SCRIPT_DIR/.." && pwd )"
cd "$PROJECT_ROOT"

echo "=========================================="
echo "  Generating Go Package Documentation"
echo "  (pkg.go.dev style using pkgsite)"
echo "=========================================="
echo ""

# Check if pkgsite is installed
if ! command -v pkgsite >/dev/null 2>&1; then
    echo "Installing pkgsite..."
    go install golang.org/x/pkgsite/cmd/pkgsite@latest
    export PATH="$HOME/go/bin:$PATH"
fi

# Create output directory
mkdir -p docs/_build/html/backend/godoc

# Change to backend directory so pkgsite can find the Go modules
cd backend

echo "Starting pkgsite server..."
pkgsite -cache -http=:8080 . > /tmp/pkgsite.log 2>&1 &
PKGSITE_PID=$!

# Wait for server to start
echo "Waiting for pkgsite server to start..."
for i in {1..15}; do
    if curl -s http://localhost:8080/ > /dev/null 2>&1; then
        echo "✓ Pkgsite server is running"
        break
    fi
    if [ $i -eq 15 ]; then
        echo "✗ Pkgsite server failed to start"
        cat /tmp/pkgsite.log 2>/dev/null || echo "(no log available)"
        kill $PKGSITE_PID 2>/dev/null || true
        exit 1
    fi
    sleep 1
done

# Go back to project root
cd "$PROJECT_ROOT"

PACKAGE_PATH="github.com/zfogg/sidechain/backend"

echo "Downloading pkgsite documentation..."
# Use wget --mirror with proper options to download everything and convert links automatically
wget \
    --mirror \
    --convert-links \
    --adjust-extension \
    --page-requisites \
    --no-parent \
    --no-host-directories \
    --cut-dirs=0 \
    --directory-prefix=docs/_build/html/backend/godoc \
    --quiet \
    --level=20 \
    --wait=0 \
    --recursive \
    http://localhost:8080/${PACKAGE_PATH}/ || echo "Warning: Some files may not have downloaded"

# Stop pkgsite
kill $PKGSITE_PID 2>/dev/null || true
wait $PKGSITE_PID 2>/dev/null || true

# Create symlink for Sphinx compatibility
echo "Creating symlink for Sphinx compatibility..."
if [ -d "docs/_build/html/backend/godoc/github.com/zfogg/sidechain" ] && [ ! -d "docs/_build/html/backend/godoc/zfogg" ]; then
    mkdir -p docs/_build/html/backend/godoc/zfogg
    ln -sf ../github.com/zfogg/sidechain docs/_build/html/backend/godoc/zfogg/sidechain || \
    cp -r docs/_build/html/backend/godoc/github.com/zfogg/sidechain docs/_build/html/backend/godoc/zfogg/sidechain
    echo "✓ Created zfogg/sidechain path"
fi

echo ""
echo "✓ Go documentation generated successfully!"
echo "=========================================="
echo ""
echo "Output location: docs/_build/html/backend/godoc/"
HTML_COUNT=$(find docs/_build/html/backend/godoc -name "*.html" 2>/dev/null | wc -l)
echo "Downloaded $HTML_COUNT HTML files"
echo ""
echo "To view:"
echo "  open docs/_build/html/backend/godoc/index.html"
echo "  or: open docs/_build/html/backend/godoc/github.com/zfogg/sidechain/backend/index.html"
