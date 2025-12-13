#pragma once

#include <JuceHeader.h>
#include "../../models/FeedPost.h"
#include "../feed/PostCard.h"
#include "../social/UserCard.h"  // For DiscoveredUser struct and UserCard
#include "../common/ErrorState.h"

class NetworkClient;
class StreamChatClient;

//==============================================================================
/**
 * Search provides comprehensive search functionality for users and posts
 *
 * Features:
 * - Search input with real-time results
 * - Tabbed results (Users + Posts)
 * - Filter controls (genre, BPM range, key)
 * - Recent searches (persisted locally)
 * - Trending searches section
 * - "No results" state with suggestions
 * - Keyboard navigation
 */
class Search : public juce::Component,
                        public juce::TextEditor::Listener,
                        public juce::ScrollBar::Listener,
                        public juce::KeyListener,
                        public juce::Timer
{
public:
    Search();
    ~Search() override;

    //==============================================================================
    // Network client integration
    void setNetworkClient(NetworkClient* client);
    void setCurrentUserId(const juce::String& userId) { currentUserId = userId; }
    void setStreamChatClient(StreamChatClient* client);

    // Presence updates (6.5.2.7)
    void updateUserPresence(const juce::String& userId, bool isOnline, const juce::String& status);

    //==============================================================================
    // Callbacks
    std::function<void()> onBackPressed;
    std::function<void(const juce::String& userId)> onUserSelected;  // Navigate to user profile
    std::function<void(const FeedPost& post)> onPostSelected;  // Navigate to post details

    //==============================================================================
    // Component overrides
    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;

    // TextEditor::Listener
    void textEditorTextChanged(juce::TextEditor& editor) override;
    void textEditorReturnKeyPressed(juce::TextEditor& editor) override;
    void textEditorEscapeKeyPressed(juce::TextEditor& editor) override;

    // ScrollBar::Listener
    void scrollBarMoved(juce::ScrollBar* scrollBar, double newRangeStart) override;

    // Bring Component::keyPressed into scope to avoid hiding warning
    using juce::Component::keyPressed;

    // KeyListener
    bool keyPressed(const juce::KeyPress& key, juce::Component* originatingComponent) override;

    // Timer (for debouncing search)
    void timerCallback() override;

    //==============================================================================
    // Public methods
    void focusSearchInput();
    void clearSearch();

private:
    //==============================================================================
    // Result tabs
    enum class ResultTab
    {
        Users,
        Posts
    };

    ResultTab currentTab = ResultTab::Users;

    //==============================================================================
    // Search state
    enum class SearchState
    {
        Empty,          // No search query, show trending/recent
        Searching,      // Search in progress
        Results,        // Showing search results
        NoResults,      // No results found
        Error           // Error occurred
    };

    SearchState searchState = SearchState::Empty;

    //==============================================================================
    // Data
    NetworkClient* networkClient = nullptr;
    StreamChatClient* streamChatClient = nullptr;
    juce::String currentUserId;

    // Search query and results
    juce::String currentQuery;
    juce::Array<DiscoveredUser> userResults;
    juce::Array<FeedPost> postResults;
    bool isSearching = false;
    int totalUserResults = 0;
    int totalPostResults = 0;

    // Filters
    juce::String selectedGenre;
    int bpmMin = 0;
    int bpmMax = 200;
    juce::String selectedKey;

    // Recent searches (persisted)
    juce::Array<juce::String> recentSearches;
    static constexpr int MAX_RECENT_SEARCHES = 10;

    // Trending searches (from backend)
    juce::Array<juce::String> trendingSearches;

    // Available genres for filter
    juce::Array<juce::String> availableGenres;

    //==============================================================================
    // UI Components
    std::unique_ptr<juce::TextEditor> searchInput;
    std::unique_ptr<juce::ScrollBar> scrollBar;
    juce::OwnedArray<PostCard> postCards;
    juce::OwnedArray<UserCard> userCards;
    std::unique_ptr<ErrorState> errorStateComponent;

    // Filter UI bounds
    juce::Rectangle<int> genreFilterBounds;
    juce::Rectangle<int> bpmFilterBounds;
    juce::Rectangle<int> keyFilterBounds;
    juce::Rectangle<int> usersTabBounds;
    juce::Rectangle<int> postsTabBounds;
    juce::Rectangle<int> clearButtonBounds;
    juce::Rectangle<int> backButtonBounds;

    // Scroll state
    double scrollPosition = 0.0;
    int totalContentHeight = 0;
    static constexpr int CARD_HEIGHT = 100;
    static constexpr int HEADER_HEIGHT = 120;
    static constexpr int FILTER_HEIGHT = 60;

    // Keyboard navigation state (7.3.8)
    int selectedResultIndex = -1;

    //==============================================================================
    // Helper methods
    void performSearch();
    void loadRecentSearches();
    void saveRecentSearches();
    void addToRecentSearches(const juce::String& query);
    void loadTrendingSearches();
    void useGenresAsTrendingFallback();
    void loadAvailableGenres();
    void applyFilters();
    void switchTab(ResultTab tab);
    void showGenrePicker();
    void showBPMPicker();
    void showCustomBPMDialog();
    void showKeyPicker();
    static const std::array<juce::String, 12>& getAvailableGenres();
    static const std::array<juce::String, 24>& getMusicalKeys();

    // Drawing methods
    void drawHeader(juce::Graphics& g);
    void drawFilters(juce::Graphics& g);
    void drawTabs(juce::Graphics& g);
    void drawResults(juce::Graphics& g);
    void drawEmptyState(juce::Graphics& g);
    void drawNoResultsState(juce::Graphics& g);
    void drawErrorState(juce::Graphics& g);
    void drawRecentSearches(juce::Graphics& g);
    void drawTrendingSearches(juce::Graphics& g);

    // Layout methods
    void layoutComponents();
    juce::Rectangle<int> getHeaderBounds() const;
    juce::Rectangle<int> getFilterBounds() const;
    juce::Rectangle<int> getTabBounds() const;
    juce::Rectangle<int> getResultsBounds() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Search)
};
