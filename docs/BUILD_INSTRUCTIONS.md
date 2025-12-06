# Building Documentation with Code Docs

## Quick Start

### Option 1: Using CMake (Recommended)

The easiest way - CMake automates everything:

```bash
cd docs
cmake -S . -B build-docs
cmake --build build-docs --target docs
```

This will:
1. Run Doxygen to extract code documentation → generates XML
2. Run Exhale to create API hierarchy → generates .rst files in `api/`
3. Run Sphinx to build HTML → final website

Output: `docs/build-docs/html/index.html`

### Open Documentation in Browser

After building, you can automatically open the documentation in your default browser:

```bash
cmake --build build-docs --target docs-open
```

This works cross-platform:
- **macOS**: Uses `open` command
- **Linux**: Uses `xdg-open` command  
- **Windows**: Uses `start` command

The `docs-open` target will automatically build the docs first if needed (it depends on the `docs` target).

### Option 2: Manual Build (For Debugging)

If you want to see each step:

```bash
cd docs

# Step 1: Generate Doxygen XML
doxygen Doxyfile
# Creates: docs/doxygen/xml/ directory with all the XML files

# Step 2: Build Sphinx (Breathe/Exhale run automatically)
sphinx-build -b html . _build/html

# Step 3: View it
open _build/html/index.html  # macOS
```

## After Building

1. Open `_build/html/index.html` (or `build-docs/html/index.html` if using CMake)
2. Look for "API Reference" in the sidebar or table of contents
3. Click through to see your documented classes, functions, namespaces!

## What Gets Generated

- **Doxygen XML**: `doxygen/xml/` - Structured data about your API
- **Exhale API files**: `api/` directory - Auto-generated .rst files organizing your API
- **Final HTML**: `_build/html/` or `build-docs/html/` - Complete website

## Troubleshooting

**"Doxygen XML not found" error:**
- Make sure you ran `doxygen Doxyfile` first
- Check that `docs/doxygen/xml/index.xml` exists

**API docs empty or not showing:**
- Check Sphinx build output for errors
- Verify your code has Doxygen comments (like `@brief`, `@param`)
- Make sure Exhale generated files in the `api/` directory

**Need to rebuild after code changes:**
- Just rebuild! The pipeline automatically re-scans everything

## Integration with Main Plugin Build

If you build the plugin with docs enabled:

```bash
cd plugin
cmake -S . -B build -DSIDECHAIN_BUILD_DOCS=ON
cmake --build build --target docs
```

The docs will be built alongside the plugin.
