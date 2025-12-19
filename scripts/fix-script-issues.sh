#!/bin/bash
# Fix issues introduced by the note removal script
# - Fix broken URLs (http://  -> http://)
# - Fix broken hex colors (#  -> #)
# - Fix incomplete comment removals (// : -> //)
# - Fix comment separators (// ===== -> //======)

set -e

echo "Fixing script issues..."

FILES=$(find backend plugin/src -type f \( -name "*.go" -o -name "*.cpp" -o -name "*.h" \) ! -path "*/vendor/*" ! -path "*/build/*")

for file in $FILES; do
    if [ ! -f "$file" ]; then
        continue
    fi
    
    # Fix broken URLs (need to match space after ://)
    sed -i 's|"http:// |"http://|g' "$file"
    sed -i "s|'http:// |'http://|g" "$file"
    sed -i 's|"https:// |"https://|g' "$file"
    sed -i "s|'https:// |'https://|g" "$file"
    sed -i 's|http:// |http://|g' "$file"
    sed -i 's|https:// |https://|g' "$file"
    
    # Fix broken hex colors in strings
    sed -i 's|# \([0-9a-fA-F]\{6\}\)|#\1|g' "$file"
    sed -i 's|# \([0-9a-fA-F]\{3\}\)|#\1|g' "$file"
    
    # Fix incomplete comment removals
    sed -i 's|^\([[:space:]]*\)// : |\1// |g' "$file"
    sed -i 's|// : |// |g' "$file"
    
    # Fix comment separators (optional - keep as is if user prefers)
    # sed -i 's|// =====|//======|g' "$file"
done

echo "Done fixing script issues."
