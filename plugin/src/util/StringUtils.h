#pragma once

#include <JuceHeader.h>

// ==============================================================================
/**
 * String manipulation utilities.
 */
namespace StringUtils {

/**
 * Generate initials from a display name.
 *
 * For names with 2+ words: takes first letter of first and last word.
 * Examples:
 * - "Mary Sue Wallace" -> "MW"
 * - "Leo Van Dorn" -> "LD"
 *
 * For single-word names: takes first N letters (default 2).
 * Examples:
 * - "alice" -> "AL"
 * - "bob" -> "BO"
 * - "?" for empty name
 *
 * @param displayName The display name to extract initials from
 * @param maxChars Maximum number of characters to use (default 2)
 * @return The generated initials, uppercased
 */
juce::String getInitials(const juce::String &displayName, int maxChars = 2);

} // namespace StringUtils
