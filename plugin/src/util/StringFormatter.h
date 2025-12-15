#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
 * StringFormatter - Number, duration, and display string formatting utilities
 *
 * Centralizes formatting logic for counts, durations, percentages, and
 * music-specific values like BPM.
 *
 * Usage:
 *   auto countStr = StringFormatter::formatCount(1234);      // "1.2K"
 *   auto duration = StringFormatter::formatDuration(90.5);   // "1:30"
 *   auto percent = StringFormatter::formatPercentage(0.75);  // "75%"
 */
namespace StringFormatter {
//==========================================================================
// Count Formatting (K, M, B suffixes)

/**
 * Format a count with K/M/B suffix for large numbers.
 * Examples: 999 → "999", 1234 → "1.2K", 1500000 → "1.5M"
 */
juce::String formatCount(int value);

/**
 * Format a count with specified decimal places.
 */
juce::String formatCount(int value, int decimals);

/**
 * Format a large number with appropriate suffix.
 */
juce::String formatLargeNumber(int64_t value);

//==========================================================================
// Duration Formatting

/**
 * Format seconds as MM:SS or H:MM:SS if >= 1 hour.
 * Examples: 90 → "1:30", 3661 → "1:01:01"
 */
juce::String formatDuration(double seconds);

/**
 * Format seconds as MM:SS only (no hours).
 * Examples: 90 → "1:30", 3661 → "61:01"
 */
juce::String formatDurationMMSS(double seconds);

/**
 * Format seconds as H:MM:SS always.
 * Examples: 90 → "0:01:30", 3661 → "1:01:01"
 */
juce::String formatDurationHMS(double seconds);

/**
 * Format milliseconds as seconds with decimal.
 * Examples: 1500 → "1.5s"
 */
juce::String formatMilliseconds(int ms);

//==========================================================================
// Percentage Formatting

/**
 * Format a 0.0-1.0 value as percentage.
 * Examples: 0.75 → "75%", 0.333 → "33%"
 */
juce::String formatPercentage(float value);

/**
 * Format a percentage with specified decimal places.
 * Examples: formatPercentage(0.755, 1) → "75.5%"
 */
juce::String formatPercentage(float value, int decimals);

//==========================================================================
// Music-specific Formatting

/**
 * Format BPM value.
 * Examples: 120.0 → "120 BPM", 120.5 → "120.5 BPM"
 */
juce::String formatBPM(double bpm);

/**
 * Format BPM without suffix.
 * Examples: 120.0 → "120", 120.5 → "120.5"
 */
juce::String formatBPMValue(double bpm);

/**
 * Format confidence value (0.0-1.0) as percentage.
 * Examples: 0.92 → "92%"
 */
juce::String formatConfidence(float confidence);

/**
 * Format a musical key.
 * Examples: "Cm" → "C minor", "F#" → "F# major"
 */
juce::String formatKeyLong(const juce::String &key);

//==========================================================================
// Social/Engagement Formatting

/**
 * Format follower count with appropriate suffix.
 * Examples: 150 → "150 followers", 1500 → "1.5K followers"
 */
juce::String formatFollowers(int count);

/**
 * Format like count.
 * Examples: 42 → "42 likes", 1200 → "1.2K likes"
 */
juce::String formatLikes(int count);

/**
 * Format comment count.
 */
juce::String formatComments(int count);

/**
 * Format play count.
 * Examples: 42 → "42 plays", 1200 → "1.2K plays"
 */
juce::String formatPlays(int count);

//==========================================================================
// Time Ago Formatting

/**
 * Format a timestamp as relative time.
 * Examples: "2m ago", "3h ago", "5d ago", "2w ago"
 */
juce::String formatTimeAgo(const juce::Time &timestamp);

/**
 * Format a timestamp as relative time from ISO string.
 */
juce::String formatTimeAgo(const juce::String &isoTimestamp);

} // namespace StringFormatter
