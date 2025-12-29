#include "../AppStore.h"
#include "../EntityStore.h"
#include "../util/UserLoadingHelper.h"
#include "../util/StoreUtils.h"
#include "../../util/logging/Logger.h"
#include "../../util/Json.h"
#include "../../util/rx/JuceScheduler.h"
#include "../../models/User.h"
#include <rxcpp/rx.hpp>

namespace Sidechain {
namespace Stores {

using Utils::NetworkClientGuard;
using Utils::UserLoadingHelper;

// ==============================================================================
// Discovery Redux Actions
// ==============================================================================

void AppStore::loadTrendingUsersAndCache(int limit) {
  if (!NetworkClientGuard::check(networkClient, "load trending users")) {
    return;
  }

  Util::logInfo("AppStore", "Loading trending users");

  // Set loading state
  DiscoveryState loadingState = stateManager.discovery->getState();
  loadingState.isTrendingLoading = true;
  loadingState.discoveryError = "";
  stateManager.discovery->setState(loadingState);

  loadTrendingUsersObservable(limit).subscribe(
      [this](const std::vector<User> &users) {
        // Convert to shared_ptr<const User> for state
        std::vector<std::shared_ptr<const User>> userPtrs;
        userPtrs.reserve(users.size());
        for (const auto &u : users) {
          userPtrs.push_back(std::make_shared<const User>(u));
        }

        DiscoveryState successState = stateManager.discovery->getState();
        successState.trendingUsers = std::move(userPtrs);
        successState.isTrendingLoading = false;
        successState.discoveryError = "";
        successState.lastTrendingUpdate = juce::Time::getCurrentTime().toMilliseconds();
        stateManager.discovery->setState(successState);
      },
      [this](std::exception_ptr ep) {
        juce::String errorMsg;
        try {
          std::rethrow_exception(ep);
        } catch (const std::exception &e) {
          errorMsg = e.what();
        }
        DiscoveryState errorState = stateManager.discovery->getState();
        errorState.isTrendingLoading = false;
        errorState.discoveryError = errorMsg;
        stateManager.discovery->setState(errorState);
      });
}

void AppStore::loadFeaturedProducersAndCache(int limit) {
  if (!NetworkClientGuard::check(networkClient, "load featured producers")) {
    return;
  }

  Util::logInfo("AppStore", "Loading featured producers");

  // Set loading state
  DiscoveryState loadingState = stateManager.discovery->getState();
  loadingState.isFeaturedLoading = true;
  loadingState.discoveryError = "";
  stateManager.discovery->setState(loadingState);

  loadFeaturedProducersObservable(limit).subscribe(
      [this](const std::vector<User> &users) {
        // Convert to shared_ptr<const User> for state
        std::vector<std::shared_ptr<const User>> userPtrs;
        userPtrs.reserve(users.size());
        for (const auto &u : users) {
          userPtrs.push_back(std::make_shared<const User>(u));
        }

        DiscoveryState successState = stateManager.discovery->getState();
        successState.featuredProducers = std::move(userPtrs);
        successState.isFeaturedLoading = false;
        successState.discoveryError = "";
        successState.lastFeaturedUpdate = juce::Time::getCurrentTime().toMilliseconds();
        stateManager.discovery->setState(successState);
      },
      [this](std::exception_ptr ep) {
        juce::String errorMsg;
        try {
          std::rethrow_exception(ep);
        } catch (const std::exception &e) {
          errorMsg = e.what();
        }
        DiscoveryState errorState = stateManager.discovery->getState();
        errorState.isFeaturedLoading = false;
        errorState.discoveryError = errorMsg;
        stateManager.discovery->setState(errorState);
      });
}

void AppStore::loadSuggestedUsersAndCache(int limit) {
  if (!NetworkClientGuard::check(networkClient, "load suggested users")) {
    return;
  }

  Util::logInfo("AppStore", "Loading suggested users");

  // Set loading state
  DiscoveryState loadingState = stateManager.discovery->getState();
  loadingState.isSuggestedLoading = true;
  loadingState.discoveryError = "";
  stateManager.discovery->setState(loadingState);

  loadSuggestedUsersObservable(limit).subscribe(
      [this](const std::vector<User> &users) {
        // Convert to shared_ptr<const User> for state
        std::vector<std::shared_ptr<const User>> userPtrs;
        userPtrs.reserve(users.size());
        for (const auto &u : users) {
          userPtrs.push_back(std::make_shared<const User>(u));
        }

        DiscoveryState successState = stateManager.discovery->getState();
        successState.suggestedUsers = std::move(userPtrs);
        successState.isSuggestedLoading = false;
        successState.discoveryError = "";
        successState.lastSuggestedUpdate = juce::Time::getCurrentTime().toMilliseconds();
        stateManager.discovery->setState(successState);
      },
      [this](std::exception_ptr ep) {
        juce::String errorMsg;
        try {
          std::rethrow_exception(ep);
        } catch (const std::exception &e) {
          errorMsg = e.what();
        }
        DiscoveryState errorState = stateManager.discovery->getState();
        errorState.isSuggestedLoading = false;
        errorState.discoveryError = errorMsg;
        stateManager.discovery->setState(errorState);
      });
}

// ==============================================================================
// Reactive Discovery Observables (Phase 5)
// ==============================================================================
//
// These methods return rxcpp::observable with proper model types (User values,
// not shared_ptr). They use the same network calls as the Redux actions above
// but wrap them in reactive streams for compose-able operations.
//
// Usage examples:
//   // Simple subscription
//   appStore.loadTrendingUsersObservable(10)
//       .subscribe([](auto users) { displayUsers(users); });
//
//   // With retry backoff
//   Rx::retryWithBackoff(appStore.loadTrendingUsersObservable(10))
//       .subscribe([](auto users) { displayUsers(users); });

rxcpp::observable<std::vector<User>> AppStore::loadTrendingUsersObservable(int limit) {
  using ResultType = std::vector<User>;
  return rxcpp::sources::create<ResultType>([this, limit](auto observer) {
           if (!networkClient) {
             Util::logError("AppStore", "Network client not initialized");
             observer.on_error(std::make_exception_ptr(std::runtime_error("Network client not initialized")));
             return;
           }

           Util::logDebug("AppStore", "Loading trending users via observable");

           networkClient->getTrendingUsers(limit, [observer](Outcome<juce::var> result) {
             if (result.isOk()) {
               ResultType users;
               auto usersArray = result.getValue();

               if (usersArray.isArray()) {
                 for (int i = 0; i < usersArray.size(); ++i) {
                   try {
                     auto jsonStr = juce::JSON::toString(usersArray[i]);
                     auto jsonObj = nlohmann::json::parse(jsonStr.toStdString());
                     User user;
                     from_json(jsonObj, user);
                     if (user.isValid()) {
                       users.push_back(std::move(user));
                     }
                   } catch (const std::exception &e) {
                     Util::logWarning("AppStore", "Failed to parse trending user: " + juce::String(e.what()));
                   }
                 }
               }

               Util::logInfo("AppStore", "Loaded " + juce::String(static_cast<int>(users.size())) + " trending users");
               observer.on_next(std::move(users));
               observer.on_completed();
             } else {
               Util::logError("AppStore", "Failed to load trending users: " + result.getError());
               observer.on_error(std::make_exception_ptr(std::runtime_error(result.getError().toStdString())));
             }
           });
         })
      .observe_on(Rx::observe_on_juce_thread());
}

rxcpp::observable<std::vector<User>> AppStore::loadFeaturedProducersObservable(int limit) {
  using ResultType = std::vector<User>;
  return rxcpp::sources::create<ResultType>([this, limit](auto observer) {
           if (!networkClient) {
             Util::logError("AppStore", "Network client not initialized");
             observer.on_error(std::make_exception_ptr(std::runtime_error("Network client not initialized")));
             return;
           }

           Util::logDebug("AppStore", "Loading featured producers via observable");

           networkClient->getFeaturedProducers(limit, [observer](Outcome<juce::var> result) {
             if (result.isOk()) {
               ResultType users;
               auto usersArray = result.getValue();

               if (usersArray.isArray()) {
                 for (int i = 0; i < usersArray.size(); ++i) {
                   try {
                     auto jsonStr = juce::JSON::toString(usersArray[i]);
                     auto jsonObj = nlohmann::json::parse(jsonStr.toStdString());
                     User user;
                     from_json(jsonObj, user);
                     if (user.isValid()) {
                       users.push_back(std::move(user));
                     }
                   } catch (const std::exception &e) {
                     Util::logWarning("AppStore", "Failed to parse featured producer: " + juce::String(e.what()));
                   }
                 }
               }

               Util::logInfo("AppStore",
                             "Loaded " + juce::String(static_cast<int>(users.size())) + " featured producers");
               observer.on_next(std::move(users));
               observer.on_completed();
             } else {
               Util::logError("AppStore", "Failed to load featured producers: " + result.getError());
               observer.on_error(std::make_exception_ptr(std::runtime_error(result.getError().toStdString())));
             }
           });
         })
      .observe_on(Rx::observe_on_juce_thread());
}

rxcpp::observable<std::vector<User>> AppStore::loadSuggestedUsersObservable(int limit) {
  using ResultType = std::vector<User>;
  return rxcpp::sources::create<ResultType>([this, limit](auto observer) {
           if (!networkClient) {
             Util::logError("AppStore", "Network client not initialized");
             observer.on_error(std::make_exception_ptr(std::runtime_error("Network client not initialized")));
             return;
           }

           Util::logDebug("AppStore", "Loading suggested users via observable");

           networkClient->getSuggestedUsers(limit, [observer](Outcome<juce::var> result) {
             if (result.isOk()) {
               ResultType users;
               auto usersArray = result.getValue();

               if (usersArray.isArray()) {
                 for (int i = 0; i < usersArray.size(); ++i) {
                   try {
                     auto jsonStr = juce::JSON::toString(usersArray[i]);
                     auto jsonObj = nlohmann::json::parse(jsonStr.toStdString());
                     User user;
                     from_json(jsonObj, user);
                     if (user.isValid()) {
                       users.push_back(std::move(user));
                     }
                   } catch (const std::exception &e) {
                     Util::logWarning("AppStore", "Failed to parse suggested user: " + juce::String(e.what()));
                   }
                 }
               }

               Util::logInfo("AppStore", "Loaded " + juce::String(static_cast<int>(users.size())) + " suggested users");
               observer.on_next(std::move(users));
               observer.on_completed();
             } else {
               Util::logError("AppStore", "Failed to load suggested users: " + result.getError());
               observer.on_error(std::make_exception_ptr(std::runtime_error(result.getError().toStdString())));
             }
           });
         })
      .observe_on(Rx::observe_on_juce_thread());
}

rxcpp::observable<DiscoveryState> AppStore::loadDiscoveryDataObservable() {
  return rxcpp::sources::create<DiscoveryState>([this](auto observer) {
           // Use the slice state as accumulator, loading all data in parallel
           auto state = std::make_shared<DiscoveryState>();
           auto completedCount = std::make_shared<std::atomic<int>>(0);
           auto hasError = std::make_shared<std::atomic<bool>>(false);

           // Helper to check completion
           auto checkCompletion = [observer, state, completedCount, hasError]() {
             if (completedCount->load() >= 3 && !hasError->load()) {
               observer.on_next(*state);
               observer.on_completed();
             }
           };

           // Load trending users
           loadTrendingUsersObservable(10).subscribe(
               [state, completedCount, checkCompletion](std::vector<User> users) {
                 // Convert to shared_ptr for slice state
                 for (auto &user : users) {
                   state->trendingUsers.push_back(std::make_shared<User>(std::move(user)));
                 }
                 state->isTrendingLoading = false;
                 state->lastTrendingUpdate = juce::Time::getCurrentTime().toMilliseconds();
                 (*completedCount)++;
                 checkCompletion();
               },
               [hasError, completedCount, checkCompletion, observer](std::exception_ptr /*e*/) {
                 hasError->store(true);
                 (*completedCount)++;
                 // Continue even on error - return partial data
                 checkCompletion();
               });

           // Load featured producers
           loadFeaturedProducersObservable(10).subscribe(
               [state, completedCount, checkCompletion](std::vector<User> users) {
                 for (auto &user : users) {
                   state->featuredProducers.push_back(std::make_shared<User>(std::move(user)));
                 }
                 state->isFeaturedLoading = false;
                 state->lastFeaturedUpdate = juce::Time::getCurrentTime().toMilliseconds();
                 (*completedCount)++;
                 checkCompletion();
               },
               [hasError, completedCount, checkCompletion](std::exception_ptr) {
                 hasError->store(true);
                 (*completedCount)++;
                 checkCompletion();
               });

           // Load suggested users
           loadSuggestedUsersObservable(10).subscribe(
               [state, completedCount, checkCompletion](std::vector<User> users) {
                 for (auto &user : users) {
                   state->suggestedUsers.push_back(std::make_shared<User>(std::move(user)));
                 }
                 state->isSuggestedLoading = false;
                 state->lastSuggestedUpdate = juce::Time::getCurrentTime().toMilliseconds();
                 (*completedCount)++;
                 checkCompletion();
               },
               [hasError, completedCount, checkCompletion](std::exception_ptr) {
                 hasError->store(true);
                 (*completedCount)++;
                 checkCompletion();
               });
         })
      .observe_on(Rx::observe_on_juce_thread());
}

} // namespace Stores
} // namespace Sidechain
