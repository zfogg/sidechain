#!/usr/bin/env python3
"""
Find UTF-8 that causes debugger issues:
1. Log:: calls with non-ASCII
2. juce::String("literal") with UTF-8 (but NOT juce::String(juce::CharPointer_UTF8(...)))

GOOD (keep):
- juce::String(juce::CharPointer_UTF8("\\x...")) - the right way
- g.drawText("♥", ...) - fine

BAD (fix):
- Log::debug("emoji ♥")
- juce::String("literal♥") without CharPointer_UTF8
"""

import os
import re
from pathlib import Path

def has_non_ascii(text):
    """Check if text contains non-ASCII characters."""
    try:
        text.encode('ascii')
        return False
    except UnicodeEncodeError:
        return True

def is_in_comment(line):
    """Check if the line is a comment."""
    stripped = line.strip()
    if stripped.startswith('//'):
        return True
    if stripped.startswith('*'):
        return True
    return False

def find_utf8_in_file(filepath):
    """Find UTF-8 that causes debugger issues."""
    try:
        with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()
            lines = content.split('\n')

        findings = []

        # Pattern 1: Log:: calls with non-ASCII
        log_pattern = r'Log::(debug|info|warn|error)\s*\([^;]+\);'
        for match in re.finditer(log_pattern, content, re.MULTILINE | re.DOTALL):
            if has_non_ascii(match.group(0)):
                line_num = content[:match.start()].count('\n') + 1
                line_content = lines[line_num - 1].strip()

                if is_in_comment(lines[line_num - 1]):
                    continue

                findings.append({
                    'file': filepath,
                    'line': line_num,
                    'type': 'Log with UTF-8',
                    'content': line_content[:100],
                    'match': match.group(0)[:60]
                })

        # Pattern 2: juce::String("literal") with UTF-8 (but NOT CharPointer_UTF8)
        # Look for juce::String( followed by a string literal, not CharPointer_UTF8
        string_pattern = r'juce::String\s*\(\s*"([^"]*)"'
        for match in re.finditer(string_pattern, content):
            string_content = match.group(1)

            # Skip if it's empty or pure ASCII
            if not string_content or not has_non_ascii(string_content):
                continue

            line_num = content[:match.start()].count('\n') + 1
            line_content = lines[line_num - 1].strip()

            if is_in_comment(lines[line_num - 1]):
                continue

            # Check if this is actually juce::String(juce::CharPointer_UTF8(...))
            # by looking backwards in the line
            context_start = max(0, match.start() - 50)
            context = content[context_start:match.end()]
            if 'CharPointer_UTF8' in context:
                continue

            findings.append({
                'file': filepath,
                'line': line_num,
                'type': 'juce::String("utf8")',
                'content': line_content[:100],
                'match': f'"{string_content}"'
            })

        return findings
    except Exception as e:
        print(f"Error reading {filepath}: {e}")
        return []

def main():
    src_dir = Path(__file__).parent / 'src'

    if not src_dir.exists():
        print(f"Directory not found: {src_dir}")
        return

    cpp_files = list(src_dir.rglob('*.cpp'))
    h_files = list(src_dir.rglob('*.h'))
    all_files = cpp_files + h_files

    print(f"Searching {len(all_files)} files for debugger-breaking UTF-8...")
    print("Looking for: Log:: calls with UTF-8, juce::String(\"literal\") with UTF-8")
    print("(juce::String(CharPointer_UTF8) and g.drawText are OK)")
    print("=" * 80)

    all_findings = []
    for filepath in all_files:
        findings = find_utf8_in_file(filepath)
        all_findings.extend(findings)

    # Group by file
    by_file = {}
    for finding in all_findings:
        file = finding['file']
        if file not in by_file:
            by_file[file] = []
        by_file[file].append(finding)

    # Print results
    total_count = 0
    for filepath in sorted(by_file.keys()):
        findings = by_file[filepath]
        rel_path = os.path.relpath(filepath, src_dir.parent)

        print(f"\n{rel_path}")
        print("-" * 80)

        for f in findings:
            print(f"  Line {f['line']:4d} [{f['type']}]")
            print(f"         {f['content']}")

        total_count += len(findings)

    print("\n" + "=" * 80)
    if total_count == 0:
        print("No issues found! All UTF-8 is properly handled.")
    else:
        print(f"Total: {total_count} issues found in {len(by_file)} files")

        print("\nBy type:")
        by_type = {}
        for finding in all_findings:
            t = finding['type']
            by_type[t] = by_type.get(t, 0) + 1

        for t, count in sorted(by_type.items(), key=lambda x: -x[1]):
            print(f"  {t:30s}: {count:3d}")

if __name__ == '__main__':
    main()
