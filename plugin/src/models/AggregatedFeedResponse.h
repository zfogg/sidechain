#pragma once

#include "AggregatedFeedGroup.h"
#include <JuceHeader.h>

//==============================================================================
/**
 * AggregatedFeedResponse represents a paginated response from an aggregated
 * feed API
 *
 * Contains groups of activities that have been aggregated by getstream.io
 * based on the configured aggregation format (e.g., by user+action+day).
 */
struct AggregatedFeedResponse {
  juce::Array<AggregatedFeedGroup> groups; ///< Array of aggregated activity groups
  int limit = 20;                          ///< Number of groups requested per page
  int offset = 0;                          ///< Pagination offset (starting position)
  int total = 0;                           ///< Total number of groups available
  bool hasMore = false;                    ///< Whether more groups are available
  juce::String error;                      ///< Non-empty if there was an error
};
