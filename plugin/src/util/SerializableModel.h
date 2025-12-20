#pragma once

#include "Result.h"
#include <JuceHeader.h>
#include <nlohmann/json.hpp>
#include <memory>

namespace Sidechain {

/**
 * CRTP (Curiously Recurring Template Pattern) base class for serializable models
 *
 * Provides automatic implementation of createFromJson() for all models.
 * Models must inherit from this class and implement:
 * - isValid() const method for validation
 * - from_json(const nlohmann::json&, ModelType&) free function in their namespace
 * - to_json(nlohmann::json&, const ModelType&) free function in their namespace
 *
 * Usage:
 * struct MyModel : public SerializableModel<MyModel> {
 *   // ... fields ...
 *   bool isValid() const { ... }
 * };
 *
 * inline void from_json(const nlohmann::json& j, MyModel& m) { ... }
 * inline void to_json(nlohmann::json& j, const MyModel& m) { ... }
 */
template <typename Derived> class SerializableModel {
public:
  /**
   * Create a shared_ptr instance from JSON with validation
   * Uses ADL to find the derived type's from_json function
   *
   * @param json The JSON object to parse
   * @return Outcome containing shared_ptr<Derived> if valid, or error message
   */
  static Outcome<std::shared_ptr<Derived>> createFromJson(const nlohmann::json &json) {
    try {
      if (!json.is_object())
        return Outcome<std::shared_ptr<Derived>>::error("Invalid JSON: expected object");

      Derived model;
      // ADL (Argument-Dependent Lookup) finds from_json in Derived's namespace
      from_json(json, model);

      if (!model.isValid())
        return Outcome<std::shared_ptr<Derived>>::error("Invalid data: missing required fields");

      return Outcome<std::shared_ptr<Derived>>::ok(std::make_shared<Derived>(model));
    } catch (const std::exception &e) {
      return Outcome<std::shared_ptr<Derived>>::error("Parse error: " + juce::String(e.what()));
    }
  }

  /**
   * Convert a shared_ptr model instance to JSON
   * Uses ADL to find the derived type's to_json function
   *
   * @param model The model instance to serialize
   * @return Outcome containing nlohmann::json if successful, or error message
   */
  static Outcome<nlohmann::json> toJson(const std::shared_ptr<Derived> &model) {
    if (!model)
      return Outcome<nlohmann::json>::error("Cannot serialize null model");

    try {
      nlohmann::json j;
      // ADL (Argument-Dependent Lookup) finds to_json in Derived's namespace
      to_json(j, *model);
      return Outcome<nlohmann::json>::ok(j);
    } catch (const std::exception &e) {
      return Outcome<nlohmann::json>::error("Serialization error: " + juce::String(e.what()));
    }
  }

  /**
   * Create a vector of shared_ptr instances from a JSON array
   * Uses ADL to find the derived type's from_json function
   *
   * @param jsonArray The JSON array to parse
   * @return Outcome containing vector of shared_ptr<Derived> if valid, or error message
   */
  static Outcome<std::vector<std::shared_ptr<Derived>>> createFromJsonArray(const nlohmann::json &jsonArray) {
    try {
      if (!jsonArray.is_array())
        return Outcome<std::vector<std::shared_ptr<Derived>>>::error("Invalid JSON: expected array");

      std::vector<std::shared_ptr<Derived>> models;
      models.reserve(jsonArray.size());

      for (const auto &item : jsonArray) {
        auto result = createFromJson(item);
        if (result.isError())
          return Outcome<std::vector<std::shared_ptr<Derived>>>::error("Failed to parse array item: " +
                                                                       result.getError());

        models.push_back(result.getValue());
      }

      return Outcome<std::vector<std::shared_ptr<Derived>>>::ok(std::move(models));
    } catch (const std::exception &e) {
      return Outcome<std::vector<std::shared_ptr<Derived>>>::error("Parse error: " + juce::String(e.what()));
    }
  }

protected:
  // Protected to prevent instantiation of base class directly
  SerializableModel() = default;
  ~SerializableModel() = default;

  // Explicitly default copy/move operations to avoid deprecation warnings
  SerializableModel(const SerializableModel &) = default;
  SerializableModel &operator=(const SerializableModel &) = default;
  SerializableModel(SerializableModel &&) = default;
  SerializableModel &operator=(SerializableModel &&) = default;
};

} // namespace Sidechain
