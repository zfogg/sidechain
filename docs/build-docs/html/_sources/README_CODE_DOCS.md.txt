# Integrating Code Documentation into Sphinx

## Quick Overview

Your Sphinx documentation website can automatically include API documentation from your C++ code by using:

**Linkify Test**: URLs like https://www.sphinx-doc.org and https://breathe.readthedocs.io should be automatically converted to clickable links when linkify is enabled!

1. **Doxygen** - Extracts documentation from code comments
2. **Breathe** - Bridge between Doxygen XML and Sphinx
3. **Exhale** - Auto-generates API documentation hierarchy

## How It Works (Simple Version)

```
Your C++ Code with Comments
    ↓
[Doxygen] → Reads your code, extracts /** comments */
    ↓
XML Files (structured data about your API)
    ↓
[Breathe + Exhale] → Convert XML to Sphinx format
    ↓
[Sphinx] → Combines with your .rst files
    ↓
Final HTML Website with Code Docs!
```

## Current Setup

✅ Everything is already configured in `conf.py`:
- `breathe` extension - reads Doxygen XML
- `exhale` extension - auto-generates API hierarchy
- Paths are configured correctly

✅ Your code already has Doxygen comments (like `@brief`, `@param`)

✅ Doxygen, Breathe, and Exhale are installed

## Building the Documentation

### Option 1: Using CMake (Easiest)

```bash
cd docs
cmake -S . -B build-docs
cmake --build build-docs --target docs
```

Output will be in `docs/build-docs/html/index.html`

### Option 2: Manual Build

```bash
cd docs

# Step 1: Generate Doxygen XML
doxygen Doxyfile
# This creates: docs/doxygen/xml/ directory

# Step 2: Build Sphinx (Breathe/Exhale run automatically)
sphinx-build -b html . _build/html

# Step 3: Open in browser
open _build/html/index.html
```

## Where to Find Code Docs

Once built, your API documentation will appear:

- **In the sidebar**: Look for "API Reference" link
- **Direct URL**: `/api/library_root.html`
- **Auto-generated**: Exhale creates a tree structure of all classes, functions, namespaces

## Writing Doxygen Comments

Example from your codebase:

```cpp
/**
 * BufferAudioPlayer - Handles audio playback from in-memory AudioBuffer
 *
 * This class is designed for playing audio directly from juce::AudioBuffer<float>
 * without needing to encode/decode or save to files.
 */
class BufferAudioPlayer {
    /**
     * Load audio buffer for playback
     * @param buffer The audio buffer to play (copied internally)
     * @param sampleRate The sample rate of the buffer
     */
    void loadBuffer(const juce::AudioBuffer<float>& buffer, double sampleRate);
};
```

Key tags:
- `@brief` - Short description (first line of comment)
- `@param name` - Parameter description
- `@return` - Return value description
- `@class`, `@namespace` - For classes/namespaces

## Configuration Files

- **`Doxyfile`** - Configures Doxygen (what files to scan, output format)
- **`conf.py`** - Sphinx config (already has Breathe/Exhale set up)
- **`docs/CMakeLists.txt`** - Build automation

## Troubleshooting

**"Doxygen XML not found"**:
- Run `doxygen Doxyfile` first
- Check that `docs/doxygen/xml/index.xml` exists

**API docs not showing**:
- Check Sphinx build logs for errors
- Verify Exhale generated files in `api/` directory
- Make sure your code has Doxygen comments

**Need to rebuild after code changes**:
- Just rebuild! The pipeline re-scans everything automatically

## Next Steps

1. Build the docs (use Option 1 or 2 above)
2. Open the HTML output and navigate to "API Reference"
3. Explore your documented classes and functions
4. As you add more Doxygen comments to your code, rebuild and they'll appear automatically!

The integration is seamless - your code comments automatically become part of your documentation website.
