#pragma once

#include "UserCard.h"
#include "../../stores/AppStore.h"
#include <JuceHeader.h>
#include <memory>

class NetworkClient;
class StreamChatClient;

//==============================================================================
/**
 * UserDiscovery provides user search and discovery functionality
 *
 * Features:
 * - Search bar for finding users by username
 * - Recent searches (persisted locally)
 * - Trending users section
 * - Featured producers section
 * - Suggested users based on interests
 * - Genre filtering
 */
class UserDiscovery : public juce::Component, public juce::TextEditor::Listener, public juce::ScrollBar::Listener {
public:
  UserDiscovery();
  ~UserDiscovery() override;

  //==============================================================================
  // Store and network client integration
  void setNetworkClient(NetworkClient *client);
  void setStreamChatClient(StreamChatClient *client);
  void setUserStore(std::shared_ptr<Sidechain::Stores::AppStore> store);
  void setCurrentUserId(const juce::String &userId) {
    currentUserId = userId;
  }

  // Presence updates (6.5.2.7)
  void updateUserPresence(const juce::String &userId, bool isOnline, const juce::String &status);

  //==============================================================================
  // Callbacks
  std::function<void()> onBackPressed;
  std::function<void(const DiscoveredUser &)> onUserSelected; // Navigate to profile

  //==============================================================================
  // Component overrides
  void paint(juce::Graphics &g) override;
  void resized() override;
  void mouseUp(const juce::MouseEvent &event) override;

  // TextEditor::Listener
  void textEditorTextChanged(juce::TextEditor &editor) override;
  void textEditorReturnKeyPressed(juce::TextEditor &editor) override;

  // ScrollBar::Listener
  void scrollBarMoved(juce::ScrollBar *scrollBar, double newRangeStart) override;

  //==============================================================================
  // Load initial data
  void loadDiscoveryData();
  void refresh();

private:
  //==============================================================================
  // View modes
  enum class ViewMode {
    Discovery,     // Default view with trending, featured, suggested
    SearchResults, // Showing search results
    GenreFilter    // Showing users by genre
  };

  ViewMode currentViewMode = ViewMode::Discovery;

  //==============================================================================
  // Data
  NetworkClient *networkClient = nullptr;
  StreamChatClient *streamChatClient = nullptr;
  std::shared_ptr<Sidechain::Stores::AppStore> userStore;
  std::function<void()> storeUnsubscriber;
  juce::String currentUserId;

  // Search state
  juce::String currentSearchQuery;
  juce::Array<DiscoveredUser> searchResults;
  bool isSearching = false;

  // Recent searches (persisted)
  juce::StringArray recentSearches;
  static constexpr int MAX_RECENT_SEARCHES = 10;

  // Discovery sections (fallback when store not available)
  juce::Array<DiscoveredUser> trendingUsers;
  juce::Array<DiscoveredUser> featuredProducers;
  juce::Array<DiscoveredUser> suggestedUsers;
  juce::Array<DiscoveredUser> similarProducers;
  juce::Array<DiscoveredUser> recommendedToFollow;

  // Genre filter state (local UI state)
  juce::String selectedGenre;
  juce::Array<DiscoveredUser> genreUsers;
  juce::StringArray availableGenres;

  // Loading states (fallback when store not available)
  bool isTrendingLoading = false;
  bool isFeaturedLoading = false;
  bool isSuggestedLoading = false;
  bool isSimilarLoading = false;
  bool isRecommendedLoading = false;
  bool isGenresLoading = false;

  // Error state
  juce::String errorMessage;

  //==============================================================================
  // UI Components
  std::unique_ptr<juce::TextEditor> searchBox;
  juce::ScrollBar scrollBar{true}; // vertical
  juce::OwnedArray<UserCard> userCards;

  // Scroll state
  int scrollOffset = 0;

  //==============================================================================
  // Layout constants
  static constexpr int HEADER_HEIGHT = 60;
  static constexpr int SEARCH_BAR_HEIGHT = 44;
  static constexpr int SECTION_HEADER_HEIGHT = 40;
  static constexpr int USER_CARD_HEIGHT = 72;
  static constexpr int GENRE_CHIP_HEIGHT = 32;
  static constexpr int PADDING = 16;

  //==============================================================================
  // Drawing methods
  void drawHeader(juce::Graphics &g);
  void drawSearchBar(juce::Graphics &g);
  void drawRecentSearches(juce::Graphics &g, juce::Rectangle<int> &bounds);
  void drawSectionHeader(juce::Graphics &g, juce::Rectangle<int> bounds, const juce::String &title);
  void drawTrendingSection(juce::Graphics &g, juce::Rectangle<int> &bounds);
  void drawFeaturedSection(juce::Graphics &g, juce::Rectangle<int> &bounds);
  void drawSuggestedSection(juce::Graphics &g, juce::Rectangle<int> &bounds);
  void drawSimilarSection(juce::Graphics &g, juce::Rectangle<int> &bounds);
  void drawRecommendedSection(juce::Graphics &g, juce::Rectangle<int> &bounds);
  void drawGenreChips(juce::Graphics &g, juce::Rectangle<int> &bounds, const juce::StringArray &availableGenres);
  void drawSearchResults(juce::Graphics &g, juce::Rectangle<int> bounds);
  void drawLoadingState(juce::Graphics &g, juce::Rectangle<int> bounds);
  void drawEmptyState(juce::Graphics &g, juce::Rectangle<int> bounds, const juce::String &message);

  //==============================================================================
  // Hit testing helpers
  juce::Rectangle<int> getBackButtonBounds() const;
  juce::Rectangle<int> getSearchBoxBounds() const;
  juce::Rectangle<int> getClearSearchBounds() const;
  juce::Rectangle<int> getContentBounds() const;
  juce::Rectangle<int> getRecentSearchBounds(int index) const;
  juce::Rectangle<int> getGenreChipBounds(int index) const;

  //==============================================================================
  // Network operations
  void performSearch(const juce::String &query);
  void fetchTrendingUsers();
  void fetchFeaturedProducers();
  void fetchSuggestedUsers();
  void fetchSimilarProducers();
  void fetchRecommendedToFollow();
  void fetchAvailableGenres();
  void fetchUsersByGenre(const juce::String &genre);
  void handleFollowToggle(const DiscoveredUser &user, bool willFollow);

  //==============================================================================
  // Recent searches management
  void loadRecentSearches();
  void saveRecentSearches();
  void addToRecentSearches(const juce::String &query);
  void clearRecentSearches();

  //==============================================================================
  // User card management
  void rebuildUserCards();
  void updateUserCardPositions();
  void setupUserCardCallbacks(UserCard *card);

  // Presence querying
  void queryPresenceForUsers(const juce::Array<DiscoveredUser> &users);

  //==============================================================================
  // Helper methods
  int calculateContentHeight() const;
  void updateScrollBounds();
  juce::File getRecentSearchesFile() const;

  //==============================================================================
  // Colors (matching app theme)
  struct Colors {
    static inline juce::Colour background{0xff1a1a1e};
    static inline juce::Colour headerBg{0xff252529};
    static inline juce::Colour searchBg{0xff2d2d32};
    static inline juce::Colour cardBg{0xff2d2d32};
    static inline juce::Colour textPrimary{0xffffffff};
    static inline juce::Colour textSecondary{0xffa0a0a0};
    static inline juce::Colour textPlaceholder{0xff6a6a6a};
    static inline juce::Colour accent{0xff00d4ff};
    static inline juce::Colour chipBg{0xff3a3a3e};
    static inline juce::Colour chipSelected{0xff00d4ff};
    static inline juce::Colour sectionHeader{0xff8a8a8a};
  };

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UserDiscovery)
};
