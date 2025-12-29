#include "../AppStore.h"
#include "../EntityStore.h"
#include "../util/UserLoadingHelper.h"
#include "../util/StoreUtils.h"
#include "../../util/Log.h"
#include "../../util/Json.h"
#include "../../util/rx/JuceScheduler.h"
#include "../../models/User.h"
#include <rxcpp/rx.hpp>

using namespace Sidechain;
using Stores::Utils::NetworkClientGuard;
using Stores::Utils::UserLoadingHelper;

// ==============================================================================
// Discovery Redux Actions
// ==============================================================================

void AppStore::loadTrendingUsersAndCache(int limit) {
  if (!NetworkClientGuard::check(networkClient, "load trending users")) {
    return;
  }

  Log::info("AppStore", "Loading trending users");

  UserLoadingHelper<Stores::DiscoveryState>::loadUsers(
      sliceManager.discovery,
      // Set loading state
      [](Stores::DiscoveryState &s) {
        s.isTrendingLoading = true;
        s.discoveryError = "";
      },
      // Network call
      [this, limit](auto callback) { networkClient->getTrendingUsers(limit, callback); },
      // On success
      [](Stores::DiscoveryState &s, auto users) {
        s.trendingUsers = std::move(users);
        s.isTrendingLoading = false;
        s.discoveryError = "";
        s.lastTrendingUpdate = juce::Time::getCurrentTime().toMilliseconds();
      },
      // On error
      [](Stores::DiscoveryState &s, const juce::String &err) {
        s.isTrendingLoading = false;
        s.discoveryError = err;
      },
      "trending users");
}

void AppStore::loadFeaturedProducersAndCache(int limit) {
  if (!NetworkClientGuard::check(networkClient, "load featured producers")) {
    return;
  }

  Log::info("AppStore", "Loading featured producers");

  UserLoadingHelper<Stores::DiscoveryState>::loadUsers(
      sliceManager.discovery,
      // Set loading state
      [](Stores::DiscoveryState &s) {
        s.isFeaturedLoading = true;
        s.discoveryError = "";
      },
      // Network call
      [this, limit](auto callback) { networkClient->getFeaturedProducers(limit, callback); },
      // On success
      [](Stores::DiscoveryState &s, auto users) {
        s.featuredProducers = std::move(users);
        s.isFeaturedLoading = false;
        s.discoveryError = "";
        s.lastFeaturedUpdate = juce::Time::getCurrentTime().toMilliseconds();
      },
      // On error
      [](Stores::DiscoveryState &s, const juce::String &err) {
        s.isFeaturedLoading = false;
        s.discoveryError = err;
      },
      "featured producers");
}

void AppStore::loadSuggestedUsersAndCache(int limit) {
  if (!NetworkClientGuard::check(networkClient, "load suggested users")) {
    return;
  }

  Log::info("AppStore", "Loading suggested users");

  UserLoadingHelper<Stores::DiscoveryState>::loadUsers(
      sliceManager.discovery,
      // Set loading state
      [](Stores::DiscoveryState &s) {
        s.isSuggestedLoading = true;
        s.discoveryError = "";
      },
      // Network call
      [this, limit](auto callback) { networkClient->getSuggestedUsers(limit, callback); },
      // On success
      [](Stores::DiscoveryState &s, auto users) {
        s.suggestedUsers = std::move(users);
        s.isSuggestedLoading = false;
        s.discoveryError = "";
        s.lastSuggestedUpdate = juce::Time::getCurrentTime().toMilliseconds();
      },
      // On error
      [](Stores::DiscoveryState &s, const juce::String &err) {
        s.isSuggestedLoading = false;
        s.discoveryError = err;
      },
      "suggested users");
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
             Log::error("AppStore", "Network client not initialized");
             observer.on_error(std::make_exception_ptr(std::runtime_error("Network client not initialized")));
             return;
           }

           Log::debug("AppStore", "Loading trending users via observable");

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
                     Log::warning("AppStore", "Failed to parse trending user: " + juce::String(e.what()));
                   }
                 }
               }

               Log::info("AppStore", "Loaded " + juce::String(static_cast<int>(users.size())) + " trending users");
               observer.on_next(std::move(users));
               observer.on_completed();
             } else {
               Log::error("AppStore", "Failed to load trending users: " + result.getError());
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
             Log::error("AppStore", "Network client not initialized");
             observer.on_error(std::make_exception_ptr(std::runtime_error("Network client not initialized")));
             return;
           }

           Log::debug("AppStore", "Loading featured producers via observable");

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
                     Log::warning("AppStore", "Failed to parse featured producer: " + juce::String(e.what()));
                   }
                 }
               }

               Log::info("AppStore", "Loaded " + juce::String(static_cast<int>(users.size())) + " featured producers");
               observer.on_next(std::move(users));
               observer.on_completed();
             } else {
               Log::error("AppStore", "Failed to load featured producers: " + result.getError());
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
             Log::error("AppStore", "Network client not initialized");
             observer.on_error(std::make_exception_ptr(std::runtime_error("Network client not initialized")));
             return;
           }

           Log::debug("AppStore", "Loading suggested users via observable");

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
                     Log::warning("AppStore", "Failed to parse suggested user: " + juce::String(e.what()));
                   }
                 }
               }

               Log::info("AppStore", "Loaded " + juce::String(static_cast<int>(users.size())) + " suggested users");
               observer.on_next(std::move(users));
               observer.on_completed();
             } else {
               Log::error("AppStore", "Failed to load suggested users: " + result.getError());
               observer.on_error(std::make_exception_ptr(std::runtime_error(result.getError().toStdString())));
             }
           });
         })
      .observe_on(Rx::observe_on_juce_thread());
}

rxcpp::observable<Stores::DiscoveryState> AppStore::loadDiscoveryDataObservable() {
  return rxcpp::sources::create<Stores::DiscoveryState>([this](auto observer) {
           // Use the slice state as accumulator, loading all data in parallel
           auto state = std::make_shared<Stores::DiscoveryState>();
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
               [hasError, completedCount, checkCompletion, observer](std::exception_ptr e) {
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
