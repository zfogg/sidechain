#pragma once

#include <JuceHeader.h>

namespace Sidechain {
// ==============================================================================
/**
 * TimeUtils provides utility functions for time formatting
 */
namespace TimeUtils {
/**
 * Format a timestamp as a human-readable "time ago" string
 *
 * Examples:
 * - "just now" (< 60 seconds)
 * - "1 min ago", "5 mins ago"
 * - "1 hour ago", "3 hours ago"
 * - "1 day ago", "5 days ago"
 * - "1 week ago", "2 weeks ago"
 * - "1 month ago", "6 months ago"
 * - "1 year ago", "2 years ago"
 *
 * @param time The timestamp to format
 * @return Human-readable relative time string
 */
juce::String formatTimeAgo(const juce::Time &time);

/**
 * Format a timestamp as a short "time ago" string (for compact UI)
 *
 * Examples:
 * - "now" (< 60 seconds)
 * - "1m", "5m"
 * - "1h", "3h"
 * - "1d", "5d"
 * - "1w", "2w"
 * - "Jan 15" (for older dates)
 *
 * @param time The timestamp to format
 * @return Short relative time string
 */
juce::String formatTimeAgoShort(const juce::Time &time);
} // namespace TimeUtils
} // namespace Sidechain
