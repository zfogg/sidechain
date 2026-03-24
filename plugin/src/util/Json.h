#pragma once

#include <JuceHeader.h>

// ==============================================================================
/**
 * Json - Type-safe JSON property access utilities
 *
 * @deprecated This entire namespace is deprecated. Use nlohmann::json with
 * typed models instead. See SerializableModel and JsonValidation.h for the
 * recommended approach.
 *
 * Provides concise, null-safe accessors for juce::var JSON objects.
 * Replaces verbose patterns like:
 *   json.getProperty("name", "").toString()
 * With:
 *   Json::getString(json, "name")
 *
 * All functions gracefully handle:
 * - Missing keys (returns default value)
 * - Null/void values (returns default value)
 * - Type mismatches (returns default value)
 */
namespace Json {
// ==========================================================================
// Primitive type accessors

/** Get string value, returns defaultVal if key missing or not a string
 *  @deprecated Use nlohmann::json with typed models instead
 */
[[deprecated("Use nlohmann::json with typed models instead")]]
juce::String getString(const juce::var &json, const char *key, const juce::String &defaultVal = {});

/** Get integer value, returns defaultVal if key missing or not numeric
 *  @deprecated Use nlohmann::json with typed models instead
 */
[[deprecated("Use nlohmann::json with typed models instead")]]
int getInt(const juce::var &json, const char *key, int defaultVal = 0);

/** Get int64 value for large integers
 *  @deprecated Use nlohmann::json with typed models instead
 */
[[deprecated("Use nlohmann::json with typed models instead")]]
juce::int64 getInt64(const juce::var &json, const char *key, juce::int64 defaultVal = 0);

/** Get float value, returns defaultVal if key missing or not numeric
 *  @deprecated Use nlohmann::json with typed models instead
 */
[[deprecated("Use nlohmann::json with typed models instead")]]
float getFloat(const juce::var &json, const char *key, float defaultVal = 0.0f);

/** Get double value, returns defaultVal if key missing or not numeric
 *  @deprecated Use nlohmann::json with typed models instead
 */
[[deprecated("Use nlohmann::json with typed models instead")]]
double getDouble(const juce::var &json, const char *key, double defaultVal = 0.0);

/** Get boolean value, returns defaultVal if key missing
 *  @deprecated Use nlohmann::json with typed models instead
 */
[[deprecated("Use nlohmann::json with typed models instead")]]
bool getBool(const juce::var &json, const char *key, bool defaultVal = false);

// ==========================================================================
// Complex type accessors

/** Get nested object, returns void var if key missing or not an object
 *  @deprecated Use nlohmann::json with typed models instead
 */
[[deprecated("Use nlohmann::json with typed models instead")]]
juce::var getObject(const juce::var &json, const char *key);

/** Get array, returns void var if key missing or not an array
 *  @deprecated Use nlohmann::json with typed models instead
 */
[[deprecated("Use nlohmann::json with typed models instead")]]
juce::var getArray(const juce::var &json, const char *key);

// ==========================================================================
// Array element accessors

/** Get string from array at index
 *  @deprecated Use nlohmann::json with typed models instead
 */
[[deprecated("Use nlohmann::json with typed models instead")]]
juce::String getStringAt(const juce::var &array, int index, const juce::String &defaultVal = {});

/** Get int from array at index
 *  @deprecated Use nlohmann::json with typed models instead
 */
[[deprecated("Use nlohmann::json with typed models instead")]]
int getIntAt(const juce::var &array, int index, int defaultVal = 0);

/** Get object from array at index
 *  @deprecated Use nlohmann::json with typed models instead
 */
[[deprecated("Use nlohmann::json with typed models instead")]]
juce::var getObjectAt(const juce::var &array, int index);

// ==========================================================================
// Utility functions

/** Check if json has a key (and it's not void/undefined)
 *  @deprecated Use nlohmann::json with typed models instead
 */
[[deprecated("Use nlohmann::json with typed models instead")]]
bool hasKey(const juce::var &json, const char *key);

/** Check if value is an array
 *  @deprecated Use nlohmann::json with typed models instead
 */
[[deprecated("Use nlohmann::json with typed models instead")]]
bool isArray(const juce::var &value);

/** Check if value is an object (DynamicObject)
 *  @deprecated Use nlohmann::json with typed models instead
 */
[[deprecated("Use nlohmann::json with typed models instead")]]
bool isObject(const juce::var &value);

/** Get array size (returns 0 if not an array)
 *  @deprecated Use nlohmann::json with typed models instead
 */
[[deprecated("Use nlohmann::json with typed models instead")]]
int arraySize(const juce::var &array);

} // namespace Json
