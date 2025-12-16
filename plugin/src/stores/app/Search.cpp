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

  updateState([query](AppState &state) {
    state.search.results.isSearching = true;
    state.search.results.searchQuery = query;
    state.search.results.offset = 0;
  });

  // searchPosts signature: query, genre="", bpmMin=0, bpmMax=200, key="", limit=20, offset=0, callback
  networkClient->searchPosts(query, "", 0, 200, "", 20, 0, [this, query](Outcome<juce::var> result) {
    if (result.isOk()) {
      const auto data = result.getValue();
      juce::Array<FeedPost> postsList;

      if (data.hasProperty("posts") && data.getProperty("posts", juce::var()).isArray()) {
        auto postsArray = data.getProperty("posts", juce::var());
        for (int i = 0; i < postsArray.size(); ++i) {
          postsList.add(FeedPost::fromJson(postsArray[i]));
        }
      }

      updateState([postsList, data](AppState &state) {
        state.search.results.posts = postsList;
        state.search.results.isSearching = false;
        state.search.results.totalResults = data.getProperty("total_count", postsList.size());
        state.search.results.hasMoreResults = postsList.size() < state.search.results.totalResults;
        state.search.results.offset = postsList.size();
        state.search.results.searchError = "";
        Util::logInfo("AppStore", "Search found " + juce::String(postsList.size()) +
                                      " posts for: " + state.search.results.searchQuery);
      });
    } else {
      updateState([result](AppState &state) {
        state.search.results.isSearching = false;
        state.search.results.searchError = result.getError();
        Util::logError("AppStore", "Search failed: " + result.getError());
      });
    }

    notifyObservers();
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

  updateState([query](AppState &state) {
    state.search.results.isSearching = true;
    state.search.results.searchQuery = query;
  });

  // searchUsers signature: query, limit=20, offset=0, callback
  networkClient->searchUsers(query, 20, 0, [this, query](Outcome<juce::var> result) {
    if (result.isOk()) {
      const auto data = result.getValue();
      juce::Array<juce::var> usersList;

      if (data.hasProperty("users") && data.getProperty("users", juce::var()).isArray()) {
        auto usersArray = data.getProperty("users", juce::var());
        for (int i = 0; i < usersArray.size(); ++i) {
          usersList.add(usersArray[i]);
        }
      }

      updateState([usersList, data](AppState &state) {
        state.search.results.users = usersList;
        state.search.results.isSearching = false;
        state.search.results.totalResults = data.getProperty("total_count", usersList.size());
        state.search.results.searchError = "";
        Util::logInfo("AppStore", "User search found " + juce::String(usersList.size()) + " users");
      });
    } else {
      updateState([result](AppState &state) {
        state.search.results.isSearching = false;
        state.search.results.searchError = result.getError();
      });
    }

    notifyObservers();
  });
}

void AppStore::loadMoreSearchResults() {
  const auto &currentState = getState();
  if (currentState.search.results.searchQuery.isEmpty() || !currentState.search.results.hasMoreResults) {
    return;
  }

  if (!networkClient) {
    return;
  }

  // Search for posts if there were posts in the last search
  if (!currentState.search.results.posts.isEmpty()) {
    networkClient->searchPosts(currentState.search.results.searchQuery, "", 0, 200, "", 20,
                               currentState.search.results.offset, [this](Outcome<juce::var> result) {
                                 if (result.isOk()) {
                                   const auto data = result.getValue();
                                   juce::Array<FeedPost> newPosts;

                                   if (data.hasProperty("posts") && data.getProperty("posts", juce::var()).isArray()) {
                                     auto postsArray = data.getProperty("posts", juce::var());
                                     for (int i = 0; i < postsArray.size(); ++i) {
                                       newPosts.add(FeedPost::fromJson(postsArray[i]));
                                     }
                                   }

                                   updateState([newPosts](AppState &state) {
                                     for (const auto &post : newPosts) {
                                       state.search.results.posts.add(post);
                                     }
                                     state.search.results.offset += newPosts.size();
                                   });
                                 }

                                 notifyObservers();
                               });
  }
}

void AppStore::clearSearchResults() {
  updateState([](AppState &state) {
    state.search.results.posts.clear();
    state.search.results.users.clear();
    state.search.results.searchQuery = "";
    state.search.results.isSearching = false;
    state.search.results.totalResults = 0;
    state.search.results.offset = 0;
    state.search.results.searchError = "";
    Util::logInfo("AppStore", "Search results cleared");
  });

  notifyObservers();
}

void AppStore::loadGenres() {
  if (!networkClient) {
    Util::logError("AppStore", "Cannot load genres - network client not set");
    return;
  }

  updateState([](AppState &state) { state.search.genres.isLoading = true; });

  networkClient->getAvailableGenres([this](Outcome<juce::var> result) {
    if (result.isOk()) {
      const auto data = result.getValue();
      juce::StringArray genresList;

      if (data.isArray()) {
        for (int i = 0; i < data.size(); ++i) {
          genresList.add(data[i].toString());
        }
      }

      updateState([genresList](AppState &state) {
        state.search.genres.genres = genresList;
        state.search.genres.isLoading = false;
        state.search.genres.genresError = "";
        Util::logInfo("AppStore", "Loaded " + juce::String(genresList.size()) + " genres");
      });
    } else {
      updateState([result](AppState &state) {
        state.search.genres.isLoading = false;
        state.search.genres.genresError = result.getError();
      });
    }

    notifyObservers();
  });
}

void AppStore::filterByGenre(const juce::String &genre) {
  Util::logInfo("AppStore", "Filtering by genre: " + genre);

  // This would typically update search results to filter by genre
  // For now, just log it
  // TODO: Implement genre filtering in search
}

} // namespace Stores
} // namespace Sidechain
