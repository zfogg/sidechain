#include "FileCache.h"
#include "../Log.h"

// ==============================================================================
// Explicit template instantiations
// ==============================================================================

// For URL-based caching (used by AudioCache and ImageCache)
template class FileCache<juce::String>;
