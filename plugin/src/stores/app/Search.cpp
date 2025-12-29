#include "../AppStore.h"
#include "../util/StoreUtils.h"
#include "../../util/logging/Logger.h"
#include "../../util/rx/JuceScheduler.h"
#include <rxcpp/rx.hpp>

namespace Sidechain {
namespace Stores {

using Utils::JsonArrayParser;
using Utils::NetworkClientGuard;

void AppStore::searchPosts(const juce::String &query) {
  if (!NetworkClientGuard::check(networkClient, "search posts")) {
    return;
  }

  if (query.isEmpty()) {
    clearSearchResults();
    return;
  }

  SearchState loadingState = stateManager.search->getState();
  loadingState.results.isSearching = true;
  loadingState.results.searchQuery = query;
  loadingState.results.offset = 0;
  stateManager.search->setState(loadingState);

  // Get current genre filter from state
  auto searchState = stateManager.search;
  auto currentGenre = searchState->getState().results.currentGenre;

  // searchPosts signature: query, genre="", bpmMin=0, bpmMax=200, key="", limit=20, offset=0, callback
  networkClient->searchPosts(query, currentGenre, 0, 200, "", 20, 0, [this, query](Outcome<juce::var> result) {
    if (result.isOk()) {
      const auto data = result.getValue();
      auto postsArray = data.getProperty("posts", juce::var());

      // Use JsonArrayParser
      auto postsList = JsonArrayParser<FeedPost>::parse(postsArray, "search posts");

      SearchState successState = stateManager.search->getState();
      successState.results.posts = std::move(postsList);
      successState.results.isSearching = false;
      successState.results.totalResults = static_cast<int>(
          data.getProperty("total_count", static_cast<juce::var>(static_cast<int>(successState.results.posts.size()))));
      successState.results.hasMoreResults =
          successState.results.posts.size() < static_cast<size_t>(successState.results.totalResults);
      successState.results.offset = static_cast<int>(successState.results.posts.size());
      successState.results.searchError = "";
      Util::logInfo("AppStore", "Search found " + juce::String(successState.results.posts.size()) +
                                    " posts for: " + successState.results.searchQuery);
      stateManager.search->setState(successState);
    } else {
      SearchState errorState = stateManager.search->getState();
      errorState.results.isSearching = false;
      errorState.results.searchError = result.getError();
      Util::logError("AppStore", "Search failed: " + result.getError());
      stateManager.search->setState(errorState);
    }
  });
}

void AppStore::searchUsers(const juce::String &query) {
  if (!NetworkClientGuard::check(networkClient, "search users")) {
    return;
  }

  if (query.isEmpty()) {
    clearSearchResults();
    return;
  }

  SearchState loadingState = stateManager.search->getState();
  loadingState.results.isSearching = true;
  loadingState.results.searchQuery = query;
  stateManager.search->setState(loadingState);

  // Use observable API
  networkClient->searchUsersObservable(query, 20).subscribe(
      [this, query](const juce::var &data) {
        auto usersArray = data.getProperty("users", juce::var());

        // Use JsonArrayParser
        auto usersList = JsonArrayParser<User>::parse(usersArray, "search users");

        SearchState successState = stateManager.search->getState();
        successState.results.users = std::move(usersList);
        successState.results.isSearching = false;
        successState.results.totalResults = static_cast<int>(data.getProperty(
            "total_count", static_cast<juce::var>(static_cast<int>(successState.results.users.size()))));
        successState.results.searchError = "";
        Util::logInfo("AppStore", "User search found " + juce::String(successState.results.users.size()) + " users");
        stateManager.search->setState(successState);
      },
      [this](std::exception_ptr ep) {
        try {
          std::rethrow_exception(ep);
        } catch (const std::exception &e) {
          SearchState errorState = stateManager.search->getState();
          errorState.results.isSearching = false;
          errorState.results.searchError = juce::String(e.what());
          stateManager.search->setState(errorState);
        }
      });
}

void AppStore::loadMoreSearchResults() {
  auto searchState = stateManager.search;
  const auto &currentState = searchState->getState();
  if (currentState.results.searchQuery.isEmpty() || !currentState.results.hasMoreResults) {
    return;
  }

  if (!NetworkClientGuard::checkSilent(networkClient)) {
    return;
  }

  // Search for posts if there were posts in the last search
  if (!currentState.results.posts.empty()) {
    networkClient->searchPosts(currentState.results.searchQuery, currentState.results.currentGenre, 0, 200, "", 20,
                               currentState.results.offset, [this](Outcome<juce::var> result) {
                                 if (result.isOk()) {
                                   const auto data = result.getValue();
                                   auto postsArray = data.getProperty("posts", juce::var());

                                   // Use JsonArrayParser
                                   auto newPosts = JsonArrayParser<FeedPost>::parse(postsArray, "search more posts");

                                   SearchState moreState = stateManager.search->getState();
                                   for (const auto &post : newPosts) {
                                     moreState.results.posts.push_back(post);
                                   }
                                   moreState.results.offset += static_cast<int>(newPosts.size());
                                   stateManager.search->setState(moreState);
                                 }
                               });
  }
}

void AppStore::clearSearchResults() {
  SearchState clearedState = stateManager.search->getState();
  clearedState.results.posts.clear();
  clearedState.results.users.clear();
  clearedState.results.searchQuery = "";
  clearedState.results.currentGenre = "";
  clearedState.results.isSearching = false;
  clearedState.results.totalResults = 0;
  clearedState.results.offset = 0;
  clearedState.results.searchError = "";
  Util::logInfo("AppStore", "Search results cleared");
  stateManager.search->setState(clearedState);
}

void AppStore::loadGenres() {
  if (!networkClient) {
    Util::logError("AppStore", "Cannot load genres - network client not set");
    return;
  }

  SearchState loadingState = stateManager.search->getState();
  loadingState.genres.isLoading = true;
  stateManager.search->setState(loadingState);

  networkClient->getAvailableGenres([this](Outcome<juce::var> result) {
    if (result.isOk()) {
      const auto data = result.getValue();
      juce::StringArray genresList;

      if (data.isArray()) {
        for (int i = 0; i < data.size(); ++i) {
          genresList.add(data[i].toString());
        }
      }

      SearchState successState = stateManager.search->getState();
      successState.genres.genres = genresList;
      successState.genres.isLoading = false;
      successState.genres.genresError = "";
      Util::logInfo("AppStore", "Loaded " + juce::String(genresList.size()) + " genres");
      stateManager.search->setState(successState);
    } else {
      SearchState errorState = stateManager.search->getState();
      errorState.genres.isLoading = false;
      errorState.genres.genresError = result.getError();
      stateManager.search->setState(errorState);
    }
  });
}

void AppStore::filterByGenre(const juce::String &genre) {
  Util::logInfo("AppStore", "Filtering by genre: " + genre);

  auto searchState = stateManager.search;
  const auto &currentState = searchState->getState();

  // If no active search query, nothing to filter
  if (currentState.results.searchQuery.isEmpty()) {
    Util::logWarning("AppStore", "No active search to filter by genre");
    return;
  }

  // Store the selected genre in state and reset pagination
  SearchState filterState = stateManager.search->getState();
  filterState.results.currentGenre = genre;
  filterState.results.offset = 0;
  filterState.results.posts.clear();
  filterState.results.totalResults = 0;
  filterState.results.hasMoreResults = false;
  Util::logInfo("AppStore", "Applied genre filter: " + genre);
  stateManager.search->setState(filterState);

  // Re-run the search with the new genre filter
  searchPosts(currentState.results.searchQuery);
}

void AppStore::autocompleteUsers(const juce::String &query,
                                 std::function<void(const juce::Array<juce::String> &suggestions)> callback) {
  if (!networkClient) {
    Util::logError("AppStore", "Cannot autocomplete users - network client not set");
    if (callback)
      callback(juce::Array<juce::String>());
    return;
  }

  if (query.isEmpty()) {
    if (callback)
      callback(juce::Array<juce::String>());
    return;
  }

  networkClient->autocompleteUsers(query, 10, [callback](Outcome<juce::var> result) {
    juce::Array<juce::String> suggestions;

    if (result.isOk()) {
      auto data = result.getValue();
      if (data.isArray()) {
        for (int i = 0; i < data.size(); ++i) {
          suggestions.add(data[i].toString());
        }
      }
      Util::logInfo("AppStore", "Autocomplete users returned " + juce::String(suggestions.size()) + " suggestions");
    } else {
      Util::logError("AppStore", "Autocomplete users failed: " + result.getError());
    }

    if (callback)
      callback(suggestions);
  });
}

void AppStore::autocompleteGenres(const juce::String &query,
                                  std::function<void(const juce::Array<juce::String> &suggestions)> callback) {
  if (!networkClient) {
    Util::logError("AppStore", "Cannot autocomplete genres - network client not set");
    if (callback)
      callback(juce::Array<juce::String>());
    return;
  }

  if (query.isEmpty()) {
    if (callback)
      callback(juce::Array<juce::String>());
    return;
  }

  networkClient->autocompleteGenres(query, 10, [callback](Outcome<juce::var> result) {
    juce::Array<juce::String> suggestions;

    if (result.isOk()) {
      auto data = result.getValue();
      if (data.isArray()) {
        for (int i = 0; i < data.size(); ++i) {
          suggestions.add(data[i].toString());
        }
      }
      Util::logInfo("AppStore", "Autocomplete genres returned " + juce::String(suggestions.size()) + " suggestions");
    } else {
      Util::logError("AppStore", "Autocomplete genres failed: " + result.getError());
    }

    if (callback)
      callback(suggestions);
  });
}

// ==============================================================================
// Search and Discovery - User Search

void AppStore::searchUsersAndCache(const juce::String &query, int limit, int offset) {
  if (!networkClient) {
    Util::logError("AppStore", "NetworkClient not set");
    return;
  }

  // Use observable API
  networkClient->searchUsersObservable(query, limit)
      .subscribe(
          [](const juce::var &data) {
            try {
              auto usersArray = data.getProperty("users", juce::var());
              if (usersArray.isArray()) {
                for (int i = 0; i < usersArray.size(); ++i) {
                  auto json = nlohmann::json::parse(usersArray[i].toString().toStdString());
                  EntityStore::getInstance().normalizeUser(json);
                }
              }
            } catch (const std::exception &e) {
              Util::logError("AppStore", "Failed to parse search results JSON: " + juce::String(e.what()));
            }
          },
          [](std::exception_ptr ep) {
            try {
              std::rethrow_exception(ep);
            } catch (const std::exception &e) {
              Util::logError("AppStore", "Failed to search users: " + juce::String(e.what()));
            }
          });
}

// ==============================================================================
// Reactive Search with Debounce (Phase 2)
//
// These observables provide debounced search functionality, reducing API calls
// when the user types quickly. Uses RxCpp operators: debounce, distinct_until_changed.

/**
 * Search posts with proper reactive pattern.
 *
 * @param query Search query string
 * @return Observable that emits search results as FeedPost model objects (copies)
 */
rxcpp::observable<std::vector<FeedPost>> AppStore::searchPostsObservable(const juce::String &query) {
  using ResultType = std::vector<FeedPost>;
  return rxcpp::sources::create<ResultType>([this, query](auto observer) {
           if (!networkClient) {
             observer.on_error(std::make_exception_ptr(std::runtime_error("Network client not initialized")));
             return;
           }

           if (query.isEmpty()) {
             observer.on_next(ResultType{});
             observer.on_completed();
             return;
           }

           auto searchState = stateManager.search;
           auto currentGenre = searchState->getState().results.currentGenre;

           networkClient->searchPosts(
               query, currentGenre, 0, 200, "", 20, 0, [observer, query](Outcome<juce::var> result) {
                 if (result.isOk()) {
                   const auto data = result.getValue();
                   auto postsArray = data.getProperty("posts", juce::var());
                   auto parsedPosts = Utils::JsonArrayParser<FeedPost>::parse(postsArray, "search posts observable");

                   // Convert shared_ptr vector to value vector
                   ResultType posts;
                   posts.reserve(parsedPosts.size());
                   for (const auto &postPtr : parsedPosts) {
                     if (postPtr) {
                       posts.push_back(*postPtr);
                     }
                   }

                   Util::logInfo("AppStore",
                                 "Search observable found " + juce::String(posts.size()) + " results for: " + query);
                   observer.on_next(std::move(posts));
                   observer.on_completed();
                 } else {
                   Util::logError("AppStore", "Search observable failed: " + result.getError());
                   observer.on_error(std::make_exception_ptr(
                       std::runtime_error("Search failed: " + result.getError().toStdString())));
                 }
               });
         })
      .observe_on(Rx::observe_on_juce_thread());
}

/**
 * Search users with proper reactive pattern.
 *
 * @param query Search query string
 * @return Observable that emits search results as User model objects (copies)
 */
rxcpp::observable<std::vector<User>> AppStore::searchUsersReactiveObservable(const juce::String &query) {
  using ResultType = std::vector<User>;

  if (!networkClient) {
    return rxcpp::sources::error<ResultType>(
        std::make_exception_ptr(std::runtime_error("Network client not initialized")));
  }

  if (query.isEmpty()) {
    return rxcpp::sources::just(ResultType{});
  }

  // Use the network client's observable API and transform the result
  return networkClient->searchUsersObservable(query, 20).map([query](const juce::var &data) {
    auto usersArray = data.getProperty("users", juce::var());
    auto parsedUsers = Utils::JsonArrayParser<User>::parse(usersArray, "search users observable");

    // Convert shared_ptr vector to value vector
    ResultType users;
    users.reserve(parsedUsers.size());
    for (const auto &userPtr : parsedUsers) {
      if (userPtr) {
        users.push_back(*userPtr);
      }
    }

    Util::logInfo("AppStore", "Search users observable found " + juce::String(users.size()) + " results for: " + query);
    return users;
  });
}

/**
 * Autocomplete users with proper reactive pattern.
 *
 * @param query Partial username to autocomplete
 * @return Observable that emits array of username suggestions
 */
rxcpp::observable<juce::Array<juce::String>> AppStore::autocompleteUsersObservable(const juce::String &query) {
  return rxcpp::sources::create<juce::Array<juce::String>>([this, query](auto observer) {
           if (!networkClient) {
             observer.on_error(std::make_exception_ptr(std::runtime_error("Network client not initialized")));
             return;
           }

           if (query.isEmpty()) {
             observer.on_next(juce::Array<juce::String>());
             observer.on_completed();
             return;
           }

           networkClient->autocompleteUsers(query, 10, [observer, query](Outcome<juce::var> result) {
             juce::Array<juce::String> suggestions;

             if (result.isOk()) {
               auto data = result.getValue();
               if (data.isArray()) {
                 for (int i = 0; i < data.size(); ++i) {
                   suggestions.add(data[i].toString());
                 }
               }
               Util::logDebug("AppStore",
                              "Autocomplete observable returned " + juce::String(suggestions.size()) + " suggestions");
               observer.on_next(suggestions);
               observer.on_completed();
             } else {
               Util::logError("AppStore", "Autocomplete observable failed: " + result.getError());
               observer.on_error(std::make_exception_ptr(
                   std::runtime_error("Autocomplete failed: " + result.getError().toStdString())));
             }
           });
         })
      .observe_on(Rx::observe_on_juce_thread());
}

/**
 * Autocomplete genres with proper reactive pattern.
 *
 * @param query Partial genre name to autocomplete
 * @return Observable that emits array of genre suggestions
 */
rxcpp::observable<juce::Array<juce::String>> AppStore::autocompleteGenresObservable(const juce::String &query) {
  return rxcpp::sources::create<juce::Array<juce::String>>([this, query](auto observer) {
           if (!networkClient) {
             observer.on_error(std::make_exception_ptr(std::runtime_error("Network client not initialized")));
             return;
           }

           if (query.isEmpty()) {
             observer.on_next(juce::Array<juce::String>());
             observer.on_completed();
             return;
           }

           networkClient->autocompleteGenres(query, 10, [observer, query](Outcome<juce::var> result) {
             juce::Array<juce::String> suggestions;

             if (result.isOk()) {
               auto data = result.getValue();
               if (data.isArray()) {
                 for (int i = 0; i < data.size(); ++i) {
                   suggestions.add(data[i].toString());
                 }
               }
               Util::logDebug("AppStore", "Genre autocomplete observable returned " + juce::String(suggestions.size()) +
                                              " suggestions");
               observer.on_next(suggestions);
               observer.on_completed();
             } else {
               Util::logError("AppStore", "Genre autocomplete observable failed: " + result.getError());
               observer.on_error(std::make_exception_ptr(
                   std::runtime_error("Genre autocomplete failed: " + result.getError().toStdString())));
             }
           });
         })
      .observe_on(Rx::observe_on_juce_thread());
}

} // namespace Stores
} // namespace Sidechain
