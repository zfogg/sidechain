#include "Json.h"

namespace Json {

//==============================================================================
// Primitive type accessors

juce::String getString(const juce::var &json, const char *key, const juce::String &defaultVal) {
  if (!json.isObject())
    return defaultVal;

  auto value = json.getProperty(key, juce::var());
  if (value.isVoid() || value.isUndefined())
    return defaultVal;

  return value.toString();
}

int getInt(const juce::var &json, const char *key, int defaultVal) {
  if (!json.isObject())
    return defaultVal;

  auto value = json.getProperty(key, juce::var());
  if (value.isVoid() || value.isUndefined())
    return defaultVal;

  return static_cast<int>(value);
}

juce::int64 getInt64(const juce::var &json, const char *key, juce::int64 defaultVal) {
  if (!json.isObject())
    return defaultVal;

  auto value = json.getProperty(key, juce::var());
  if (value.isVoid() || value.isUndefined())
    return defaultVal;

  return static_cast<juce::int64>(value);
}

float getFloat(const juce::var &json, const char *key, float defaultVal) {
  if (!json.isObject())
    return defaultVal;

  auto value = json.getProperty(key, juce::var());
  if (value.isVoid() || value.isUndefined())
    return defaultVal;

  return static_cast<float>(value);
}

double getDouble(const juce::var &json, const char *key, double defaultVal) {
  if (!json.isObject())
    return defaultVal;

  auto value = json.getProperty(key, juce::var());
  if (value.isVoid() || value.isUndefined())
    return defaultVal;

  return static_cast<double>(value);
}

bool getBool(const juce::var &json, const char *key, bool defaultVal) {
  if (!json.isObject())
    return defaultVal;

  auto value = json.getProperty(key, juce::var());
  if (value.isVoid() || value.isUndefined())
    return defaultVal;

  return static_cast<bool>(value);
}

//==============================================================================
// Complex type accessors

juce::var getObject(const juce::var &json, const char *key) {
  if (!json.isObject())
    return {};

  auto value = json.getProperty(key, juce::var());
  if (!value.isObject())
    return {};

  return value;
}

juce::var getArray(const juce::var &json, const char *key) {
  if (!json.isObject())
    return {};

  auto value = json.getProperty(key, juce::var());
  if (!value.isArray())
    return {};

  return value;
}

//==============================================================================
// Array element accessors

juce::String getStringAt(const juce::var &array, int index, const juce::String &defaultVal) {
  if (!array.isArray())
    return defaultVal;

  auto *arr = array.getArray();
  if (arr == nullptr || index < 0 || index >= arr->size())
    return defaultVal;

  return (*arr)[index].toString();
}

int getIntAt(const juce::var &array, int index, int defaultVal) {
  if (!array.isArray())
    return defaultVal;

  auto *arr = array.getArray();
  if (arr == nullptr || index < 0 || index >= arr->size())
    return defaultVal;

  return static_cast<int>((*arr)[index]);
}

juce::var getObjectAt(const juce::var &array, int index) {
  if (!array.isArray())
    return {};

  auto *arr = array.getArray();
  if (arr == nullptr || index < 0 || index >= arr->size())
    return {};

  auto value = (*arr)[index];
  if (!value.isObject())
    return {};

  return value;
}

//==============================================================================
// Utility functions

bool hasKey(const juce::var &json, const char *key) {
  if (!json.isObject())
    return false;

  auto value = json.getProperty(key, juce::var());
  return !value.isVoid() && !value.isUndefined();
}

bool isArray(const juce::var &value) {
  return value.isArray();
}

bool isObject(const juce::var &value) {
  return value.isObject();
}

int arraySize(const juce::var &array) {
  if (!array.isArray())
    return 0;

  auto *arr = array.getArray();
  return arr ? arr->size() : 0;
}

} // namespace Json
