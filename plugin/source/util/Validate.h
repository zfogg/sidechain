#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
 * Validate - Input validation and sanitization utilities
 *
 * Centralizes validation logic that was previously scattered across the codebase.
 * All functions are stateless and thread-safe.
 *
 * Usage:
 *   if (Validate::isEmail(email)) { ... }
 *   auto cleanUsername = Validate::sanitizeUsername(input);
 */
namespace Validate
{
    //==========================================================================
    // String Format Validation

    /**
     * Check if string is a valid email address.
     * Uses a practical regex that catches most invalid emails without being overly strict.
     */
    bool isEmail(const juce::String& str);

    /**
     * Check if string is a valid URL (http:// or https://).
     */
    bool isUrl(const juce::String& str);

    /**
     * Check if string is a valid username.
     * Rules: alphanumeric + underscore, 3-30 characters, starts with letter.
     */
    bool isUsername(const juce::String& str);

    /**
     * Check if string is a valid display name.
     * Rules: 1-50 characters, no control characters.
     */
    bool isDisplayName(const juce::String& str);

    /**
     * Check if string looks like a valid UUID.
     */
    bool isUuid(const juce::String& str);

    //==========================================================================
    // Range Validation

    /**
     * Check if integer is within range [min, max] (inclusive).
     */
    bool inRange(int val, int min, int max);

    /**
     * Check if float is within range [min, max] (inclusive).
     */
    bool inRange(float val, float min, float max);

    /**
     * Check if string length is within range [min, max] (inclusive).
     */
    bool lengthInRange(const juce::String& str, int minLen, int maxLen);

    //==========================================================================
    // Content Validation

    /**
     * Check if string is empty or contains only whitespace.
     */
    bool isBlank(const juce::String& str);

    /**
     * Check if string is not empty and not just whitespace.
     */
    bool isNotBlank(const juce::String& str);

    /**
     * Check if string contains only alphanumeric characters.
     */
    bool isAlphanumeric(const juce::String& str);

    /**
     * Check if string contains only digits.
     */
    bool isNumeric(const juce::String& str);

    /**
     * Check if string is valid JSON.
     */
    bool isValidJson(const juce::String& str);

    //==========================================================================
    // Audio/Music Validation

    /**
     * Check if BPM is in valid range (20-300).
     */
    bool isValidBpm(float bpm);

    /**
     * Check if musical key is valid (e.g., "C", "Am", "F#m", "Bb").
     */
    bool isValidKey(const juce::String& key);

    /**
     * Check if duration is valid (positive, reasonable for a loop: 0.1s - 300s).
     */
    bool isValidDuration(float seconds);

    //==========================================================================
    // Sanitization

    /**
     * Sanitize username: lowercase, remove invalid chars, truncate.
     * Returns empty string if input can't be made valid.
     */
    juce::String sanitizeUsername(const juce::String& input);

    /**
     * Sanitize display name: trim whitespace, normalize spaces, truncate.
     */
    juce::String sanitizeDisplayName(const juce::String& input);

    /**
     * Escape HTML special characters to prevent XSS.
     * Converts: & < > " ' to HTML entities.
     */
    juce::String escapeHtml(const juce::String& input);

    /**
     * Remove all HTML tags from string.
     */
    juce::String stripHtml(const juce::String& input);

    /**
     * Trim and collapse multiple whitespace to single spaces.
     */
    juce::String normalizeWhitespace(const juce::String& input);

    /**
     * Truncate string to max length, optionally adding ellipsis.
     */
    juce::String truncate(const juce::String& input, int maxLength, bool addEllipsis = true);

}  // namespace Validate
