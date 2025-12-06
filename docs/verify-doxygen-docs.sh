#!/bin/bash
# Verify that Doxygen comments are present in the HTML documentation

set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$SCRIPT_DIR"

echo "=========================================="
echo "  Verifying Doxygen Documentation"
echo "=========================================="
echo ""

# Find HTML output directory
HTML_DIR=""
if [ -d "build-docs/html" ]; then
    HTML_DIR="build-docs/html"
elif [ -d "_build/html" ]; then
    HTML_DIR="_build/html"
else
    echo "❌ ERROR: No HTML output found. Please build the docs first:"
    echo "   ./rebuild-docs.sh"
    exit 1
fi

echo "Checking HTML files in: $HTML_DIR"
echo ""

# Check for Doxygen-generated API pages
API_FILES=$(find "$HTML_DIR" -name "*.html" -path "*/api/*" | head -5)

if [ -z "$API_FILES" ]; then
    echo "❌ ERROR: No API HTML files found in $HTML_DIR/api/"
    echo "   The documentation may not have been built correctly."
    exit 1
fi

echo "✓ Found API HTML files"
echo ""

# Check a specific class file for Doxygen content
AUTH_FILE=$(find "$HTML_DIR" -name "*Auth*.html" -path "*/api/*" | head -1)

if [ -z "$AUTH_FILE" ]; then
    echo "⚠️  WARNING: Could not find Auth class HTML file"
    echo "   This might be because the class was renamed or docs need rebuilding"
else
    echo "Checking: $(basename $AUTH_FILE)"

    # Check for common Doxygen content indicators
    if grep -q "Called when login" "$AUTH_FILE" 2>/dev/null; then
        echo "✓ Found Doxygen comment: 'Called when login'"
    else
        echo "❌ Missing Doxygen comment: 'Called when login'"
    fi

    if grep -q "@param" "$AUTH_FILE" 2>/dev/null || grep -q "Pointer to the NetworkClient" "$AUTH_FILE" 2>/dev/null; then
        echo "✓ Found parameter documentation"
    else
        echo "❌ Missing parameter documentation"
    fi

    if grep -q "briefdescription\|detaileddescription" "$AUTH_FILE" 2>/dev/null; then
        echo "✓ Found Doxygen description tags"
    else
        echo "⚠️  No Doxygen description tags found (might be rendered differently)"
    fi
fi

echo ""
echo "=========================================="
echo "  Checking Doxygen XML"
echo "=========================================="
echo ""

# Check Doxygen XML
XML_DIR=""
if [ -d "build-docs/doxygen/xml" ]; then
    XML_DIR="build-docs/doxygen/xml"
elif [ -d "doxygen/xml" ]; then
    XML_DIR="doxygen/xml"
fi

if [ -z "$XML_DIR" ]; then
    echo "❌ ERROR: No Doxygen XML directory found"
    echo "   Please run Doxygen first"
    exit 1
fi

echo "Checking XML files in: $XML_DIR"
echo ""

# Check if XML has comments
AUTH_XML=$(find "$XML_DIR" -name "*Auth*.xml" | head -1)

if [ -z "$AUTH_XML" ]; then
    echo "⚠️  WARNING: Could not find Auth XML file"
else
    echo "Checking: $(basename $AUTH_XML)"

    # Check if briefdescription has content (not just empty tags)
    if grep -A 2 "<briefdescription>" "$AUTH_XML" | grep -q -v "^[[:space:]]*<" | grep -q "."; then
        echo "✓ XML contains brief descriptions"
    else
        echo "❌ XML brief descriptions are empty"
        echo "   Doxygen is not extracting comments from source files"
    fi

    # Check for specific comment content
    if grep -q "Called when login" "$AUTH_XML" 2>/dev/null; then
        echo "✓ XML contains 'Called when login' comment"
    else
        echo "❌ XML missing 'Called when login' comment"
    fi

    # Check if file references are correct (should be Auth.h, not AuthComponent.h)
    if grep -q "Auth\.h" "$AUTH_XML" 2>/dev/null; then
        echo "✓ XML references correct file (Auth.h)"
    elif grep -q "AuthComponent\.h" "$AUTH_XML" 2>/dev/null; then
        echo "⚠️  XML still references old file (AuthComponent.h)"
        echo "   Documentation needs to be rebuilt after renaming"
    fi
fi

echo ""
echo "=========================================="
echo "  Summary"
echo "=========================================="
echo ""
echo "If comments are missing, try:"
echo "  1. Rebuild documentation: ./rebuild-docs.sh"
echo "  2. Check source files have /** ... */ comments"
echo "  3. Verify Doxygen is parsing the correct input directory"
echo ""
