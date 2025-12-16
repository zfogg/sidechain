#pragma once

#include "../models/FeedPost.h"
#include "../network/NetworkClient.h"
#include "../util/logging/Logger.h"
#include "Store.h"
#include <JuceHeader.h>

namespace Sidechain {
namespace Stores {

/**
 * SearchResultsState - Immutable search results and filters
 */
struct SearchResultsState {
  // Search results
  juce::Array<FeedPost> posts;
  juce::Array<juce::var> users; // User search results

  // Search metadata
  juce::String searchQuery;
  bool isSearching = false;
  bool hasMoreResults = false;
  int totalResults = 0;
  int offset = 0;
  int limit = 20;

  // Error handling
  juce::String error;
  int64_t lastSearchTime = 0;

  bool operator==(const SearchResultsState &other) const {
    return searchQuery == other.searchQuery && isSearching == other.isSearching && totalResults == other.totalResults &&
           offset == other.offset;
  }
};

/**
 * GenresState - Available genres for filtering
 */
struct GenresState {
  juce::StringArray genres;
  bool isLoading = false;
  juce::String error;

  bool operator==(const GenresState &other) const {
    return genres == other.genres && isLoading == other.isLoading;
  }
};

/**
 * SearchState - Complete search store state
 */
struct SearchState {
  SearchResultsState results;
  GenresState genres;

  bool operator==(const SearchState &other) const {
    return results == other.results && genres == other.genres;
  }
};

/**
 * SearchStore - Reactive store for search functionality
 *
 * Handles:
 * - Text search for posts and users
 * - Genre/tag filtering
 * - Search result pagination
 * - Available genres loading
 *
 * Usage:
 *   auto& searchStore = SearchStore::getInstance();
 *   searchStore.setNetworkClient(networkClient);
 *
 *   auto unsubscribe = searchStore.subscribe([](const SearchState& state) {
 *       if (state.results.isSearching) {
 *           showLoadingSpinner();
 *       } else {
 *           displayResults(state.results.posts, state.results.users);
 *       }
 *   });
 *
 *   // Search
 *   searchStore.searchPosts("ambient");
 *   searchStore.searchUsers("producer");
 *
 *   // Load more results
 *   searchStore.loadMoreResults();
 */
class SearchStore : public Store<SearchState> {
public:
  /**
   * Get singleton instance
   */
  static SearchStore &getInstance() {
    static SearchStore instance;
    return instance;
  }

  /**
   * Set the network client for API calls
   */
  void setNetworkClient(NetworkClient *client) {
    networkClient = client;
  }

  //==============================================================================
  // Search Methods

  /**
   * Search for posts by query
   */
  void searchPosts(const juce::String &query);

  /**
   * Search for users by query
   */
  void searchUsers(const juce::String &query);

  /**
   * Load more results (pagination)
   */
  void loadMoreResults();

  /**
   * Clear search results
   */
  void clearResults();

  /**
   * Load available genres
   */
  void loadGenres();

  /**
   * Filter results by genre
   */
  void filterByGenre(const juce::String &genre);

private:
  SearchStore() : Store<SearchState>() {}

  NetworkClient *networkClient = nullptr;
};

} // namespace Stores
} // namespace Sidechain
