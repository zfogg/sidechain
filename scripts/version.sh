#!/usr/bin/env bash
# version.sh - Print and calculate version numbers from git tags
#
# Supports component-based versioning: plugin/vX.Y.Z, backend/vX.Y.Z, etc.
# Also supports main project versioning: vX.Y.Z
#
# Usage:
#   ./scripts/version.sh                    # Print main project version (e.g., 0.3.57)
#   ./scripts/version.sh [plugin|backend|web|cli]  # Print component version
#   ./scripts/version.sh --major            # Print major component
#   ./scripts/version.sh --minor            # Print minor component
#   ./scripts/version.sh --patch            # Print patch component
#   ./scripts/version.sh --next             # Print next patch version
#   ./scripts/version.sh --next-major       # Increment major
#   ./scripts/version.sh --next-minor       # Increment minor
#   ./scripts/version.sh --next-patch       # Increment patch
#   ./scripts/version.sh plugin --next      # Get next version for plugin component

set -euo pipefail

COMPONENT=""
SHOW_MAJOR=false
SHOW_MINOR=false
SHOW_PATCH=false
NEXT_MAJOR=false
NEXT_MINOR=false
NEXT_PATCH=false

# Parse arguments
while [[ $# -gt 0 ]]; do
    case "$1" in
        plugin|backend|web|cli)
            COMPONENT="$1"
            shift
            ;;
        --major)
            SHOW_MAJOR=true
            shift
            ;;
        --minor)
            SHOW_MINOR=true
            shift
            ;;
        --patch)
            SHOW_PATCH=true
            shift
            ;;
        --next)
            NEXT_PATCH=true
            shift
            ;;
        --next-major)
            NEXT_MAJOR=true
            shift
            ;;
        --next-minor)
            NEXT_MINOR=true
            shift
            ;;
        --next-patch)
            NEXT_PATCH=true
            shift
            ;;
        -h|--help)
            echo "Usage: $0 [COMPONENT] [OPTIONS]"
            echo ""
            echo "Components (optional):"
            echo "  plugin              Get plugin version"
            echo "  backend             Get backend version"
            echo "  web                 Get web version"
            echo "  cli                 Get CLI version"
            echo "  (none)              Get main project version (default)"
            echo ""
            echo "Options:"
            echo "  --major             Print major component (e.g., 0)"
            echo "  --minor             Print minor component (e.g., 3)"
            echo "  --patch             Print patch component (e.g., 57)"
            echo "  --next              Print next patch version (e.g., 0.3.58)"
            echo "  --next-major        Increment major (e.g., 1.0.0)"
            echo "  --next-minor        Increment minor (e.g., 0.4.0)"
            echo "  --next-patch        Increment patch (e.g., 0.3.58)"
            echo ""
            echo "Examples:"
            echo "  $0                          # Main project version (0.3.57)"
            echo "  $0 plugin                   # Plugin version (0.0.1)"
            echo "  $0 backend --patch          # Backend patch version (1)"
            echo "  $0 web --next               # Next web version (0.0.2)"
            echo "  $0 --next-major             # Next main project version (1.0.0)"
            exit 0
            ;;
        *)
            echo "Unknown option: $1" >&2
            exit 1
            ;;
    esac
done

# Get current version from git describe
# Strips the 'v' prefix if present
get_current_version() {
    local pattern
    if [[ -z "$COMPONENT" ]]; then
        # Main project version: v[0-9]*.[0-9]*.[0-9]*
        pattern="v[0-9]*.[0-9]*.[0-9]*"
    else
        # Component version: COMPONENT/v[0-9]*.[0-9]*.[0-9]*
        pattern="${COMPONENT}/v[0-9]*.[0-9]*.[0-9]*"
    fi

    local version
    version=$(git tag --list "$pattern" --sort=-v:refname --merged HEAD 2>/dev/null | head -1 || echo "")

    if [[ -z "$version" ]]; then
        echo "0.0.0"
    else
        # Remove component prefix if present (e.g., "plugin/v0.0.1" -> "0.0.1")
        version="${version#*/v}"
        version="${version#v}"
        echo "$version"
    fi
}

# Parse version into components
parse_version() {
    local version="$1"
    IFS='.' read -r MAJOR MINOR PATCH <<< "$version"
    # Handle versions with extra components (e.g., 0.3.57-rc1)
    PATCH="${PATCH%%-*}"
}

# Main logic
CURRENT_VERSION=$(get_current_version)
parse_version "$CURRENT_VERSION"

# Handle component queries
if [[ "$SHOW_MAJOR" == "true" ]]; then
    echo "$MAJOR"
    exit 0
fi

if [[ "$SHOW_MINOR" == "true" ]]; then
    echo "$MINOR"
    exit 0
fi

if [[ "$SHOW_PATCH" == "true" ]]; then
    echo "$PATCH"
    exit 0
fi

# Handle next version calculations
if [[ "$NEXT_MAJOR" == "true" ]]; then
    MAJOR=$((MAJOR + 1))
    MINOR=0
    PATCH=0
    echo "${MAJOR}.${MINOR}.${PATCH}"
    exit 0
fi

if [[ "$NEXT_MINOR" == "true" ]]; then
    MINOR=$((MINOR + 1))
    PATCH=0
    echo "${MAJOR}.${MINOR}.${PATCH}"
    exit 0
fi

if [[ "$NEXT_PATCH" == "true" ]]; then
    PATCH=$((PATCH + 1))
    echo "${MAJOR}.${MINOR}.${PATCH}"
    exit 0
fi

# Default: print current version
echo "$CURRENT_VERSION"
