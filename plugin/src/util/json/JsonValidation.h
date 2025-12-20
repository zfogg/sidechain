#pragma once

#include <JuceHeader.h>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>

namespace Sidechain {
namespace Json {

// ==============================================================================
/**
 * ValidationError - JSON validation exception with detailed error context
 *
 * Thrown when JSON parsing fails due to missing required fields, type mismatches,
 * or malformed data. Includes the field name, reason, and JSON context for debugging.
 */
class ValidationError : public std::runtime_error {
public:
  ValidationError(const std::string &field, const std::string &reason, const std::string &context = "")
      : std::runtime_error("JSON validation failed for field '" + field + "': " + reason +
                           (context.empty() ? "" : " (context: " + context + ")")) {}
};

// ==============================================================================
// Required field validation

/**
 * Require a field to exist and have the correct type
 *
 * @tparam T The expected type
 * @param j JSON object to read from
 * @param field Field name to read
 * @return Value of type T
 * @throws ValidationError if field is missing or has wrong type
 */
template <typename T> T require(const nlohmann::json &j, const std::string &field) {
  if (!j.contains(field)) {
    throw ValidationError(field, "required field is missing", j.dump());
  }
  try {
    return j.at(field).get<T>();
  } catch (const nlohmann::json::exception &e) {
    throw ValidationError(field, "type mismatch - " + std::string(e.what()), j.dump());
  }
}

// ==============================================================================
// Optional field with default value

/**
 * Get an optional field with a default value
 *
 * @tparam T The expected type
 * @param j JSON object to read from
 * @param field Field name to read
 * @param defaultValue Value to return if field is missing or null
 * @return Value of type T or default
 * @throws ValidationError if field exists but has wrong type
 */
template <typename T> T optional(const nlohmann::json &j, const std::string &field, const T &defaultValue) {
  if (!j.contains(field) || j.at(field).is_null()) {
    return defaultValue;
  }
  try {
    return j.at(field).get<T>();
  } catch (const nlohmann::json::exception &e) {
    throw ValidationError(field, "type mismatch - " + std::string(e.what()), j.dump());
  }
}

// ==============================================================================
// String conversion utilities

/**
 * Convert std::string to juce::String
 */
inline juce::String toJuceString(const std::string &str) {
  return juce::String(str);
}

/**
 * Convert juce::String to std::string
 */
inline std::string fromJuceString(const juce::String &str) {
  return str.toStdString();
}

// ==============================================================================
// Custom macro for models with validation

/**
 * SIDECHAIN_JSON_TYPE - Generate JSON serialization methods for a type
 *
 * Usage:
 *   struct MyModel {
 *     juce::String id;
 *     int count;
 *
 *     SIDECHAIN_JSON_TYPE(MyModel)
 *   };
 *
 *   // Then implement to_json and from_json:
 *   void to_json(nlohmann::json& j, const MyModel& m) { ... }
 *   void from_json(const nlohmann::json& j, MyModel& m) { ... }
 *
 * This generates:
 *   - static MyModel fromJson(const nlohmann::json& j)
 *   - nlohmann::json toJson() const
 *   - Friend declarations for to_json/from_json
 */
#define SIDECHAIN_JSON_TYPE(Type, ...)                                                                                 \
  friend void to_json(nlohmann::json &j, const Type &obj);                                                             \
  friend void from_json(const nlohmann::json &j, Type &obj);                                                           \
  static Type fromJson(const nlohmann::json &j) {                                                                      \
    try {                                                                                                              \
      Type obj;                                                                                                        \
      from_json(j, obj);                                                                                               \
      return obj;                                                                                                      \
    } catch (const Sidechain::Json::ValidationError &e) {                                                              \
      throw;                                                                                                           \
    } catch (const std::exception &e) {                                                                                \
      throw Sidechain::Json::ValidationError("unknown", e.what(), j.dump());                                           \
    }                                                                                                                  \
  }                                                                                                                    \
  nlohmann::json toJson() const {                                                                                      \
    nlohmann::json j;                                                                                                  \
    to_json(j, *this);                                                                                                 \
    return j;                                                                                                          \
  }

} // namespace Json
} // namespace Sidechain

// ==============================================================================
// Helper macros for common patterns in from_json implementations

/**
 * JSON_REQUIRE - Require a field and assign to variable
 *
 * Usage:
 *   int count;
 *   JSON_REQUIRE(json, "count", count);
 */
#define JSON_REQUIRE(json, field, var) var = Sidechain::Json::require<decltype(var)>(json, field)

/**
 * JSON_OPTIONAL - Get optional field with default and assign to variable
 *
 * Usage:
 *   int count;
 *   JSON_OPTIONAL(json, "count", count, 0);
 */
#define JSON_OPTIONAL(json, field, var, defaultVal)                                                                    \
  var = Sidechain::Json::optional<decltype(var)>(json, field, defaultVal)

/**
 * JSON_REQUIRE_STRING - Require a string field and convert to juce::String
 *
 * Usage:
 *   juce::String name;
 *   JSON_REQUIRE_STRING(json, "name", name);
 */
#define JSON_REQUIRE_STRING(json, field, var)                                                                          \
  var = Sidechain::Json::toJuceString(Sidechain::Json::require<std::string>(json, field))

/**
 * JSON_OPTIONAL_STRING - Get optional string field and convert to juce::String
 *
 * Usage:
 *   juce::String bio;
 *   JSON_OPTIONAL_STRING(json, "bio", bio, "");
 */
#define JSON_OPTIONAL_STRING(json, field, var, defaultVal)                                                             \
  var = Sidechain::Json::toJuceString(Sidechain::Json::optional<std::string>(json, field, defaultVal))
