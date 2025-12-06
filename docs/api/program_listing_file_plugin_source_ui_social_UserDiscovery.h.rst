
.. _program_listing_file_plugin_source_ui_social_UserDiscovery.h:

Program Listing for File UserDiscovery.h
========================================

|exhale_lsh| :ref:`Return to documentation for file <file_plugin_source_ui_social_UserDiscovery.h>` (``plugin/source/ui/social/UserDiscovery.h``)

.. |exhale_lsh| unicode:: U+021B0 .. UPWARDS ARROW WITH TIP LEFTWARDS

.. code-block:: cpp

   #pragma once
   
   #include <JuceHeader.h>
   #include "UserCard.h"
   
   class NetworkClient;
   
   //==============================================================================
   class UserDiscovery : public juce::Component,
                                   public juce::TextEditor::Listener,
                                   public juce::ScrollBar::Listener
   {
   public:
       UserDiscovery();
       ~UserDiscovery() override;
   
       //==============================================================================
       // Network client integration
       void setNetworkClient(NetworkClient* client);
       void setCurrentUserId(const juce::String& userId) { currentUserId = userId; }
   
       //==============================================================================
       // Callbacks
       std::function<void()> onBackPressed;
       std::function<void(const DiscoveredUser&)> onUserSelected;  // Navigate to profile
   
       //==============================================================================
       // Component overrides
       void paint(juce::Graphics& g) override;
       void resized() override;
       void mouseUp(const juce::MouseEvent& event) override;
   
       // TextEditor::Listener
       void textEditorTextChanged(juce::TextEditor& editor) override;
       void textEditorReturnKeyPressed(juce::TextEditor& editor) override;
   
       // ScrollBar::Listener
       void scrollBarMoved(juce::ScrollBar* scrollBar, double newRangeStart) override;
   
       //==============================================================================
       // Load initial data
       void loadDiscoveryData();
       void refresh();
   
   private:
       //==============================================================================
       // View modes
       enum class ViewMode
       {
           Discovery,      // Default view with trending, featured, suggested
           SearchResults,  // Showing search results
           GenreFilter     // Showing users by genre
       };
   
       ViewMode currentViewMode = ViewMode::Discovery;
   
       //==============================================================================
       // Data
       NetworkClient* networkClient = nullptr;
       juce::String currentUserId;
   
       // Search state
       juce::String currentSearchQuery;
       juce::Array<DiscoveredUser> searchResults;
       bool isSearching = false;
   
       // Discovery sections
       juce::Array<DiscoveredUser> trendingUsers;
       juce::Array<DiscoveredUser> featuredProducers;
       juce::Array<DiscoveredUser> suggestedUsers;
   
       // Recent searches (persisted)
       juce::StringArray recentSearches;
       static constexpr int MAX_RECENT_SEARCHES = 10;
   
       // Genre filter state
       juce::StringArray availableGenres;
       juce::String selectedGenre;
       juce::Array<DiscoveredUser> genreUsers;
   
       // Loading states
       bool isTrendingLoading = true;
       bool isFeaturedLoading = true;
       bool isSuggestedLoading = true;
       bool isGenresLoading = true;
   
       // Error state
       juce::String errorMessage;
   
       //==============================================================================
       // UI Components
       std::unique_ptr<juce::TextEditor> searchBox;
       juce::ScrollBar scrollBar { true };  // vertical
       juce::OwnedArray<UserCardComponent> userCards;
   
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
       void drawHeader(juce::Graphics& g);
       void drawSearchBar(juce::Graphics& g);
       void drawRecentSearches(juce::Graphics& g, juce::Rectangle<int>& bounds);
       void drawSectionHeader(juce::Graphics& g, juce::Rectangle<int> bounds, const juce::String& title);
       void drawTrendingSection(juce::Graphics& g, juce::Rectangle<int>& bounds);
       void drawFeaturedSection(juce::Graphics& g, juce::Rectangle<int>& bounds);
       void drawSuggestedSection(juce::Graphics& g, juce::Rectangle<int>& bounds);
       void drawGenreChips(juce::Graphics& g, juce::Rectangle<int>& bounds);
       void drawSearchResults(juce::Graphics& g, juce::Rectangle<int> bounds);
       void drawLoadingState(juce::Graphics& g, juce::Rectangle<int> bounds);
       void drawEmptyState(juce::Graphics& g, juce::Rectangle<int> bounds, const juce::String& message);
   
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
       void performSearch(const juce::String& query);
       void fetchTrendingUsers();
       void fetchFeaturedProducers();
       void fetchSuggestedUsers();
       void fetchAvailableGenres();
       void fetchUsersByGenre(const juce::String& genre);
       void handleFollowToggle(const DiscoveredUser& user, bool willFollow);
   
       //==============================================================================
       // Recent searches management
       void loadRecentSearches();
       void saveRecentSearches();
       void addToRecentSearches(const juce::String& query);
       void clearRecentSearches();
   
       //==============================================================================
       // User card management
       void rebuildUserCards();
       void updateUserCardPositions();
       void setupUserCardCallbacks(UserCardComponent* card);
   
       //==============================================================================
       // Helper methods
       int calculateContentHeight() const;
       void updateScrollBounds();
       juce::File getRecentSearchesFile() const;
   
       //==============================================================================
       // Colors (matching app theme)
       struct Colors
       {
           static inline juce::Colour background { 0xff1a1a1e };
           static inline juce::Colour headerBg { 0xff252529 };
           static inline juce::Colour searchBg { 0xff2d2d32 };
           static inline juce::Colour cardBg { 0xff2d2d32 };
           static inline juce::Colour textPrimary { 0xffffffff };
           static inline juce::Colour textSecondary { 0xffa0a0a0 };
           static inline juce::Colour textPlaceholder { 0xff6a6a6a };
           static inline juce::Colour accent { 0xff00d4ff };
           static inline juce::Colour chipBg { 0xff3a3a3e };
           static inline juce::Colour chipSelected { 0xff00d4ff };
           static inline juce::Colour sectionHeader { 0xff8a8a8a };
       };
   
       JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UserDiscovery)
   };
