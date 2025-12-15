#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
 * Json - Type-safe JSON property access utilities
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
//==========================================================================
// Primitive type accessors

/** Get string value, returns defaultVal if key missing or not a string
 *  @param json JSON object to read from
 *  @param key Property key name
 *  @param defaultVal Default value if key is missing or invalid
 *  @return String value or default
 */
juce::String getString(const juce::var &json, const char *key, const juce::String &defaultVal = {});

/** Get integer value, returns defaultVal if key missing or not numeric
 *  @param json JSON object to read from
 *  @param key Property key name
 *  @param defaultVal Default value if key is missing or invalid
 *  @return Integer value or default
 */
int getInt(const juce::var &json, const char *key, int defaultVal = 0);

/** Get int64 value for large integers
 *  @param json JSON object to read from
 *  @param key Property key name
 *  @param defaultVal Default value if key is missing or invalid
 *  @return Int64 value or default
 */
juce::int64 getInt64(const juce::var &json, const char *key, juce::int64 defaultVal = 0);

/** Get float value, returns defaultVal if key missing or not numeric
 *  @param json JSON object to read from
 *  @param key Property key name
 *  @param defaultVal Default value if key is missing or invalid
 *  @return Float value or default
 */
float getFloat(const juce::var &json, const char *key, float defaultVal = 0.0f);

/** Get double value, returns defaultVal if key missing or not numeric
 *  @param json JSON object to read from
 *  @param key Property key name
 *  @param defaultVal Default value if key is missing or invalid
 *  @return Double value or default
 */
double getDouble(const juce::var &json, const char *key, double defaultVal = 0.0);

/** Get boolean value, returns defaultVal if key missing
 *  @param json JSON object to read from
 *  @param key Property key name
 *  @param defaultVal Default value if key is missing
 *  @return Boolean value or default
 */
bool getBool(const juce::var &json, const char *key, bool defaultVal = false);

//==========================================================================
// Complex type accessors

/** Get nested object, returns void var if key missing or not an object
 *  @param json JSON object to read from
 *  @param key Property key name
 *  @return Nested object or void var if missing/invalid
 */
juce::var getObject(const juce::var &json, const char *key);

/** Get array, returns void var if key missing or not an array
 *  @param json JSON object to read from
 *  @param key Property key name
 *  @return Array or void var if missing/invalid
 */
juce::var getArray(const juce::var &json, const char *key);

//==========================================================================
// Array element accessors

/** Get string from array at index
 *  @param array JSON array to read from
 *  @param index Array index
 *  @param defaultVal Default value if index is out of bounds or invalid
 *  @return String value or default
 */
juce::String getStringAt(const juce::var &array, int index, const juce::String &defaultVal = {});

/** Get int from array at index
 *  @param array JSON array to read from
 *  @param index Array index
 *  @param defaultVal Default value if index is out of bounds or invalid
 *  @return Integer value or default
 */
int getIntAt(const juce::var &array, int index, int defaultVal = 0);

/** Get object from array at index
 *  @param array JSON array to read from
 *  @param index Array index
 *  @return Object at index or void var if out of bounds
 */
juce::var getObjectAt(const juce::var &array, int index);

//==========================================================================
// Utility functions

/** Check if json has a key (and it's not void/undefined)
 *  @param json JSON object to check
 *  @param key Property key name
 *  @return true if key exists and has a value
 */
bool hasKey(const juce::var &json, const char *key);

/** Check if value is an array
 *  @param value Value to check
 *  @return true if value is an array
 */
bool isArray(const juce::var &value);

/** Check if value is an object (DynamicObject)
 *  @param value Value to check
 *  @return true if value is an object
 */
bool isObject(const juce::var &value);

/** Get array size (returns 0 if not an array)
 *  @param array Array to get size of
 *  @return Array size or 0 if not an array
 */
int arraySize(const juce::var &array);

} // namespace Json
