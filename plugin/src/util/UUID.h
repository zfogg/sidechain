#pragma once

#include <juce_core/juce_core.h>
#include <random>
#include <uuid.h>

namespace Sidechain {
namespace Util {

/**
 * UUID Utility Functions
 * Provides UUID generation and manipulation using stduuid library
 */
class UUID {
public:
  /**
   * Generate a new random UUID (v4)
   * @return UUID as juce::String in standard format (8-4-4-4-12)
   */
  static juce::String generate() {
    auto id = uuids::uuid_system_generator{}();
    return juce::String(uuids::to_string(id));
  }

  /**
   * Check if a string is a valid UUID
   * @param str String to validate
   * @return true if valid UUID format
   */
  static bool isValid(const juce::String &str) {
    try {
      auto id = uuids::uuid::from_string(str.toStdString());
      return id.has_value();
    } catch (...) {
      return false;
    }
  }

  /**
   * Generate a new random UUID and return as hex string (no dashes)
   * @return UUID as hex string
   */
  static juce::String generateHex() {
    auto id = uuids::uuid_system_generator{}();
    auto str = uuids::to_string(id);
    // Remove dashes
    std::string hex;
    for (char c : str) {
      if (c != '-') {
        hex += c;
      }
    }
    return juce::String(hex);
  }

private:
  UUID() = delete;
};

} // namespace Util
} // namespace Sidechain
