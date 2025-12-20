#include "../AppStore.h"
#include "../../util/logging/Logger.h"

namespace Sidechain {
namespace Stores {

void AppStore::searchPosts(const juce::String &query) {
  if (!networkClient) {
    Util::logError("AppStore", "Cannot search posts - network client not set");
    return;
  }

  if (query.isEmpty()) {
    clearSearchResults();
    return;
  }

  sliceManager.getSearchSlice()->dispatch([query](SearchState &state) {
    state.results.isSearching = true;
    state.results.searchQuery = query;
    state.results.offset = 0;
  });

  // Get current genre filter from state
  auto searchSlice = sliceManager.getSearchSlice();
  auto currentGenre = searchSlice->getState().results.currentGenre;

  // searchPosts signature: query, genre="", bpmMin=0, bpmMax=200, key="", limit=20, offset=0, callback
  networkClient->searchPosts(query, currentGenre, 0, 200, "", 20, 0, [this, query](Outcome<juce::var> result) {
    if (result.isOk()) {
      const auto data = result.getValue();
      std::vector<std::shared_ptr<Sidechain::FeedPost>> postsList;

      if (data.hasProperty("posts") && data.getProperty("posts", juce::var()).isArray()) {
        auto postsArray = data.getProperty("posts", juce::var());
        for (int i = 0; i < postsArray.size(); ++i) {
          auto post = Sidechain::FeedPost::fromJson(postsArray[i]);
          postsList.push_back(std::make_shared<Sidechain::FeedPost>(post));
        }
      }

      sliceManager.getSearchSlice()->dispatch([postsList, data](SearchState &state) {
        state.results.posts = postsList;
        state.results.isSearching = false;
        state.results.totalResults = static_cast<int>(
            data.getProperty("total_count", static_cast<juce::var>(static_cast<int>(postsList.size()))));
        state.results.hasMoreResults = postsList.size() < static_cast<size_t>(state.results.totalResults);
        state.results.offset = postsList.size();
        state.results.searchError = "";
        Util::logInfo("AppStore",
                      "Search found " + juce::String(postsList.size()) + " posts for: " + state.results.searchQuery);
      });
    } else {
      sliceManager.getSearchSlice()->dispatch([result](SearchState &state) {
        state.results.isSearching = false;
        state.results.searchError = result.getError();
        Util::logError("AppStore", "Search failed: " + result.getError());
      });
    }
  });
}

void AppStore::searchUsers(const juce::String &query) {
  if (!networkClient) {
    Util::logError("AppStore", "Cannot search users - network client not set");
    return;
  }

  if (query.isEmpty()) {
    clearSearchResults();
    return;
  }

  sliceManager.getSearchSlice()->dispatch([query](SearchState &state) {
    state.results.isSearching = true;
    state.results.searchQuery = query;
  });

  // searchUsers signature: query, limit=20, offset=0, callback
  networkClient->searchUsers(query, 20, 0, [this, query](Outcome<juce::var> result) {
    if (result.isOk()) {
      const auto data = result.getValue();
      std::vector<std::shared_ptr<Sidechain::User>> usersList;

      if (data.hasProperty("users") && data.getProperty("users", juce::var()).isArray()) {
        auto usersArray = data.getProperty("users", juce::var());
        for (int i = 0; i < usersArray.size(); ++i) {
          // TODO: Add User::fromJson implementation and parse users
          // For now, skip user parsing
        }
      }

      sliceManager.getSearchSlice()->dispatch([usersList, data](SearchState &state) {
        state.results.users = usersList;
        state.results.isSearching = false;
        state.results.totalResults = static_cast<int>(
            data.getProperty("total_count", static_cast<juce::var>(static_cast<int>(usersList.size()))));
        state.results.searchError = "";
        Util::logInfo("AppStore", "User search found " + juce::String(usersList.size()) + " users");
      });
    } else {
      sliceManager.getSearchSlice()->dispatch([result](SearchState &state) {
        state.results.isSearching = false;
        state.results.searchError = result.getError();
      });
    }
  });
}

void AppStore::loadMoreSearchResults() {
  auto searchSlice = sliceManager.getSearchSlice();
  const auto &currentState = searchSlice->getState();
  if (currentState.results.searchQuery.isEmpty() || !currentState.results.hasMoreResults) {
    return;
  }

  if (!networkClient) {
    return;
  }

  // Search for posts if there were posts in the last search
  if (!currentState.results.posts.empty()) {
    networkClient->searchPosts(currentState.results.searchQuery, currentState.results.currentGenre, 0, 200, "", 20,
                               currentState.results.offset, [this](Outcome<juce::var> result) {
                                 if (result.isOk()) {
                                   const auto data = result.getValue();
                                   std::vector<std::shared_ptr<Sidechain::FeedPost>> newPosts;

                                   if (data.hasProperty("posts") && data.getProperty("posts", juce::var()).isArray()) {
                                     auto postsArray = data.getProperty("posts", juce::var());
                                     for (int i = 0; i < postsArray.size(); ++i) {
                                       auto post = Sidechain::FeedPost::fromJson(postsArray[i]);
                                       newPosts.push_back(std::make_shared<Sidechain::FeedPost>(post));
                                     }
                                   }

                                   sliceManager.getSearchSlice()->dispatch([newPosts](SearchState &state) {
                                     for (const auto &post : newPosts) {
                                       state.results.posts.push_back(post);
                                     }
                                     state.results.offset += newPosts.size();
                                   });
                                 }
                               });
  }
}

void AppStore::clearSearchResults() {
  sliceManager.getSearchSlice()->dispatch([](SearchState &state) {
    state.results.posts.clear();
    state.results.users.clear();
    state.results.searchQuery = "";
    state.results.currentGenre = "";
    state.results.isSearching = false;
    state.results.totalResults = 0;
    state.results.offset = 0;
    state.results.searchError = "";
    Util::logInfo("AppStore", "Search results cleared");
  });
}

void AppStore::loadGenres() {
  if (!networkClient) {
    Util::logError("AppStore", "Cannot load genres - network client not set");
    return;
  }

  sliceManager.getSearchSlice()->dispatch([](SearchState &state) { state.genres.isLoading = true; });

  networkClient->getAvailableGenres([this](Outcome<juce::var> result) {
    if (result.isOk()) {
      const auto data = result.getValue();
      juce::StringArray genresList;

      if (data.isArray()) {
        for (int i = 0; i < data.size(); ++i) {
          genresList.add(data[i].toString());
        }
      }

      sliceManager.getSearchSlice()->dispatch([genresList](SearchState &state) {
        state.genres.genres = genresList;
        state.genres.isLoading = false;
        state.genres.genresError = "";
        Util::logInfo("AppStore", "Loaded " + juce::String(genresList.size()) + " genres");
      });
    } else {
      sliceManager.getSearchSlice()->dispatch([result](SearchState &state) {
        state.genres.isLoading = false;
        state.genres.genresError = result.getError();
      });
    }
  });
}

void AppStore::filterByGenre(const juce::String &genre) {
  Util::logInfo("AppStore", "Filtering by genre: " + genre);

  auto searchSlice = sliceManager.getSearchSlice();
  const auto &currentState = searchSlice->getState();

  // If no active search query, nothing to filter
  if (currentState.results.searchQuery.isEmpty()) {
    Util::logWarning("AppStore", "No active search to filter by genre");
    return;
  }

  // Store the selected genre in state and reset pagination
  sliceManager.getSearchSlice()->dispatch([genre](SearchState &state) {
    state.results.currentGenre = genre;
    state.results.offset = 0;
    state.results.posts.clear();
    state.results.totalResults = 0;
    state.results.hasMoreResults = false;
    Util::logInfo("AppStore", "Applied genre filter: " + genre);
  });

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

} // namespace Stores
} // namespace Sidechain
