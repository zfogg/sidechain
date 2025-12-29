#pragma once

#include <JuceHeader.h>

namespace Sidechain {
namespace Util {

/**
 * ModelBuilder - Copy-with-modifications pattern for immutable state updates
 *
 * Provides a fluent interface for creating modified copies of model objects
 * without mutating the original. Enforces the architectural pattern that all
 * state changes flow through Redux state updates, not direct mutations.
 *
 * Usage:
 * ```cpp
 * auto user = entityCache.get(userId);
 * auto updated = ModelBuilder(*user)
 *     .withFollowStatus(true)
 *     .withFollowerCount(user->followerCount + 1)
 *     .build();
 * ```
 *
 * This pattern ensures:
 * 1. Original entity is never mutated
 * 2. Modifications are explicit and traceable
 * 3. All changes can be logged for debugging
 * 4. State updates are atomic (copy-replace, not in-place mutation)
 */

/**
 * ImmutableGuard - Wrapper that prevents direct mutation
 *
 * Used by state slices to enforce that model objects can only be modified
 * through copy-with-modifications, not through direct field access.
 *
 * Usage:
 * ```cpp
 * class User {
 * private:
 *     friend class ImmutableGuard<User>;
 *     bool isFollowing = false;  // Private - prevent direct access
 *
 * public:
 *     bool getIsFollowing() const { return isFollowing; }
 *
 *     User withFollowStatus(bool following) const {
 *         auto copy = *this;
 *         copy.isFollowing = following;  // ImmutableGuard friend access
 *         return copy;
 *     }
 * };
 * ```
 */
template <typename T> class ImmutableGuard {
public:
  /**
   * Allows controlled mutation only through state update functions
   * Called only by StateSubject<T>::next() to ensure mutations
   * are tracked and observable
   */
  static T mutateSafely(const T &original, std::function<void(T &)> mutator) {
    T copy = original;
    mutator(copy);
    return copy;
  }
};

/**
 * ConcreteBuilder - Template for building copy-with-modifications
 *
 * Model classes inherit from this to get standard builder methods.
 *
 * Example:
 * ```cpp
 * struct User : public ConcreteBuilder<User> {
 *     // Fields...
 *
 *     // Builder methods (return modified copy)
 *     User withFollowStatus(bool following) const {
 *         return this->modify([following](User& u) {
 *             u.isFollowing = following;
 *         });
 *     }
 * };
 * ```
 */
template <typename T> class ConcreteBuilder {
protected:
  /**
   * Create a modified copy without mutating original
   * Protected - only subclass can use this
   */
  T modify(std::function<void(T &)> mutator) const {
    T copy = static_cast<const T &>(*this);
    mutator(copy);
    return copy;
  }
};

} // namespace Util
} // namespace Sidechain
