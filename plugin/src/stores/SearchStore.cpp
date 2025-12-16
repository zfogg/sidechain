#include "SearchStore.h"

namespace Sidechain {
namespace Stores {

void SearchStore::searchPosts(const juce::String &query) {
  if (!networkClient) {
    updateState([](SearchState &state) { state.results.error = "Network client not initialized"; });
    return;
  }

  updateState([query](SearchState &state) {
    state.results.searchQuery = query;
    state.results.isSearching = true;
    state.results.offset = 0;
    state.results.posts.clear();
    state.results.error = "";
  });

  // TODO: Implement post search with NetworkClient
  updateState([](SearchState &state) {
    state.results.isSearching = false;
    state.results.error = "";
  });
}

void SearchStore::searchUsers(const juce::String &query) {
  if (!networkClient) {
    updateState([](SearchState &state) { state.results.error = "Network client not initialized"; });
    return;
  }

  updateState([query](SearchState &state) {
    state.results.searchQuery = query;
    state.results.isSearching = true;
    state.results.offset = 0;
    state.results.error = "";
  });

  // TODO: Implement user search with NetworkClient
  updateState([](SearchState &state) {
    state.results.isSearching = false;
    state.results.error = "";
  });
}

void SearchStore::loadMoreResults() {
  if (!networkClient) {
    updateState([](SearchState &state) { state.results.error = "Network client not initialized"; });
    return;
  }

  SearchState currentState = getState();
  if (currentState.results.searchQuery.isEmpty()) {
    return;
  }

  updateState([](SearchState &state) { state.results.isSearching = true; });

  // TODO: Implement load more results with NetworkClient
  updateState([](SearchState &state) {
    state.results.isSearching = false;
    state.results.error = "";
  });
}

void SearchStore::clearResults() {
  updateState([](SearchState &state) {
    state.results.searchQuery = "";
    state.results.posts.clear();
    state.results.users.clear();
    state.results.offset = 0;
    state.results.totalResults = 0;
    state.results.error = "";
  });
}

void SearchStore::loadGenres() {
  if (!networkClient) {
    updateState([](SearchState &state) { state.genres.error = "Network client not initialized"; });
    return;
  }

  updateState([](SearchState &state) { state.genres.isLoading = true; });

  // TODO: Implement load genres with NetworkClient
  updateState([](SearchState &state) {
    state.genres.isLoading = false;
    state.genres.error = "";
  });
}

void SearchStore::filterByGenre(const juce::String &genre) {
  searchPosts(genre);
}

} // namespace Stores
} // namespace Sidechain
