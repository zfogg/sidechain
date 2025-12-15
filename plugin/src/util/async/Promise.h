#pragma once

#include <JuceHeader.h>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

namespace Sidechain {
namespace Util {

/**
 * Promise<T> - Result wrapper for chaining async operations
 *
 * Similar to JavaScript Promises, allows chaining async operations with
 * then(), catch(), and finally() methods.
 *
 * Features:
 * - Promise chaining for sequential async operations
 * - Error handling with catch()
 * - Finalization with finally()
 * - Value transformation with then()
 * - Thread-safe resolution/rejection
 *
 * Example:
 *   auto promise = fetchData("url")
 *       .then([](const juce::String& data) {
 *           return parseJSON(data);
 *       })
 *       .then([](const JSONObj& obj) {
 *           return obj.getString("name");
 *       })
 *       .catch([](const juce::String& err) {
 *           std::cout << "Error: " << err << std::endl;
 *       });
 *
 * @tparam T The resolved value type
 */
template <typename T> class Promise;

/**
 * Outcome<T, E> - Result/Error wrapper for operations
 *
 * @tparam T Success value type
 * @tparam E Error type (usually juce::String or std::exception)
 */
template <typename T, typename E = juce::String> class Outcome {
public:
  using Value = T;
  using Error = E;
  using SuccessCallback = std::function<void(const T &)>;
  using ErrorCallback = std::function<void(const E &)>;
  using ThenCallback = std::function<Outcome<T, E>(const T &)>;

  /**
   * Create successful outcome
   */
  static Outcome success(const T &value) {
    return Outcome(value, true);
  }

  /**
   * Create failed outcome
   */
  static Outcome failure(const E &error) {
    return Outcome(error, false);
  }

  /**
   * Check if operation was successful
   */
  bool isSuccess() const {
    return isSuccess_;
  }

  /**
   * Check if operation failed
   */
  bool isError() const {
    return !isSuccess_;
  }

  /**
   * Get the value (only valid if isSuccess())
   */
  const T &getValue() const {
    return value;
  }

  /**
   * Get the error (only valid if isError())
   */
  const E &getError() const {
    return error;
  }

  /**
   * Chain a operation that transforms the value
   */
  template <typename U> Outcome<U, E> then(std::function<U(const T &)> transform) const {
    if (!isSuccess_)
      return Outcome<U, E>::failure(error);

    try {
      return Outcome<U, E>::success(transform(value));
    } catch (const std::exception &ex) {
      return Outcome<U, E>::failure(E(juce::String(ex.what())));
    }
  }

  /**
   * Chain an operation that might fail
   */
  template <typename U> Outcome<U, E> flatMap(std::function<Outcome<U, E>(const T &)> transform) const {
    if (!isSuccess_)
      return Outcome<U, E>::failure(error);

    try {
      return transform(value);
    } catch (const std::exception &ex) {
      return Outcome<U, E>::failure(E(juce::String(ex.what())));
    }
  }

  /**
   * Handle success case
   */
  Outcome &onSuccess(SuccessCallback callback) {
    if (isSuccess_ && callback)
      callback(value);
    return *this;
  }

  /**
   * Handle error case
   */
  Outcome &onError(ErrorCallback callback) {
    if (!isSuccess_ && callback)
      callback(error);
    return *this;
  }

  /**
   * Get value or default
   */
  T getOrElse(const T &defaultValue) const {
    return isSuccess_ ? value : defaultValue;
  }

  // Copy/Move constructors
  Outcome(const Outcome &) = default;
  Outcome &operator=(const Outcome &) = default;
  Outcome(Outcome &&) = default;
  Outcome &operator=(Outcome &&) = default;

private:
  T value{};
  E error{};
  bool isSuccess_ = false;

  Outcome(const T &v, bool s) : value(v), isSuccess_(s) {}
  Outcome(const E &e, bool s) : error(e), isSuccess_(s) {}
};

/**
 * AsyncPromise<T> - Promise that resolves/rejects asynchronously
 *
 * Used for operations that complete on background threads.
 */
template <typename T> class AsyncPromise : public std::enable_shared_from_this<AsyncPromise<T>> {
public:
  using Value = T;
  using Resolver = std::function<void(const T &)>;
  using Rejecter = std::function<void(const juce::String &)>;
  using ThenCallback = std::function<void(const T &)>;
  using CatchCallback = std::function<void(const juce::String &)>;
  using FinallyCallback = std::function<void()>;

  /**
   * Create new async promise
   */
  static std::shared_ptr<AsyncPromise> create() {
    return std::make_shared<AsyncPromise>();
  }

  /**
   * Resolve promise with value
   */
  void resolve(const T &value) {
    std::vector<ThenCallback> callbacks;
    std::vector<FinallyCallback> finallyCallbacks;

    {
      std::lock_guard<std::mutex> lock(mutex);
      if (resolved || rejected)
        return; // Already settled

      resolved = true;
      resolvedValue = value;
      callbacks = thenCallbacks;
      finallyCallbacks = finallyCallbacks_;
    }

    // Invoke callbacks outside lock
    for (auto &cb : callbacks) {
      if (cb)
        cb(value);
    }

    for (auto &cb : finallyCallbacks) {
      if (cb)
        cb();
    }
  }

  /**
   * Reject promise with error
   */
  void reject(const juce::String &error) {
    std::vector<CatchCallback> callbacks;
    std::vector<FinallyCallback> finallyCallbacks;

    {
      std::lock_guard<std::mutex> lock(mutex);
      if (resolved || rejected)
        return; // Already settled

      rejected = true;
      rejectionReason = error;
      callbacks = catchCallbacks;
      finallyCallbacks = finallyCallbacks_;
    }

    // Invoke callbacks outside lock
    for (auto &cb : callbacks) {
      if (cb)
        cb(error);
    }

    for (auto &cb : finallyCallbacks) {
      if (cb)
        cb();
    }
  }

  /**
   * Register callback for success
   */
  std::shared_ptr<AsyncPromise> then(ThenCallback callback) {
    if (!callback)
      return this->shared_from_this();

    {
      std::lock_guard<std::mutex> lock(mutex);

      if (resolved) {
        // Already resolved, call immediately
        callback(resolvedValue);
      } else if (!rejected) {
        // Pending, register for later
        thenCallbacks.push_back(callback);
      }
    }

    return this->shared_from_this();
  }

  /**
   * Register callback for error
   */
  std::shared_ptr<AsyncPromise> catch_impl(CatchCallback callback) {
    if (!callback)
      return this->shared_from_this();

    {
      std::lock_guard<std::mutex> lock(mutex);

      if (rejected) {
        // Already rejected, call immediately
        callback(rejectionReason);
      } else if (!resolved) {
        // Pending, register for later
        catchCallbacks.push_back(callback);
      }
    }

    return this->shared_from_this();
  }

  /**
   * Register finally callback
   */
  std::shared_ptr<AsyncPromise> finally(FinallyCallback callback) {
    if (!callback)
      return this->shared_from_this();

    {
      std::lock_guard<std::mutex> lock(mutex);

      if (resolved || rejected) {
        // Already settled, call immediately
        callback();
      } else {
        // Pending, register for later
        finallyCallbacks_.push_back(callback);
      }
    }

    return this->shared_from_this();
  }

  /**
   * Check if promise is settled (resolved or rejected)
   */
  bool isSettled() const {
    std::lock_guard<std::mutex> lock(mutex);
    return resolved || rejected;
  }

  /**
   * Check if promise is resolved
   */
  bool isResolved() const {
    std::lock_guard<std::mutex> lock(mutex);
    return resolved;
  }

  /**
   * Check if promise is rejected
   */
  bool isRejected() const {
    std::lock_guard<std::mutex> lock(mutex);
    return rejected;
  }

private:
  AsyncPromise() = default;

  mutable std::mutex mutex;
  bool resolved = false;
  bool rejected = false;
  T resolvedValue{};
  juce::String rejectionReason;

  std::vector<ThenCallback> thenCallbacks;
  std::vector<CatchCallback> catchCallbacks;
  std::vector<FinallyCallback> finallyCallbacks_;
};

} // namespace Util
} // namespace Sidechain
