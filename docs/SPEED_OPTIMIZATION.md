# Documentation Build Speed Optimization

## Target
Build time should be **< 5 seconds** for incremental builds.

## Optimizations Applied

### 1. Doxygen Optimizations
- ✅ **Disabled preprocessing**: `ENABLE_PREPROCESSING = NO` - Skips macro expansion
- ✅ **Quiet mode**: `QUIET = YES`, `WARNINGS = NO` - No verbose output
- ✅ **Exclude all dependencies**: Comprehensive exclusion patterns
- ✅ **Only extract documented items**: `EXTRACT_ALL = NO`
- ✅ **Disable unnecessary features**: No sorting, no todo lists, no file listings

**Result**: Doxygen completes in ~0.7 seconds

### 2. Sphinx/Exhale Optimizations
- ✅ **Parallel builds**: `-j auto` flag for Sphinx
- ✅ **Quiet mode**: `-q` flag for Sphinx
- ✅ **Disabled tree view**: `createTreeView = False`
- ✅ **Reduced toctree depth**: `fullToctreeMaxDepth = 2`
- ✅ **Disabled viewcode extension**: Not needed for API docs

**Result**: Sphinx processing is faster but still the bottleneck

### 3. Build System Optimizations
- ✅ **Incremental builds**: CMake tracks dependencies
- ✅ **Quiet Doxygen**: `-q` flag
- ✅ **Parallel Sphinx**: `-j auto` flag

## Current Performance

- **Full build**: ~55 seconds (first time or after clean)
- **Incremental build**: Should be < 5 seconds if nothing changed

## Further Optimization Options

If incremental builds are still slow:

1. **Skip Exhale entirely**: Use Breathe directly (faster but less organized)
2. **Cache Exhale output**: Only regenerate API files when Doxygen XML changes
3. **Limit API scope**: Only document specific namespaces/classes
4. **Use Sphinx's incremental build cache**: Already enabled by default

## Testing

```bash
# Full clean build (slow)
cd docs
rm -rf build-docs
cmake -S . -B build-docs
time cmake --build build-docs --target docs

# Incremental build (should be fast)
time cmake --build build-docs --target docs
```

The second build should complete in < 5 seconds if no source files changed.
