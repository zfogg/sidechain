#pragma once

#include "../EntityStore.h"
#include "../AppState.h"
#include "../../util/logging/Logger.h"
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
 *     discoveryState,
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
  using StateRef = Rx::State<StateType>;
  using UserList = std::vector<std::shared_ptr<const User>>;
  using SetLoadingFn = std::function<void(StateType &)>;
  using NetworkCallFn = std::function<void(std::function<void(Outcome<juce::var>)>)>;
  using OnSuccessFn = std::function<void(StateType &, UserList)>;
  using OnErrorFn = std::function<void(StateType &, const juce::String &)>;

  static void loadUsers(StateRef state, SetLoadingFn setLoading, NetworkCallFn networkCall, OnSuccessFn onSuccess,
                        OnErrorFn onError, const juce::String &logContext) {
    // Set loading state
    StateType loadingState = state->getState();
    setLoading(loadingState);
    state->setState(loadingState);

    // Make network request
    networkCall([state, onSuccess, onError, logContext](Outcome<juce::var> result) {
      if (result.isError()) {
        StateType errorState = state->getState();
        onError(errorState, result.getError());
        state->setState(errorState);
        Util::logError("UserLoadingHelper", "Failed to load " + logContext + ": " + result.getError());
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
            Util::logWarning("UserLoadingHelper", "Failed to parse " + logContext + " JSON: " + juce::String(e.what()));
          }
        }

        // Convert to immutable view
        UserList immutableUsers;
        immutableUsers.reserve(mutableUsers.size());
        for (const auto &user : mutableUsers) {
          immutableUsers.push_back(std::const_pointer_cast<const User>(user));
        }

        // Update state
        StateType successState = state->getState();
        onSuccess(successState, std::move(immutableUsers));
        state->setState(successState);

        Util::logInfo("UserLoadingHelper", "Loaded " + juce::String(mutableUsers.size()) + " " + logContext);

      } catch (const std::exception &e) {
        StateType errorState = state->getState();
        onError(errorState, juce::String(e.what()));
        state->setState(errorState);
        Util::logError("UserLoadingHelper", "Failed to load " + logContext + ": " + juce::String(e.what()));
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
  using StateRef = Rx::State<StateType>;
  using ModelList = std::vector<std::shared_ptr<ModelType>>;
  using SetLoadingFn = std::function<void(StateType &)>;
  using NetworkCallFn = std::function<void(std::function<void(Outcome<juce::var>)>)>;
  using ParseFn = std::function<ModelList(const juce::var &)>;
  using OnSuccessFn = std::function<void(StateType &, ModelList)>;
  using OnErrorFn = std::function<void(StateType &, const juce::String &)>;

  static void load(StateRef state, SetLoadingFn setLoading, NetworkCallFn networkCall, ParseFn parse,
                   OnSuccessFn onSuccess, OnErrorFn onError, const juce::String &logContext) {
    // Set loading state
    StateType loadingState = state->getState();
    setLoading(loadingState);
    state->setState(loadingState);

    // Make network request
    networkCall([state, parse, onSuccess, onError, logContext](Outcome<juce::var> result) {
      if (result.isError()) {
        StateType errorState = state->getState();
        onError(errorState, result.getError());
        state->setState(errorState);
        Util::logError("GenericListLoader", "Failed to load " + logContext + ": " + result.getError());
        return;
      }

      try {
        auto models = parse(result.getValue());

        StateType successState = state->getState();
        onSuccess(successState, std::move(models));
        state->setState(successState);

        Util::logInfo("GenericListLoader", "Loaded " + juce::String(models.size()) + " " + logContext);

      } catch (const std::exception &e) {
        StateType errorState = state->getState();
        onError(errorState, juce::String(e.what()));
        state->setState(errorState);
        Util::logError("GenericListLoader", "Exception loading " + logContext + ": " + juce::String(e.what()));
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
 *   AsyncStateUpdater<SearchState>::update(state, [](SearchState& s) {
 *     s.isSearching = true;
 *     s.error = "";
 *   });
 */
template <typename StateType> class AsyncStateUpdater {
public:
  using StateRef = Rx::State<StateType>;
  using UpdateFn = std::function<void(StateType &)>;

  static void update(StateRef stateRef, UpdateFn updateFn) {
    StateType state = stateRef->getState();
    updateFn(state);
    stateRef->setState(state);
  }

  /**
   * Update with conditional - only applies update if condition returns true.
   */
  static bool updateIf(StateRef stateRef, std::function<bool(const StateType &)> condition, UpdateFn updateFn) {
    StateType state = stateRef->getState();
    if (!condition(state)) {
      return false;
    }
    updateFn(state);
    stateRef->setState(state);
    return true;
  }
};

} // namespace Utils
} // namespace Stores
} // namespace Sidechain
