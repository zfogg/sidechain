#pragma once

#include <JuceHeader.h>
#include "FeedPost.h"

//==============================================================================
/**
 * FeedResponse represents a paginated response from the feed API
 */
struct FeedResponse
{
    juce::Array<FeedPost> posts;
    int limit = 20;
    int offset = 0;
    int total = 0;
    bool hasMore = false;
    juce::String error;        // Non-empty if there was an error
};
