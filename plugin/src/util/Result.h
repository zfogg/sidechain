#pragma once

#include "Log.h"
#include <JuceHeader.h>
#include <functional>
#include <optional>

//==============================================================================
/**
 * Outcome<T> - A type-safe error handling utility
 *
 * Named "Outcome" to avoid conflict with juce::Result.
 * Inspired by Rust's Result type, this provides explicit error handling
 * instead of ad-hoc bool + string pairs or exceptions.
 *
 * Usage:
 *   // Returning results
 *   Outcome<User> fetchUser(const juce::String& id) {
 *       if (id.isEmpty())
 *           return Outcome<User>::error("User ID is empty");
 *       // ... fetch logic
 *       return Outcome<User>::ok(user);
 *   }
 *
 *   // Handling results
 *   auto result = fetchUser(userId);
 *   if (result.isOk())
 *       setUser(result.getValue());
 *   else
 *       Log::error("Failed to fetch user: " + result.getError());
 *
 *   // Chaining with map/flatMap
 *   auto nameResult = fetchUser(userId)
 *       .map([](const User& u) { return u.name; });
 *
 *   // Providing defaults
 *   auto name = fetchUser(userId)
 *       .map([](const User& u) { return u.name; })
 *       .getValueOr("Unknown");
 */
template <typename T> class Outcome {
public:
  //==========================================================================
  // Factory methods

  /** Create a successful result with a value */
  static Outcome<T> ok(const T &value) {
    return Outcome<T>(value);
  }

  /** Create a successful result with a moved value */
  static Outcome<T> ok(T &&value) {
    return Outcome<T>(std::move(value));
  }

  /** Create a failed result with an error message */
  static Outcome<T> error(const juce::String &message) {
    return Outcome<T>(message, true);
  }

  //==========================================================================
  // State checking

  /** Returns true if this result contains a value */
  bool isOk() const noexcept {
    return hasValue;
  }

  /** Returns true if this result contains an error */
  bool isError() const noexcept {
    return !hasValue;
  }

  /** Implicit bool conversion - true if ok */
  explicit operator bool() const noexcept {
    return hasValue;
  }

  //==========================================================================
  // Value access

  /**
   * Get the value. Only call if isOk() returns true.
   * Logs an error and returns default T if called on an error result.
   */
  const T &getValue() const {
    if (!hasValue) {
      Log::error("Outcome::getValue() called on error result: " + errorMessage);
      static T defaultValue{};
      return defaultValue;
    }
    return value.value();
  }

  /**
   * Get the value (mutable). Only call if isOk() returns true.
   */
  T &getValue() {
    if (!hasValue) {
      Log::error("Outcome::getValue() called on error result: " + errorMessage);
      static T defaultValue{};
      return defaultValue;
    }
    return value.value();
  }

  /**
   * Get the value or a default if this is an error result.
   */
  T getValueOr(const T &defaultValue) const {
    return hasValue ? value.value() : defaultValue;
  }

  /**
   * Get the value or compute a default if this is an error result.
   */
  T getValueOrElse(std::function<T()> defaultFn) const {
    return hasValue ? value.value() : defaultFn();
  }

  /**
   * Get the error message. Returns empty string if this is an ok result.
   */
  const juce::String &getError() const noexcept {
    return errorMessage;
  }

  //==========================================================================
  // Monadic operations

  /**
   * Transform the value if ok, propagate error if not.
   *
   * Example:
   *   Outcome<int> count = fetchItems().map([](auto& items) { return
   * items.size(); });
   */
  template <typename U> Outcome<U> map(std::function<U(const T &)> fn) const {
    if (hasValue)
      return Outcome<U>::ok(fn(value.value()));
    return Outcome<U>::error(errorMessage);
  }

  /**
   * Transform the value with a function that returns an Outcome.
   * Useful for chaining operations that can fail.
   *
   * Example:
   *   Outcome<Profile> profile = fetchUser(id).flatMap([](auto& u) {
   *       return fetchProfile(u.profileId);
   *   });
   */
  template <typename U> Outcome<U> flatMap(std::function<Outcome<U>(const T &)> fn) const {
    if (hasValue)
      return fn(value.value());
    return Outcome<U>::error(errorMessage);
  }

  /**
   * Execute a function if this is ok, return self for chaining.
   */
  const Outcome<T> &onSuccess(std::function<void(const T &)> fn) const {
    if (hasValue)
      fn(value.value());
    return *this;
  }

  /**
   * Execute a function if this is an error, return self for chaining.
   */
  const Outcome<T> &onError(std::function<void(const juce::String &)> fn) const {
    if (!hasValue)
      fn(errorMessage);
    return *this;
  }

  /**
   * Log the error if this is an error result.
   */
  const Outcome<T> &logIfError(const juce::String &context = "") const {
    if (!hasValue) {
      if (context.isNotEmpty())
        Log::error(context + ": " + errorMessage);
      else
        Log::error(errorMessage);
    }
    return *this;
  }

  /**
   * Transform the error message if this is an error result.
   */
  Outcome<T> mapError(std::function<juce::String(const juce::String &)> fn) const {
    if (hasValue)
      return *this;
    return Outcome<T>::error(fn(errorMessage));
  }

  /**
   * Provide a recovery value if this is an error.
   */
  Outcome<T> recover(std::function<T(const juce::String &)> fn) const {
    if (hasValue)
      return *this;
    return Outcome<T>::ok(fn(errorMessage));
  }

private:
  //==========================================================================
  // Private constructors

  // Ok constructor
  explicit Outcome(const T &val) : value(val), hasValue(true) {}

  explicit Outcome(T &&val) : value(std::move(val)), hasValue(true) {}

  // Error constructor
  Outcome(const juce::String &error, bool) : errorMessage(error), hasValue(false) {}

  //==========================================================================
  // Members

  std::optional<T> value;
  juce::String errorMessage;
  bool hasValue = false;
};

//==============================================================================
/**
 * Outcome<void> specialization for operations that don't return a value.
 *
 * Usage:
 *   Outcome<void> deleteUser(const juce::String& id) {
 *       if (id.isEmpty())
 *           return Outcome<void>::error("User ID is empty");
 *       // ... delete logic
 *       return Outcome<void>::ok();
 *   }
 */
template <> class Outcome<void> {
public:
  //==========================================================================
  // Factory methods

  /** Create a successful void result */
  static Outcome<void> ok() {
    return Outcome<void>(true);
  }

  /** Create a failed result with an error message */
  static Outcome<void> error(const juce::String &message) {
    return Outcome<void>(message);
  }

  //==========================================================================
  // State checking

  bool isOk() const noexcept {
    return success;
  }
  bool isError() const noexcept {
    return !success;
  }
  explicit operator bool() const noexcept {
    return success;
  }

  //==========================================================================
  // Error access

  const juce::String &getError() const noexcept {
    return errorMessage;
  }

  //==========================================================================
  // Chaining

  const Outcome<void> &onSuccess(std::function<void()> fn) const {
    if (success)
      fn();
    return *this;
  }

  const Outcome<void> &onError(std::function<void(const juce::String &)> fn) const {
    if (!success)
      fn(errorMessage);
    return *this;
  }

  const Outcome<void> &logIfError(const juce::String &context = "") const {
    if (!success) {
      if (context.isNotEmpty())
        Log::error(context + ": " + errorMessage);
      else
        Log::error(errorMessage);
    }
    return *this;
  }

  /**
   * Chain another operation if this one succeeded.
   */
  template <typename U> Outcome<U> then(std::function<Outcome<U>()> fn) const {
    if (success)
      return fn();
    return Outcome<U>::error(errorMessage);
  }

private:
  explicit Outcome(bool ok) : success(ok) {}
  Outcome(const juce::String &error) : errorMessage(error), success(false) {}

  juce::String errorMessage;
  bool success = false;
};

//==============================================================================
// Type aliases for common result types

using VoidOutcome = Outcome<void>;
using StringOutcome = Outcome<juce::String>;
using IntOutcome = Outcome<int>;
using BoolOutcome = Outcome<bool>;
using JsonOutcome = Outcome<juce::var>;
