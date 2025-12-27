#include "Validate.h"
#include <regex>

namespace Validate {

// ==============================================================================
// String Format Validation

bool isEmail(const juce::String &str) {
  if (str.isEmpty())
    return false;

  // Practical email regex - not RFC 5322 compliant but catches most issues
  // Allows: letters, digits, dots, hyphens, underscores before @
  // Requires: @ followed by domain with at least one dot
  static const std::regex emailRegex(R"([a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,})", std::regex::optimize);

  return std::regex_match(str.toStdString(), emailRegex);
}

bool isUrl(const juce::String &str) {
  if (str.isEmpty())
    return false;

  // Check for http://or https://prefix
  if (!str.startsWithIgnoreCase("http://") && !str.startsWithIgnoreCase("https://"))
    return false;

  // Basic URL structure validation
  static const std::regex urlRegex(R"(https?:// [a-zA-Z0-9][-a-zA-Z0-9]*(\.[a-zA-Z0-9][-a-zA-Z0-9]*)+(:[0-9]+)?(/.*)?)",
                                   std::regex::optimize);

  return std::regex_match(str.toStdString(), urlRegex);
}

bool isUsername(const juce::String &str) {
  if (str.isEmpty())
    return false;

  int len = str.length();
  if (len < 3 || len > 30)
    return false;

  // Must start with a letter
  if (!juce::CharacterFunctions::isLetter(str[0]))
    return false;

  // Rest must be alphanumeric or underscore
  for (int i = 0; i < len; ++i) {
    juce::juce_wchar c = str[i];
    if (!juce::CharacterFunctions::isLetterOrDigit(c) && c != '_')
      return false;
  }

  return true;
}

bool isDisplayName(const juce::String &str) {
  if (str.isEmpty())
    return false;

  int len = str.trim().length();
  if (len < 1 || len > 50)
    return false;

  // No control characters
  for (int i = 0; i < str.length(); ++i) {
    juce::juce_wchar c = str[i];
    if (c < 32 && c != '\t') // Allow printable chars and tab
      return false;
  }

  return true;
}

bool isUuid(const juce::String &str) {
  if (str.length() != 36)
    return false;

  // UUID format: 8-4-4-4-12 hex digits
  static const std::regex uuidRegex(R"([0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12})",
                                    std::regex::optimize);

  return std::regex_match(str.toStdString(), uuidRegex);
}

// ==============================================================================
// Range Validation

bool inRange(int val, int min, int max) {
  return val >= min && val <= max;
}

bool inRange(float val, float min, float max) {
  return val >= min && val <= max;
}

bool lengthInRange(const juce::String &str, int minLen, int maxLen) {
  int len = str.length();
  return len >= minLen && len <= maxLen;
}

// ==============================================================================
// Content Validation

bool isBlank(const juce::String &str) {
  return str.trim().isEmpty();
}

bool isNotBlank(const juce::String &str) {
  return !isBlank(str);
}

bool isAlphanumeric(const juce::String &str) {
  if (str.isEmpty())
    return false;

  for (int i = 0; i < str.length(); ++i) {
    if (!juce::CharacterFunctions::isLetterOrDigit(str[i]))
      return false;
  }

  return true;
}

bool isNumeric(const juce::String &str) {
  if (str.isEmpty())
    return false;

  for (int i = 0; i < str.length(); ++i) {
    if (!juce::CharacterFunctions::isDigit(str[i]))
      return false;
  }

  return true;
}

bool isValidJson(const juce::String &str) {
  if (str.isEmpty())
    return false;

  auto result = juce::JSON::parse(str);
  // JSON::parse returns undefined var on failure
  return !result.isUndefined();
}

// ==============================================================================
// Audio/Music Validation

bool isValidBpm(float bpm) {
  return bpm >= 20.0f && bpm <= 300.0f;
}

bool isValidKey(const juce::String &key) {
  if (key.isEmpty() || key.length() > 4)
    return false;

  // Valid keys: C, C#, Db, D, D#, Eb, E, F, F#, Gb, G, G#, Ab, A, A#, Bb, B
  // Optionally followed by 'm' for minor
  static const juce::StringArray validKeys = {
      "C",  "C#",  "Db",  "D",  "D#",  "Eb",  "E",  "F",  "F#",  "Gb",  "G",  "G#",  "Ab",  "A",  "A#",  "Bb",  "B",
      "Cm", "C#m", "Dbm", "Dm", "D#m", "Ebm", "Em", "Fm", "F#m", "Gbm", "Gm", "G#m", "Abm", "Am", "A#m", "Bbm", "Bm"};

  return validKeys.contains(key);
}

bool isValidDuration(float seconds) {
  return seconds >= 0.1f && seconds <= 300.0f;
}

// ==============================================================================
// Sanitization

juce::String sanitizeUsername(const juce::String &input) {
  if (input.isEmpty())
    return {};

  juce::String result;
  result.preallocateBytes(static_cast<size_t>(input.length()));

  for (int i = 0; i < input.length() && result.length() < 30; ++i) {
    juce::juce_wchar c = input[i];

    // Convert to lowercase
    if (juce::CharacterFunctions::isUpperCase(c))
      c = juce::CharacterFunctions::toLowerCase(c);

    // Only keep alphanumeric and underscore
    if (juce::CharacterFunctions::isLetterOrDigit(c) || c == '_') {
      // First char must be a letter
      if (result.isEmpty() && !juce::CharacterFunctions::isLetter(c))
        continue;

      result += c;
    }
  }

  // Must be at least 3 chars
  if (result.length() < 3)
    return {};

  return result;
}

juce::String sanitizeDisplayName(const juce::String &input) {
  if (input.isEmpty())
    return {};

  // Normalize whitespace and trim
  juce::String result = normalizeWhitespace(input);

  // Remove control characters
  juce::String cleaned;
  cleaned.preallocateBytes(static_cast<size_t>(result.length()));

  for (int i = 0; i < result.length(); ++i) {
    juce::juce_wchar c = result[i];
    if (c >= 32 || c == '\t')
      cleaned += c;
  }

  // Truncate to 50 chars
  if (cleaned.length() > 50)
    cleaned = cleaned.substring(0, 50);

  return cleaned.trim();
}

juce::String escapeHtml(const juce::String &input) {
  if (input.isEmpty())
    return {};

  juce::String result;
  result.preallocateBytes(static_cast<size_t>(input.length() * 2));

  for (int i = 0; i < input.length(); ++i) {
    juce::juce_wchar c = input[i];

    switch (c) {
    case '&':
      result += "&amp;";
      break;
    case '<':
      result += "&lt;";
      break;
    case '>':
      result += "&gt;";
      break;
    case '"':
      result += "&quot;";
      break;
    case '\'':
      result += "&#39;";
      break;
    default:
      result += c;
      break;
    }
  }

  return result;
}

juce::String stripHtml(const juce::String &input) {
  if (input.isEmpty())
    return {};

  juce::String result;
  result.preallocateBytes(static_cast<size_t>(input.length()));

  bool inTag = false;

  for (int i = 0; i < input.length(); ++i) {
    juce::juce_wchar c = input[i];

    if (c == '<') {
      inTag = true;
    } else if (c == '>') {
      inTag = false;
    } else if (!inTag) {
      result += c;
    }
  }

  return result;
}

juce::String normalizeWhitespace(const juce::String &input) {
  if (input.isEmpty())
    return {};

  juce::String result;
  result.preallocateBytes(static_cast<size_t>(input.length()));

  bool lastWasSpace = true; // Start true to trim leading

  for (int i = 0; i < input.length(); ++i) {
    juce::juce_wchar c = input[i];

    if (juce::CharacterFunctions::isWhitespace(c)) {
      if (!lastWasSpace) {
        result += ' ';
        lastWasSpace = true;
      }
    } else {
      result += c;
      lastWasSpace = false;
    }
  }

  // Trim trailing space
  if (result.isNotEmpty() && result.getLastCharacter() == ' ')
    result = result.dropLastCharacters(1);

  return result;
}

juce::String truncate(const juce::String &input, int maxLength, bool addEllipsis) {
  if (input.isEmpty() || maxLength <= 0)
    return {};

  if (input.length() <= maxLength)
    return input;

  if (addEllipsis && maxLength > 3) {
    return input.substring(0, maxLength - 3) + "...";
  }

  return input.substring(0, maxLength);
}

} // namespace Validate
