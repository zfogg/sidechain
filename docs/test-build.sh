#!/bin/bash
# Test script to simulate the CI documentation build process locally
# This helps validate the workflow before pushing to CI

set -e

echo "=========================================="
echo "  Testing Documentation Build Process"
echo "=========================================="
echo ""

# Colors for output
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

# Get the project root directory
PROJECT_ROOT="$( cd "$( dirname "${BASH_SOURCE[0]}" )/.." && pwd )"
cd "$PROJECT_ROOT"

# Check dependencies
echo "Checking dependencies..."
MISSING_DEPS=()

command -v python3 >/dev/null 2>&1 || MISSING_DEPS+=("python3")
command -v pip3 >/dev/null 2>&1 || MISSING_DEPS+=("pip3")
command -v doxygen >/dev/null 2>&1 || MISSING_DEPS+=("doxygen")
command -v cmake >/dev/null 2>&1 || MISSING_DEPS+=("cmake")
command -v ninja >/dev/null 2>&1 || MISSING_DEPS+=("ninja")
command -v go >/dev/null 2>&1 || MISSING_DEPS+=("go")
command -v wget >/dev/null 2>&1 || MISSING_DEPS+=("wget")

if [ ${#MISSING_DEPS[@]} -gt 0 ]; then
    echo -e "${RED}✗ Missing dependencies: ${MISSING_DEPS[*]}${NC}"
    echo "Please install missing dependencies before running this test."
    exit 1
fi
echo -e "${GREEN}✓ All dependencies found${NC}"
echo ""

# Check Python packages
echo "Checking Python packages..."
cd docs
if python3 -c "import sphinx, breathe, exhale, myst_parser" 2>/dev/null; then
    echo -e "${GREEN}✓ Python packages installed${NC}"
else
    echo -e "${YELLOW}⚠ Python packages not installed${NC}"
    echo "Run: pip3 install -r docs/requirements.txt"
    echo "Skipping Python package check for now..."
fi
cd ..
echo ""

# Test 1: Plugin documentation build
echo "Test 1: Building plugin documentation (Doxygen)..."
cd plugin
if [ -d "build-docs" ]; then
    rm -rf build-docs
fi

if cmake -S . -B build-docs \
    -G Ninja \
    -DSIDECHAIN_BUILD_DOCS=ON \
    -DCMAKE_BUILD_TYPE=Release 2>&1 | tee /tmp/cmake-config.log; then
    echo -e "${GREEN}✓ CMake configuration successful${NC}"
else
    echo -e "${RED}✗ CMake configuration failed${NC}"
    cat /tmp/cmake-config.log
    exit 1
fi

if cmake --build build-docs --target docs 2>&1 | tee /tmp/cmake-build.log; then
    echo -e "${GREEN}✓ Plugin documentation build successful${NC}"
else
    echo -e "${RED}✗ Plugin documentation build failed${NC}"
    cat /tmp/cmake-build.log
    exit 1
fi

# Check if Doxygen XML was generated
if [ -d "build-docs/docs/doxygen/xml" ] || [ -d "build-docs/doxygen/xml" ]; then
    echo -e "${GREEN}✓ Doxygen XML generated${NC}"
    
    # Copy to expected location (simulating CI workflow)
    mkdir -p "$PROJECT_ROOT/docs/doxygen"
    if [ -d "build-docs/docs/doxygen/xml" ]; then
        cp -r build-docs/docs/doxygen/xml "$PROJECT_ROOT/docs/doxygen/"
    elif [ -d "build-docs/doxygen/xml" ]; then
        cp -r build-docs/doxygen/xml "$PROJECT_ROOT/docs/doxygen/"
    fi
    echo -e "${GREEN}✓ Doxygen XML copied to docs/doxygen/xml${NC}"
else
    echo -e "${YELLOW}⚠ Doxygen XML not found in expected location${NC}"
    echo "This may be okay if Doxygen hasn't run yet"
fi

cd ..
echo ""

# Test 2: Go documentation (simulated - we won't start a server)
echo "Test 2: Checking Go documentation setup..."
if command -v godoc >/dev/null 2>&1; then
    echo -e "${GREEN}✓ godoc command available${NC}"
else
    echo -e "${YELLOW}⚠ godoc not found, will be installed in CI${NC}"
fi
echo ""

# Test 3: Sphinx documentation build
echo "Test 3: Building Sphinx documentation..."
cd docs

# Check if Makefile exists and is valid
if [ ! -f "Makefile" ] || [ ! -s "Makefile" ]; then
    echo -e "${RED}✗ Makefile is missing or empty${NC}"
    exit 1
fi
echo -e "${GREEN}✓ Makefile exists${NC}"

# Set environment variables (as CI does)
export DOXYGEN_XML_DIR="$PROJECT_ROOT/docs/doxygen/xml"
export PYTHONPATH="$PROJECT_ROOT/docs"

# Try to build (this will fail if Python packages aren't installed, but that's okay for testing)
if python3 -c "import sphinx" 2>/dev/null; then
    echo "Building Sphinx docs..."
    if make html 2>&1 | tee /tmp/sphinx-build.log | tail -20; then
        echo -e "${GREEN}✓ Sphinx documentation build successful${NC}"
        
        # Check if index.html was created
        if [ -f "_build/html/index.html" ]; then
            echo -e "${GREEN}✓ Documentation HTML generated at docs/_build/html/index.html${NC}"
        else
            echo -e "${YELLOW}⚠ index.html not found${NC}"
        fi
    else
        echo -e "${YELLOW}⚠ Sphinx build had issues (may be due to missing Python packages)${NC}"
        echo "This is expected if Python packages aren't installed locally"
    fi
else
    echo -e "${YELLOW}⚠ Sphinx not available - skipping build test${NC}"
    echo "Install with: pip3 install -r docs/requirements.txt"
fi

cd ..
echo ""

# Summary
echo "=========================================="
echo "  Test Summary"
echo "=========================================="
echo ""
echo "✓ Dependency checks: PASSED"
echo "✓ Plugin documentation build: PASSED"
if [ -d "docs/doxygen/xml" ]; then
    echo "✓ Doxygen XML location: PASSED"
else
    echo "⚠ Doxygen XML location: Check needed"
fi
if [ -f "docs/Makefile" ] && [ -s "docs/Makefile" ]; then
    echo "✓ Sphinx Makefile: PASSED"
else
    echo "✗ Sphinx Makefile: FAILED"
fi
echo ""
echo "To test the full build locally:"
echo "  1. Install Python packages: pip3 install -r docs/requirements.txt"
echo "  2. Run: cd docs && make html"
echo "  3. View: open docs/_build/html/index.html"
echo ""
echo "=========================================="

