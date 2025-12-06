# Doxygen Performance Optimization

## Problem

Doxygen was taking a long time to generate documentation because it was:
1. Processing JUCE headers (via `#include <JuceHeader.h>`)
2. Following includes recursively
3. Extracting documentation for all symbols, including external libraries

## Solution

The `Doxyfile` has been optimized to exclude ALL dependencies:

### 1. Exclude All Dependencies and External Libraries
```
EXCLUDE_PATTERNS =
  - */tests/*, */test/*
  - */_deps/*, */deps/*, */dependencies/*
  - */vendor/*, */third_party/*, */external/*
  - */build/*, */build-*/*, */cmake-build-*/*
  - */JUCE/*, */juce/*, *JuceHeader*, */juce_*/*
  - */modules/*, */libs/*, */lib/*, */include/*
  - */submodules/*, */.git/*, */CMakeFiles/*
  - */generated/*, */gen/*, */out/*, */output/*

EXCLUDE_SYMBOLS =
  - juce::*, std::*, ::juce::*, ::std::*
  - boost::*, fmt::*, spdlog::* (and other common libraries)
```

### 2. Explicit Directory Exclusions
```
EXCLUDE =
  - ../plugin/modules
  - ../plugin/libs
  - ../plugin/vendor
  - ../plugin/third_party
  - ../plugin/external
  - ../plugin/deps
  - ../plugin/_deps
```

### 2. Disable Include Following
```
SEARCH_INCLUDES = NO
EXPAND_ONLY_PREDEF = YES
```

This prevents Doxygen from following includes outside your project directory.

### 3. Only Extract Documented Items
```
EXTRACT_ALL = NO
HIDE_UNDOC_MEMBERS = YES
HIDE_UNDOC_CLASSES = YES
```

This means Doxygen will only document items that have Doxygen comments, making it faster and more focused.

### 4. Predefined Macros
Added common JUCE macros to avoid preprocessing issues:
```
PREDEFINED = JUCE_DISPLAY_SPLASH_SCREEN=0, ...
```

## Performance Impact

**Before**: Doxygen was processing potentially thousands of JUCE header files
**After**: Doxygen only processes your 111 source files in `plugin/source/`

Expected speedup: **10-100x faster** depending on JUCE installation size

## What's Still Documented

- ✅ All your classes, functions, and members with Doxygen comments
- ✅ Public API documentation
- ✅ Your custom types and structures

## What's Excluded

- ❌ **All dependencies**: JUCE, boost, fmt, spdlog, and any other external libraries
- ❌ **All vendor/third-party code**: Anything in vendor/, third_party/, external/, deps/, etc.
- ❌ **Build artifacts**: All build directories, generated files, CMakeFiles
- ❌ **Test files**: All test directories and files
- ❌ **System libraries**: Standard library (std::*), system headers
- ❌ **Undocumented items**: Only documented code appears in the docs

## Rebuilding

After these changes, rebuild the docs:

```bash
cd docs
rm -rf build-docs doxygen api _build
./rebuild-docs.sh
```

The build should now complete much faster!
