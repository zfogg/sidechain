#pragma once

#include "../EntityStore.h"
#include "../slices/AppSlices.h"
#include "../../util/Log.h"
#include "../../util/Result.h"
#include <JuceHeader.h>
#include <nlohmann/json.hpp>
#include <functional>
#include <memory>
#include <vector>

namespace Sidechain {
namespace Stores {
namespace Utils {

// ==============================================================================
// UserLoadingHelper - Generic helper for loading user lists
// ==============================================================================

/**
 * Helper class to reduce boilerplate in user loading operations.
 * Handles the common pattern of:
 * 1. Set loading state
 * 2. Make network request
 * 3. Parse and normalize users
 * 4. Update state with results or error
 *
 * Usage:
 *   UserLoadingHelper<DiscoveryState>::loadUsers(
 *     discoverySlice,
 *     [](DiscoveryState& s) { s.isTrendingLoading = true; s.discoveryError = ""; },
 *     [this, limit](auto callback) { networkClient->getTrendingUsers(limit, callback); },
 *     [](DiscoveryState& s, auto users) {
 *       s.trendingUsers = users;
 *       s.isTrendingLoading = false;
 *       s.lastTrendingUpdate = juce::Time::getCurrentTime().toMilliseconds();
 *     },
 *     [](DiscoveryState& s, const juce::String& err) {
 *       s.isTrendingLoading = false;
 *       s.discoveryError = err;
 *     },
 *     "trending users"
 *   );
 */
template <typename StateType> class UserLoadingHelper {
public:
  using StateSlice = std::shared_ptr<ImmutableSlice<StateType>>;
  using UserList = std::vector<std::shared_ptr<const User>>;
  using SetLoadingFn = std::function<void(StateType &)>;
  using NetworkCallFn = std::function<void(std::function<void(Outcome<juce::var>)>)>;
  using OnSuccessFn = std::function<void(StateType &, UserList)>;
  using OnErrorFn = std::function<void(StateType &, const juce::String &)>;

  static void loadUsers(StateSlice slice, SetLoadingFn setLoading, NetworkCallFn networkCall, OnSuccessFn onSuccess,
                        OnErrorFn onError, const juce::String &logContext) {
    // Set loading state
    StateType loadingState = slice->getState();
    setLoading(loadingState);
    slice->setState(loadingState);

    // Make network request
    networkCall([slice, onSuccess, onError, logContext](Outcome<juce::var> result) {
      if (result.isError()) {
        StateType errorState = slice->getState();
        onError(errorState, result.getError());
        slice->setState(errorState);
        Log::error("UserLoadingHelper", "Failed to load " + logContext + ": " + result.getError());
        return;
      }

      try {
        auto jsonArray = result.getValue().getArray();
        if (!jsonArray) {
          throw std::runtime_error("Response is not an array");
        }

        // Normalize and cache users in EntityStore
        std::vector<std::shared_ptr<User>> mutableUsers;
        for (int i = 0; i < jsonArray->size(); ++i) {
          try {
            auto jsonStr = (*jsonArray)[i].toString().toStdString();
            auto json = nlohmann::json::parse(jsonStr);
            auto user = EntityStore::getInstance().normalizeUser(json);
            if (user) {
              mutableUsers.push_back(user);
            }
          } catch (const std::exception &e) {
            Log::warn("UserLoadingHelper", "Failed to parse " + logContext + " JSON: " + juce::String(e.what()));
          }
        }

        // Convert to immutable view
        UserList immutableUsers;
        immutableUsers.reserve(mutableUsers.size());
        for (const auto &user : mutableUsers) {
          immutableUsers.push_back(std::const_pointer_cast<const User>(user));
        }

        // Update state
        StateType successState = slice->getState();
        onSuccess(successState, std::move(immutableUsers));
        slice->setState(successState);

        Log::info("UserLoadingHelper", "Loaded " + juce::String(mutableUsers.size()) + " " + logContext);

      } catch (const std::exception &e) {
        StateType errorState = slice->getState();
        onError(errorState, juce::String(e.what()));
        slice->setState(errorState);
        Log::error("UserLoadingHelper", "Failed to load " + logContext + ": " + juce::String(e.what()));
      }
    });
  }
};

// ==============================================================================
// GenericListLoader - Template for loading any list type
// ==============================================================================

/**
 * Generic list loading helper for any model type.
 * Handles the common loading pattern for various list types.
 *
 * @tparam StateType The state struct type
 * @tparam ModelType The model type being loaded
 */
template <typename StateType, typename ModelType> class GenericListLoader {
public:
  using StateSlice = std::shared_ptr<ImmutableSlice<StateType>>;
  using ModelList = std::vector<std::shared_ptr<ModelType>>;
  using SetLoadingFn = std::function<void(StateType &)>;
  using NetworkCallFn = std::function<void(std::function<void(Outcome<juce::var>)>)>;
  using ParseFn = std::function<ModelList(const juce::var &)>;
  using OnSuccessFn = std::function<void(StateType &, ModelList)>;
  using OnErrorFn = std::function<void(StateType &, const juce::String &)>;

  static void load(StateSlice slice, SetLoadingFn setLoading, NetworkCallFn networkCall, ParseFn parse,
                   OnSuccessFn onSuccess, OnErrorFn onError, const juce::String &logContext) {
    // Set loading state
    StateType loadingState = slice->getState();
    setLoading(loadingState);
    slice->setState(loadingState);

    // Make network request
    networkCall([slice, parse, onSuccess, onError, logContext](Outcome<juce::var> result) {
      if (result.isError()) {
        StateType errorState = slice->getState();
        onError(errorState, result.getError());
        slice->setState(errorState);
        Log::error("GenericListLoader", "Failed to load " + logContext + ": " + result.getError());
        return;
      }

      try {
        auto models = parse(result.getValue());

        StateType successState = slice->getState();
        onSuccess(successState, std::move(models));
        slice->setState(successState);

        Log::info("GenericListLoader", "Loaded " + juce::String(models.size()) + " " + logContext);

      } catch (const std::exception &e) {
        StateType errorState = slice->getState();
        onError(errorState, juce::String(e.what()));
        slice->setState(errorState);
        Log::error("GenericListLoader", "Exception loading " + logContext + ": " + juce::String(e.what()));
      }
    });
  }
};

// ==============================================================================
// AsyncStateUpdater - Simplified state update pattern
// ==============================================================================

/**
 * Simplifies the pattern of getting state, modifying it, and setting it back.
 *
 * Usage:
 *   AsyncStateUpdater<SearchState>::update(slice, [](SearchState& s) {
 *     s.isSearching = true;
 *     s.error = "";
 *   });
 */
template <typename StateType> class AsyncStateUpdater {
public:
  using StateSlice = std::shared_ptr<ImmutableSlice<StateType>>;
  using UpdateFn = std::function<void(StateType &)>;

  static void update(StateSlice slice, UpdateFn updateFn) {
    StateType state = slice->getState();
    updateFn(state);
    slice->setState(state);
  }

  /**
   * Update with conditional - only applies update if condition returns true.
   */
  static bool updateIf(StateSlice slice, std::function<bool(const StateType &)> condition, UpdateFn updateFn) {
    StateType state = slice->getState();
    if (!condition(state)) {
      return false;
    }
    updateFn(state);
    slice->setState(state);
    return true;
  }
};

} // namespace Utils
} // namespace Stores
} // namespace Sidechain
