#!/bin/bash
# Generate Go package documentation locally using pkgsite (pkg.go.dev style)
# This script mimics what the CI workflow does

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

# Check if wget is installed
if ! command -v wget >/dev/null 2>&1; then
    echo "Error: wget is required but not installed"
    echo "Install with: sudo apt-get install wget (Linux) or brew install wget (macOS)"
    exit 1
fi

# Create output directory
mkdir -p docs/_build/html/backend/godoc

# Change to backend directory so pkgsite can find the Go modules
cd backend

echo "Starting pkgsite server..."
# Start pkgsite server in background (default port is 8080)
# Use -cache flag to serve from module cache (works for local packages)
pkgsite -cache -http=:8080 . > /tmp/pkgsite.log 2>&1 &
PKGSITE_PID=$!

# Wait for server to start and verify it's running
echo "Waiting for pkgsite server to start..."
for i in {1..15}; do
    if curl -s http://localhost:8080/ > /dev/null 2>&1; then
        echo "✓ Pkgsite server is running"
        break
    fi
    if [ $i -eq 15 ]; then
        echo "✗ Pkgsite server failed to start"
        echo "Pkgsite log:"
        cat /tmp/pkgsite.log 2>/dev/null || echo "(no log available)"
        kill $PKGSITE_PID 2>/dev/null || true
        exit 1
    fi
    sleep 1
done

# Go back to project root
cd "$PROJECT_ROOT"

# Download all package pages and assets using wget mirror
# This automatically converts links to be relative
echo "Downloading pkgsite documentation and assets..."
# pkgsite uses /github.com/username/repo/... path structure
PACKAGE_PATH="github.com/zfogg/sidechain/backend"

# First download the static assets directory
echo "Downloading static assets..."
wget \
    --mirror \
    --convert-links \
    --adjust-extension \
    --page-requisites \
    --no-parent \
    --no-host-directories \
    --cut-dirs=0 \
    --directory-prefix=docs/_build/html/backend/godoc \
    --accept="*.css,*.js,*.woff,*.woff2,*.ttf,*.svg,*.png,*.ico" \
    --reject="*.html,robots.txt" \
    --quiet \
    --level=5 \
    http://localhost:8080/static/ || echo "Note: Some static assets may not be available"

# Then download the package pages
echo "Downloading package pages..."
wget \
    --mirror \
    --convert-links \
    --adjust-extension \
    --page-requisites \
    --no-parent \
    --no-host-directories \
    --cut-dirs=0 \
    --directory-prefix=docs/_build/html/backend/godoc \
    --accept="*.html" \
    --reject="robots.txt" \
    --quiet \
    --level=10 \
    http://localhost:8080/${PACKAGE_PATH}/ || echo "Warning: Failed to download some files"

# Fix any remaining localhost URLs to use relative paths
echo "Fixing any remaining absolute URLs..."
find docs/_build/html/backend/godoc -name "*.html" -type f 2>/dev/null | while read f; do
    # Calculate relative depth from godoc root
    REL_PATH=$(echo "$f" | sed 's|docs/_build/html/backend/godoc/||')
    if [ -n "$REL_PATH" ] && [ "$REL_PATH" != "index.html" ]; then
        REL_DEPTH=$(echo "$REL_PATH" | tr -cd '/' | wc -c)
        if [[ "$REL_PATH" == *"/"* ]]; then
            REL_DEPTH=$((REL_DEPTH + 1))
        fi
        if [ $REL_DEPTH -gt 0 ]; then
            REL_PREFIX=$(printf '../%.0s' $(seq 1 $REL_DEPTH))
        else
            REL_PREFIX=""
        fi
    else
        REL_PREFIX=""
    fi
    
    # Replace localhost URLs with relative paths
    sed -i \
        -e 's|http://localhost:8080/static/|'${REL_PREFIX}'static/|g' \
        -e 's|http://localhost:8080/github.com/|'${REL_PREFIX}'github.com/|g' \
        -e 's|http://localhost:8080/|'${REL_PREFIX}'|g' \
        "$f" || true
done

# Stop pkgsite
echo "Stopping pkgsite server..."
kill $PKGSITE_PID || true
wait $PKGSITE_PID 2>/dev/null || true

# Create symlink for zfogg/sidechain path (without github.com prefix)
# This matches the links in docs/backend/index.rst
echo "Creating symlink for zfogg/sidechain path..."
if [ -d "docs/_build/html/backend/godoc/github.com/zfogg/sidechain" ] && [ ! -d "docs/_build/html/backend/godoc/zfogg" ]; then
    mkdir -p docs/_build/html/backend/godoc/zfogg
    ln -sf ../github.com/zfogg/sidechain docs/_build/html/backend/godoc/zfogg/sidechain || \
    cp -r docs/_build/html/backend/godoc/github.com/zfogg/sidechain docs/_build/html/backend/godoc/zfogg/sidechain
    echo "✓ Created zfogg/sidechain path"
fi

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
    echo "To view:"
    echo "  open docs/_build/html/backend/godoc/index.html"
    echo "  or: open docs/_build/html/backend/godoc/${PACKAGE_PATH}/index.html"
else
    echo ""
    echo "=========================================="
    echo "  ✗ Failed to generate Go documentation"
    echo "=========================================="
    exit 1
fi

