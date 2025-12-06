# Linkify Test

This document tests the linkify extension to ensure URLs are automatically converted to clickable links.

## Test URLs

Here are some test URLs that should become clickable:

- Visit https://www.example.com for more information
- Check out http://juce.com for JUCE documentation
- Email me at test@example.com (email links work too!)
- GitHub: https://github.com/zfogg/sidechain
- Documentation site: https://docs.example.com/api/v1

## Mixed Content

You can also mix URLs with text: Check out https://sphinx-doc.org for Sphinx documentation, and https://myst.tools for MyST parser info.

URLs at end of sentences: See https://python.org. Periods should be handled correctly.

Parentheses: (See https://example.com for details)

Square brackets: [Check https://example.com]

## Multiple URLs

Visit https://example1.com, https://example2.com, and https://example3.com for more resources.

## Code Blocks (should NOT be linked)

```
Visit https://example.com in code - this should NOT be a link
```

## Links in Lists

1. First item: https://example.com/1
2. Second item: https://example.com/2
3. Third item: https://example.com/3
