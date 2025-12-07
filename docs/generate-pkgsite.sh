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
for i in {1..30}; do
    if curl -s http://localhost:8080/ > /dev/null 2>&1; then
        echo "✓ Pkgsite server is running"
        break
    fi
    if [ $i -eq 30 ]; then
        echo "✗ Pkgsite server failed to start after 30 seconds"
        echo "Pkgsite log:"
        cat /tmp/pkgsite.log 2>/dev/null || echo "(no log available)"
        echo "Checking if process is running:"
        ps aux | grep pkgsite | grep -v grep || echo "No pkgsite process found"
        kill $PKGSITE_PID 2>/dev/null || true
        exit 1
    fi
    sleep 1
done

# Go back to project root
cd "$PROJECT_ROOT"

PACKAGE_PATH="github.com/zfogg/sidechain/backend"

echo "Downloading pkgsite documentation..."
# Use wget --mirror to recursively download all pages and assets
# Remove --no-parent so it can follow absolute paths like /github.com/... and /static/...
# Don't use --adjust-extension as it breaks CSS files with query strings
wget \
    --mirror \
    --convert-links \
    --page-requisites \
    --no-host-directories \
    --cut-dirs=0 \
    --directory-prefix=docs/_build/html/backend/godoc \
    --timeout=10 \
    --tries=2 \
    --level=10 \
    --wait=0 \
    --no-verbose \
    http://localhost:8080/${PACKAGE_PATH}/ 2>&1 | head -50 || echo "Warning: Some files may not have downloaded"

# Explicitly download all subpackages that wget might have missed
# Exclude cmd/ packages as they are CLI tools, not library documentation
echo "Downloading subpackages explicitly..."
for pkg in \
    "internal/audio" \
    "internal/auth" \
    "internal/database" \
    "internal/email" \
    "internal/handlers" \
    "internal/middleware" \
    "internal/models" \
    "internal/queue" \
    "internal/recommendations" \
    "internal/search" \
    "internal/seed" \
    "internal/storage" \
    "internal/stories" \
    "internal/stream" \
    "internal/websocket"; do
    echo "  Downloading ${PACKAGE_PATH}/${pkg}..."
    wget \
        --mirror \
        --convert-links \
        --page-requisites \
        --no-host-directories \
        --cut-dirs=0 \
        --directory-prefix=docs/_build/html/backend/godoc \
        --timeout=10 \
        --tries=2 \
        --level=5 \
        --wait=0 \
        --no-verbose \
        "http://localhost:8080/${PACKAGE_PATH}/${pkg}/" 2>&1 | head -10 || echo "    Warning: Failed to download ${pkg}"
done

# Stop pkgsite
kill $PKGSITE_PID 2>/dev/null || true
wait $PKGSITE_PID 2>/dev/null || true

# Remove the files/ directory which contains source code files with full filesystem paths
# These aren't needed for the documentation and create confusing paths
echo "Removing files/ directory (source code files)..."
rm -rf docs/_build/html/backend/godoc/files

# Fix query strings in filenames (wget saves ?version= as part of filename, but HTTP treats ? as query separator)
echo "Fixing query strings in filenames..."
find docs/_build/html/backend/godoc/static -name "*?version=*" -type f | while read f; do
    NEW_NAME=$(echo "$f" | sed 's|?version=$||')
    if [ "$f" != "$NEW_NAME" ]; then
        mv "$f" "$NEW_NAME" 2>/dev/null || true
    fi
done

# Fix absolute paths and remove query strings from HTML references
echo "Fixing absolute paths and query strings in HTML/JS files..."
find docs/_build/html/backend/godoc -name "*.html" -o -name "*.js" | while read f; do
    # Calculate relative depth from godoc root
    REL_PATH=$(echo "$f" | sed 's|docs/_build/html/backend/godoc/||')
    
    if [ -n "$REL_PATH" ] && [ "$REL_PATH" != "index.html" ]; then
        # Count directory separators (depth from godoc root)
        REL_DEPTH=$(echo "$REL_PATH" | sed 's|[^/]*$||' | tr -cd '/' | wc -c)
        if [ $REL_DEPTH -gt 0 ]; then
            REL_PREFIX=$(printf '../%.0s' $(seq 1 $REL_DEPTH))
        else
            REL_PREFIX=""
        fi
    else
        REL_PREFIX=""
    fi
    
    # Replace absolute paths with relative paths, remove query strings, and strip version tags
    sed -i \
        -e 's|href="/static/|href="'${REL_PREFIX}'static/|g' \
        -e 's|src="/static/|src="'${REL_PREFIX}'static/|g' \
        -e 's|href="/github\.com/|href="'${REL_PREFIX}'github.com/|g' \
        -e 's|src="/github\.com/|src="'${REL_PREFIX}'github.com/|g' \
        -e 's|?version=||g' \
        -e 's|@v[0-9][^/"]*/|/|g' \
        -e 's|@v[0-9][^"]*"|"|g' \
        "$f" || true
done

# Inject package documentation into main page (pkgsite doesn't show it for package main)
echo "Injecting package documentation into main page..."
MAIN_INDEX="docs/_build/html/backend/godoc/github.com/zfogg/sidechain/backend/index.html"
if [ -f "$MAIN_INDEX" ]; then
    # Use Python to inject the HTML properly
    python3 << 'PYTHON_SCRIPT'
import re

main_index = "docs/_build/html/backend/godoc/github.com/zfogg/sidechain/backend/index.html"

package_doc = '''  <div class="UnitReadme UnitReadme--expanded js-readme" style="margin-bottom: 2rem;">
    <h2 class="UnitReadme-title" id="section-package">
      <img class="go-Icon" height="24" width="24" src="../../../../static/shared/icon/code_gm_grey_24dp.svg" alt="">
      Package Documentation
      <a class="UnitReadme-idLink" href="#section-package" title="Go to Package Documentation" aria-label="Go to Package Documentation">¶</a>
    </h2>
    <div class="UnitReadme-content" data-test-id="Unit-readmeContent">
      <div class="Overview-readmeContent js-readmeContent">
        <p><strong>Package backend</strong> provides the Sidechain API server.</p>
        <p>This package contains the main application entry point. The actual API documentation is organized into subpackages:</p>
        <ul>
          <li><strong><a href="../../../../github.com/zfogg/sidechain/backend/internal/handlers/index.html">internal/handlers</a></strong>: HTTP request handlers for all API endpoints</li>
          <li><strong><a href="../../../../github.com/zfogg/sidechain/backend/internal/models/index.html">internal/models</a></strong>: Data models and database schemas</li>
          <li><strong><a href="../../../../github.com/zfogg/sidechain/backend/internal/auth/index.html">internal/auth</a></strong>: Authentication and authorization services</li>
          <li><strong><a href="../../../../github.com/zfogg/sidechain/backend/internal/stream/index.html">internal/stream</a></strong>: Stream.io integration for social features</li>
          <li><strong><a href="../../../../github.com/zfogg/sidechain/backend/internal/websocket/index.html">internal/websocket</a></strong>: WebSocket server for real-time updates</li>
          <li><strong><a href="../../../../github.com/zfogg/sidechain/backend/internal/audio/index.html">internal/audio</a></strong>: Audio processing and transcoding</li>
          <li><strong><a href="../../../../github.com/zfogg/sidechain/backend/internal/storage/index.html">internal/storage</a></strong>: File storage (S3) operations</li>
          <li><strong><a href="../../../../github.com/zfogg/sidechain/backend/internal/database/index.html">internal/database</a></strong>: Database connection and migrations</li>
          <li><strong><a href="../../../../github.com/zfogg/sidechain/backend/internal/email/index.html">internal/email</a></strong>: Email service integration</li>
          <li><strong><a href="../../../../github.com/zfogg/sidechain/backend/internal/middleware/index.html">internal/middleware</a></strong>: HTTP middleware (rate limiting, etc.)</li>
          <li><strong><a href="../../../../github.com/zfogg/sidechain/backend/internal/queue/index.html">internal/queue</a></strong>: Background job processing</li>
          <li><strong><a href="../../../../github.com/zfogg/sidechain/backend/internal/recommendations/index.html">internal/recommendations</a></strong>: Recommendation engine integration</li>
          <li><strong><a href="../../../../github.com/zfogg/sidechain/backend/internal/search/index.html">internal/search</a></strong>: Search functionality</li>
          <li><strong><a href="../../../../github.com/zfogg/sidechain/backend/internal/stories/index.html">internal/stories</a></strong>: Story management</li>
        </ul>
        <p>See the individual package documentation for detailed API reference.</p>
      </div>
    </div>
  </div>'''

with open(main_index, 'r') as f:
    content = f.read()

# Find the README section and insert package doc before it
pattern = r'(<div class="UnitDetails-content[^>]*>)\s*(<div class="UnitReadme UnitReadme--expanded js-readme">)'
replacement = r'\1\n' + package_doc + r'\n\2'

content = re.sub(pattern, replacement, content, count=1)

with open(main_index, 'w') as f:
    f.write(content)

print("✓ Injected package documentation into main page")
PYTHON_SCRIPT
fi

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
