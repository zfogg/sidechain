#include "../AppStore.h"
#include "../util/StoreUtils.h"
#include "../../util/logging/Logger.h"

namespace Sidechain {
namespace Stores {

using Utils::JsonArrayParser;
using Utils::NetworkClientGuard;

void AppStore::searchPosts(const juce::String &query) {
  if (!NetworkClientGuard::check(networkClient.get(), "search posts")) {
    return;
  }

  if (query.isEmpty()) {
    clearSearchResults();
    return;
  }

  SearchState loadingState = sliceManager.search->getState();
  loadingState.results.isSearching = true;
  loadingState.results.searchQuery = query;
  loadingState.results.offset = 0;
  sliceManager.search->setState(loadingState);

  // Get current genre filter from state
  auto searchSlice = sliceManager.search;
  auto currentGenre = searchSlice->getState().results.currentGenre;

  // searchPosts signature: query, genre="", bpmMin=0, bpmMax=200, key="", limit=20, offset=0, callback
  networkClient->searchPosts(query, currentGenre, 0, 200, "", 20, 0, [this, query](Outcome<juce::var> result) {
    if (result.isOk()) {
      const auto data = result.getValue();
      auto postsArray = data.getProperty("posts", juce::var());

      // Use JsonArrayParser
      auto postsList = JsonArrayParser<FeedPost>::parse(postsArray, "search posts");

      SearchState successState = sliceManager.search->getState();
      successState.results.posts = std::move(postsList);
      successState.results.isSearching = false;
      successState.results.totalResults =
          static_cast<int>(data.getProperty("total_count", static_cast<juce::var>(static_cast<int>(successState.results.posts.size()))));
      successState.results.hasMoreResults = successState.results.posts.size() < static_cast<size_t>(successState.results.totalResults);
      successState.results.offset = static_cast<int>(successState.results.posts.size());
      successState.results.searchError = "";
      Util::logInfo("AppStore", "Search found " + juce::String(successState.results.posts.size()) +
                                    " posts for: " + successState.results.searchQuery);
      sliceManager.search->setState(successState);
    } else {
      SearchState errorState = sliceManager.search->getState();
      errorState.results.isSearching = false;
      errorState.results.searchError = result.getError();
      Util::logError("AppStore", "Search failed: " + result.getError());
      sliceManager.search->setState(errorState);
    }
  });
}

void AppStore::searchUsers(const juce::String &query) {
  if (!NetworkClientGuard::check(networkClient.get(), "search users")) {
    return;
  }

  if (query.isEmpty()) {
    clearSearchResults();
    return;
  }

  SearchState loadingState = sliceManager.search->getState();
  loadingState.results.isSearching = true;
  loadingState.results.searchQuery = query;
  sliceManager.search->setState(loadingState);

  // searchUsers signature: query, limit=20, offset=0, callback
  networkClient->searchUsers(query, 20, 0, [this, query](Outcome<juce::var> result) {
    if (result.isOk()) {
      const auto data = result.getValue();
      auto usersArray = data.getProperty("users", juce::var());

      // Use JsonArrayParser
      auto usersList = JsonArrayParser<User>::parse(usersArray, "search users");

      SearchState successState = sliceManager.search->getState();
      successState.results.users = std::move(usersList);
      successState.results.isSearching = false;
      successState.results.totalResults =
          static_cast<int>(data.getProperty("total_count", static_cast<juce::var>(static_cast<int>(successState.results.users.size()))));
      successState.results.searchError = "";
      Util::logInfo("AppStore", "User search found " + juce::String(successState.results.users.size()) + " users");
      sliceManager.search->setState(successState);
    } else {
      SearchState errorState = sliceManager.search->getState();
      errorState.results.isSearching = false;
      errorState.results.searchError = result.getError();
      sliceManager.search->setState(errorState);
    }
  });
}

void AppStore::loadMoreSearchResults() {
  auto searchSlice = sliceManager.search;
  const auto &currentState = searchSlice->getState();
  if (currentState.results.searchQuery.isEmpty() || !currentState.results.hasMoreResults) {
    return;
  }

  if (!NetworkClientGuard::checkSilent(networkClient.get())) {
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

                                   SearchState moreState = sliceManager.search->getState();
                                   for (const auto &post : newPosts) {
                                     moreState.results.posts.push_back(post);
                                   }
                                   moreState.results.offset += static_cast<int>(newPosts.size());
                                   sliceManager.search->setState(moreState);
                                 }
                               });
  }
}

void AppStore::clearSearchResults() {
  SearchState clearedState = sliceManager.search->getState();
  clearedState.results.posts.clear();
  clearedState.results.users.clear();
  clearedState.results.searchQuery = "";
  clearedState.results.currentGenre = "";
  clearedState.results.isSearching = false;
  clearedState.results.totalResults = 0;
  clearedState.results.offset = 0;
  clearedState.results.searchError = "";
  Util::logInfo("AppStore", "Search results cleared");
  sliceManager.search->setState(clearedState);
}

void AppStore::loadGenres() {
  if (!networkClient) {
    Util::logError("AppStore", "Cannot load genres - network client not set");
    return;
  }

  SearchState loadingState = sliceManager.search->getState();
  loadingState.genres.isLoading = true;
  sliceManager.search->setState(loadingState);

  networkClient->getAvailableGenres([this](Outcome<juce::var> result) {
    if (result.isOk()) {
      const auto data = result.getValue();
      juce::StringArray genresList;

      if (data.isArray()) {
        for (int i = 0; i < data.size(); ++i) {
          genresList.add(data[i].toString());
        }
      }

      SearchState successState = sliceManager.search->getState();
      successState.genres.genres = genresList;
      successState.genres.isLoading = false;
      successState.genres.genresError = "";
      Util::logInfo("AppStore", "Loaded " + juce::String(genresList.size()) + " genres");
      sliceManager.search->setState(successState);
    } else {
      SearchState errorState = sliceManager.search->getState();
      errorState.genres.isLoading = false;
      errorState.genres.genresError = result.getError();
      sliceManager.search->setState(errorState);
    }
  });
}

void AppStore::filterByGenre(const juce::String &genre) {
  Util::logInfo("AppStore", "Filtering by genre: " + genre);

  auto searchSlice = sliceManager.search;
  const auto &currentState = searchSlice->getState();

  // If no active search query, nothing to filter
  if (currentState.results.searchQuery.isEmpty()) {
    Util::logWarning("AppStore", "No active search to filter by genre");
    return;
  }

  // Store the selected genre in state and reset pagination
  SearchState filterState = sliceManager.search->getState();
  filterState.results.currentGenre = genre;
  filterState.results.offset = 0;
  filterState.results.posts.clear();
  filterState.results.totalResults = 0;
  filterState.results.hasMoreResults = false;
  Util::logInfo("AppStore", "Applied genre filter: " + genre);
  sliceManager.search->setState(filterState);

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

  auto &entityStore = EntityStore::getInstance();

  networkClient->searchUsers(query, limit, offset, [&entityStore](Outcome<juce::var> result) {
    if (!result.isOk()) {
      Util::logError("AppStore", "Failed to search users: " + result.getError());
      return;
    }

    try {
      auto var = result.getValue();
      if (var.isArray()) {
        for (int i = 0; i < var.size(); ++i) {
          auto json = nlohmann::json::parse(var[i].toString().toStdString());
          entityStore.normalizeUser(json);
        }
      }
    } catch (const std::exception &e) {
      Util::logError("AppStore", "Failed to parse search results JSON: " + juce::String(e.what()));
    }
  });
}

} // namespace Stores
} // namespace Sidechain
