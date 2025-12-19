#include "StringUtils.h"

namespace StringUtils {

juce::String getInitials(const juce::String &displayName, int maxChars) {
  if (displayName.isEmpty())
    return "?";

  auto trimmedName = displayName.trim();
  auto words = juce::StringArray::fromTokens(trimmedName, " ", "");

  if (words.isEmpty())
    return "?";

  // Two or more words: take first letter of first and last word
  // "Mary Sue Wallace" -> "MW", "Leo Van Dorn" -> "LD"
  if (words.size() >= 2) {
    juce::String initials = words[0].substring(0, 1).toUpperCase();
    if (initials.length() < maxChars) {
      initials += words[words.size() - 1].substring(0, 1).toUpperCase();
    }
    return initials;
  }

  // Single word: take first N letters (default 2)
  // "alice" -> "AL", "bob" -> "BO"
  juce::String initials = words[0].substring(0, maxChars).toUpperCase();
  return initials.isEmpty() ? "?" : initials;
}

} // namespace StringUtils
