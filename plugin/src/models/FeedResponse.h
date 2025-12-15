#pragma once

#include "FeedPost.h"
#include <JuceHeader.h>

//==============================================================================
/**
 * FeedResponse represents a paginated response from the feed API
 *
 * Contains an array of posts along with pagination metadata
 */
struct FeedResponse {
  juce::Array<FeedPost> posts; ///< Array of feed posts
  int limit = 20;              ///< Number of posts requested per page
  int offset = 0;              ///< Pagination offset (starting position)
  int total = 0;               ///< Total number of posts available
  bool hasMore = false;        ///< Whether more posts are available
  juce::String error;          ///< Non-empty if there was an error
};
