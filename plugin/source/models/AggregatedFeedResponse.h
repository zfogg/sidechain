#pragma once

#include <JuceHeader.h>
#include "AggregatedFeedGroup.h"

//==============================================================================
/**
 * AggregatedFeedResponse represents a paginated response from an aggregated feed API
 */
struct AggregatedFeedResponse
{
    juce::Array<AggregatedFeedGroup> groups;
    int limit = 20;
    int offset = 0;
    int total = 0;
    bool hasMore = false;
    juce::String error;
};
