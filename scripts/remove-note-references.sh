#!/bin/bash
# Script to remove note/task references from code comments ONLY
# Patterns: Phase X.Y, R.X.Y, ENG-X, Feature #X, numeric patterns like 5.5.2, etc.

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${YELLOW}Removing note references from code comments...${NC}"

# Find all Go and C++ files
FILES=$(find backend plugin/src -type f \( -name "*.go" -o -name "*.cpp" -o -name "*.h" \) ! -path "*/vendor/*" ! -path "*/build/*" ! -path "*/node_modules/*")

TOTAL_CHANGES=0

for file in $FILES; do
    if [ ! -f "$file" ]; then
        continue
    fi
    
    # Create backup
    cp "$file" "$file.bak"
    
    # Use Python for more precise comment-only replacement
    python3 <<PYTHON_SCRIPT
import re
import sys

def clean_comments(line):
    """Remove note references from comments only"""
    original_line = line
    
    # Skip macro definitions and preprocessor directives
    stripped = line.strip()
    if stripped.startswith('#define') or stripped.startswith('#include') or \
       stripped.startswith('#if') or stripped.startswith('#endif') or \
       stripped.startswith('#else') or stripped.startswith('#elif') or \
       '##' in line or '#' in stripped[:10]:  # Skip if # is in first 10 chars (likely macro)
        return line
    
    # Check if line contains a comment
    if '//' not in line and (not line.strip().startswith('#') and '#' not in line):
        return line
    
    # For Go/C++ files: handle // comments
    if '//' in line:
        # Split on //, but preserve the // itself
        comment_pos = line.find('//')
        code_part = line[:comment_pos]
        comment_part = line[comment_pos + 2:]  # Everything after //
        
        # Skip if this looks like a URL (http:// or https://)
        if 'http://' in code_part or 'https://' in code_part:
            return line
        
        # Clean comment part
        if comment_part:
            original_comment = comment_part
            
            # Remove (Phase X.Y) or (Phase X.Y.Z)
            comment_part = re.sub(r'\(Phase\s+\d+\.\d+(?:\.\d+)?(?:\.\d+)?[^)]*\)', '', comment_part)
            comment_part = re.sub(r'\bPhase\s+\d+\.\d+(?:\.\d+)?(?:\.\d+)?\b', '', comment_part)
            
            # Remove (R.X.Y) or (R.X.Y.Z)
            comment_part = re.sub(r'\(R\.\d+\.\d+(?:\.\d+)?(?:\.\d+)?[^)]*\)', '', comment_part)
            comment_part = re.sub(r'\bR\.\d+\.\d+(?:\.\d+)?(?:\.\d+)?\b', '', comment_part)
            
            # Remove (ENG-X)
            comment_part = re.sub(r'\(ENG-\d+\)', '', comment_part)
            comment_part = re.sub(r'\bENG-\d+\b', '', comment_part)
            
            # Remove (Feature #X)
            comment_part = re.sub(r'\(Feature\s+#\d+\)', '', comment_part)
            comment_part = re.sub(r'\bFeature\s+#\d+\b', '', comment_part)
            
            # Remove numeric patterns like (5.5.2) or (7.5.1.3.3) - but be careful
            # Only if it looks like a reference pattern (not a version number)
            comment_part = re.sub(r'\((\d+\.\d+\.\d+(?:\.\d+)?)\)', r'()', comment_part)
            
            # Clean up TODO/FIXME with Phase references
            comment_part = re.sub(r'TODO:\s*Phase\s+\d+\.\d+(?:\.\d+)?(?:\.\d+)?\s*-', 'TODO:', comment_part)
            comment_part = re.sub(r'FIXME:\s*Phase\s+\d+\.\d+(?:\.\d+)?(?:\.\d+)?\s*-', 'FIXME:', comment_part)
            
            # Clean up empty parentheses and extra spaces
            comment_part = comment_part.replace('()', '')
            comment_part = re.sub(r'  +', ' ', comment_part)  # Only collapse multiple spaces, not single spaces
            comment_part = comment_part.strip()
            
            # Reconstruct line - preserve trailing newline if original had it
            if comment_part:
                return code_part + '// ' + comment_part + ('\n' if original_line.endswith('\n') else '')
            else:
                return code_part.rstrip() + ('\n' if original_line.endswith('\n') else '')
    
    # For config files: handle # comments (only if not at start of line)
    elif '#' in line and not line.strip().startswith('#'):
        # Inline comment
        comment_pos = line.find('#')
        code_part = line[:comment_pos]
        comment_part = line[comment_pos + 1:]  # Everything after #
        
        if comment_part:
            # Apply same cleaning
            comment_part = re.sub(r'\(Phase\s+\d+\.\d+(?:\.\d+)?(?:\.\d+)?[^)]*\)', '', comment_part)
            comment_part = re.sub(r'\bPhase\s+\d+\.\d+(?:\.\d+)?(?:\.\d+)?\b', '', comment_part)
            comment_part = re.sub(r'\(R\.\d+\.\d+(?:\.\d+)?(?:\.\d+)?[^)]*\)', '', comment_part)
            comment_part = re.sub(r'\bR\.\d+\.\d+(?:\.\d+)?(?:\.\d+)?\b', '', comment_part)
            comment_part = re.sub(r'\(ENG-\d+\)', '', comment_part)
            comment_part = re.sub(r'\bENG-\d+\b', '', comment_part)
            comment_part = re.sub(r'\(Feature\s+#\d+\)', '', comment_part)
            comment_part = re.sub(r'\bFeature\s+#\d+\b', '', comment_part)
            comment_part = re.sub(r'\((\d+\.\d+\.\d+(?:\.\d+)?)\)', r'()', comment_part)
            comment_part = comment_part.replace('()', '')
            comment_part = re.sub(r'\s+', ' ', comment_part)
            comment_part = comment_part.strip()
            
            if comment_part:
                return code_part + '# ' + comment_part + ('\n' if original_line.endswith('\n') else '')
            else:
                return code_part.rstrip() + ('\n' if original_line.endswith('\n') else '')
    
    return line

# Read file
with open('$file', 'r', encoding='utf-8', errors='ignore') as f:
    lines = f.readlines()

# Process lines
new_lines = []
for line in lines:
    new_line = clean_comments(line)
    new_lines.append(new_line)

# Write back
with open('$file', 'w', encoding='utf-8') as f:
    f.writelines(new_lines)
PYTHON_SCRIPT
    
    # Check if file changed
    if ! cmp -s "$file" "$file.bak"; then
        echo -e "${GREEN}Modified: $file${NC}"
        TOTAL_CHANGES=$((TOTAL_CHANGES + 1))
    fi
    
    # Remove backup
    rm "$file.bak"
done

echo -e "${GREEN}Done! Modified $TOTAL_CHANGES files${NC}"
echo -e "${YELLOW}Please review changes and test compilation.${NC}"
