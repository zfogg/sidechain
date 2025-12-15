#!/usr/bin/env bash
# release-tag.sh - Create version tags for project components
#
# Creates lightweight tags for individual components or an annotated tag for main releases
#
# Usage:
#   ./scripts/release-tag.sh                          # Create main release tag
#   ./scripts/release-tag.sh plugin                   # Create plugin component tag
#   ./scripts/release-tag.sh backend --patch          # Create backend tag with patch bump
#   ./scripts/release-tag.sh web --minor              # Create web tag with minor bump
#   ./scripts/release-tag.sh --name "First Release"   # Create main tag with release name
#
# Options:
#   --major                                           # Increment major version
#   --minor                                           # Increment minor version
#   --patch                                           # Increment patch version (default)
#   --name "Release Name"                            # Release name for annotated tag

set -euo pipefail

COMPONENT=""
VERSION_ARG="--next-patch"  # Default to patch increment
RELEASE_NAME=""
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Parse arguments
while [[ $# -gt 0 ]]; do
    case "$1" in
        plugin|backend|web|cli)
            COMPONENT="$1"
            shift
            ;;
        --major)
            VERSION_ARG="--next-major"
            shift
            ;;
        --minor)
            VERSION_ARG="--next-minor"
            shift
            ;;
        --patch)
            VERSION_ARG="--next-patch"
            shift
            ;;
        --name)
            shift
            RELEASE_NAME="$1"
            shift
            ;;
        -h|--help)
            echo "Usage: $0 [COMPONENT] [OPTIONS]"
            echo ""
            echo "Components (optional):"
            echo "  plugin              Create plugin component tag"
            echo "  backend             Create backend component tag"
            echo "  web                 Create web component tag"
            echo "  cli                 Create CLI component tag"
            echo "  (none)              Create main project release tag"
            echo ""
            echo "Options:"
            echo "  --major             Increment major version"
            echo "  --minor             Increment minor version"
            echo "  --patch             Increment patch version (default)"
            echo "  --name \"Name\"       Release name (used in annotated tag)"
            echo ""
            echo "Examples:"
            echo "  $0                              # Create v0.0.1 tag"
            echo "  $0 --name \"First Release\"       # Create v0.0.1 with release name"
            echo "  $0 plugin                       # Create plugin/v0.0.1 tag"
            echo "  $0 backend --minor              # Create backend/v0.1.0 tag"
            echo "  $0 web --major --name \"v1\"     # Create web/v1.0.0 tag with name"
            exit 0
            ;;
        *)
            echo "Unknown option: $1" >&2
            exit 1
            ;;
    esac
done

# Get next version based on component
get_next_version() {
    local comp="$1"
    if [[ -z "$comp" ]]; then
        "$SCRIPT_DIR/version.sh" "$VERSION_ARG"
    else
        "$SCRIPT_DIR/version.sh" "$comp" "$VERSION_ARG"
    fi
}

# Create tag
create_tag() {
    local tag="$1"
    local message="$2"

    # Check if tag already exists
    if git rev-parse "$tag" >/dev/null 2>&1; then
        echo "Error: Tag '$tag' already exists" >&2
        exit 1
    fi

    if [[ -z "$message" ]]; then
        # Lightweight tag
        git tag "$tag"
        echo "Created tag: $tag"
    else
        # Annotated signed tag (requires GPG key configured)
        git tag -a "$tag" -m "$message" -s
        echo "Created signed annotated tag: $tag"
        echo "Message: $message"
    fi
}

# Main logic
if [[ -z "$COMPONENT" ]]; then
    # Creating main project release tag
    NEXT_VERSION=$(get_next_version "")
    TAG="v${NEXT_VERSION}"

    # Get all component versions
    PLUGIN_VERSION=$("$SCRIPT_DIR/version.sh" plugin)
    BACKEND_VERSION=$("$SCRIPT_DIR/version.sh" backend)
    WEB_VERSION=$("$SCRIPT_DIR/version.sh" web)
    CLI_VERSION=$("$SCRIPT_DIR/version.sh" cli)

    # Build annotation message
    if [[ -n "$RELEASE_NAME" ]]; then
        MESSAGE="Release \"$RELEASE_NAME\""$'\n\n'"backend/v${BACKEND_VERSION}"$'\n'"plugin/v${PLUGIN_VERSION}"
        if [[ "$WEB_VERSION" != "0.0.0" ]]; then
            MESSAGE="${MESSAGE}"$'\n'"web/v${WEB_VERSION}"
        fi
        if [[ "$CLI_VERSION" != "0.0.0" ]]; then
            MESSAGE="${MESSAGE}"$'\n'"cli/v${CLI_VERSION}"
        fi
    else
        MESSAGE="Release ${NEXT_VERSION}"$'\n\n'"backend/v${BACKEND_VERSION}"$'\n'"plugin/v${PLUGIN_VERSION}"
        if [[ "$WEB_VERSION" != "0.0.0" ]]; then
            MESSAGE="${MESSAGE}"$'\n'"web/v${WEB_VERSION}"
        fi
        if [[ "$CLI_VERSION" != "0.0.0" ]]; then
            MESSAGE="${MESSAGE}"$'\n'"cli/v${CLI_VERSION}"
        fi
    fi

    create_tag "$TAG" "$MESSAGE"
else
    # Creating component tag
    NEXT_VERSION=$(get_next_version "$COMPONENT")
    TAG="${COMPONENT}/v${NEXT_VERSION}"
    create_tag "$TAG" ""
fi

echo "✅ Tag created successfully"
