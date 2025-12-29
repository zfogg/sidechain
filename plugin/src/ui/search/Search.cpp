#include "Search.h"
#include "../../network/StreamChatClient.h"
#include "../../util/Colors.h"
#include "../../util/Constants.h"
#include "../../util/Log.h"
#include "../../util/Time.h"
#include "../../util/rx/JuceScheduler.h"
#include "../social/UserCard.h"
#include <fstream>

// ==============================================================================
Search::Search(Sidechain::Stores::AppStore *store)
    : AppStoreComponent(
          store, [store](auto cb) { return store ? store->subscribeToSearch(cb) : std::function<void()>([]() {}); }) {
  Log::info("Search: Initializing");

  // Create search input
  searchInput = std::make_unique<juce::TextEditor>();
  searchInput->setMultiLine(false);
  searchInput->setReturnKeyStartsNewLine(false);
  searchInput->setReadOnly(false);
  searchInput->setScrollbarsShown(false);
  searchInput->setCaretVisible(true);
  searchInput->setPopupMenuEnabled(true);
  searchInput->setTextToShowWhenEmpty("Search users and posts...", SidechainColors::textMuted());
  searchInput->setFont(juce::Font(juce::FontOptions().withHeight(16.0f)));
  searchInput->addListener(this);
  addAndMakeVisible(searchInput.get());

  // Create scrollbar (vertical)
  scrollBar = std::make_unique<juce::ScrollBar>(true);
  scrollBar->addListener(this);
  addAndMakeVisible(scrollBar.get());

  // Load recent searches
  loadRecentSearches();
  loadTrendingSearches(); // Load trending searches
  loadAvailableGenres();

  // Setup RxCpp debounced search instead of timer-based debounce
  setupDebouncedSearch();

  // Create error state component (initially hidden)
  errorStateComponent = std::make_unique<ErrorState>();
  errorStateComponent->setErrorType(ErrorState::ErrorType::Network);
  errorStateComponent->setPrimaryAction("Try Again", [this]() {
    Log::info("Search: Retry requested from error state");
    performSearch();
  });
  errorStateComponent->setSecondaryAction("Clear Search", [this]() {
    Log::info("Search: Clear search requested from error state");
    clearSearch();
  });
  addChildComponent(errorStateComponent.get());
  Log::debug("Search: Error state component created");

  // Set size after all components are initialized to avoid calling
  // layoutComponents before scrollBar exists
  setSize(1000, 700);

  // Subscribe to AppStore after UI setup

  // Phase 7 features:
  // - Advanced Search - Search by BPM, key, genre
  // - Filter posts by metadata (BPM, key, genre, DAW)
  // - Search hashtags
}

Search::~Search() {
  // Unsubscribe from RxCpp debounced search
  searchSubscription_.unsubscribe();
  // RAII: juce::Array will clean up automatically
  // AppStoreComponent destructor will handle unsubscribe
}

// ==============================================================================
// AppStoreComponent virtual methods

void Search::onAppStateChanged(const Sidechain::Stores::SearchState &state) {
  // Update search results from store
  isSearching = state.results.isSearching;

  // Update user results
  userResults.clear();
  for (const auto &userPtr : state.results.users) {
    if (userPtr) {
      DiscoveredUser user;
      user.id = userPtr->id;
      user.username = userPtr->username;
      user.displayName = userPtr->displayName;
      user.bio = userPtr->bio;
      user.avatarUrl = userPtr->avatarUrl;
      user.genre = userPtr->genre;
      user.followerCount = userPtr->followerCount;
      user.isFollowing = userPtr->isFollowing;
      userResults.add(user);
    }
  }
  totalUserResults = state.results.totalResults;

  // Update post results
  postResults.clear();
  for (const auto &post : state.results.posts) {
    if (post) {
      postResults.add(*post);
    }
  }
  totalPostResults = state.results.totalResults;

  // Update available genres from store
  availableGenres.clear();
  const auto &stateGenres = state.genres.genres;
  for (int i = 0; i < stateGenres.size(); ++i) {
    availableGenres.add(stateGenres[i]);
  }
  if (!availableGenres.isEmpty()) {
    // If genres were just loaded, update trending searches
    loadTrendingSearches();
  }

  // Update search state based on results
  if (currentQuery.isEmpty()) {
    searchState = SearchState::Empty;
  } else if (isSearching) {
    searchState = SearchState::Searching;
  } else if (state.results.searchError.isNotEmpty()) {
    searchState = SearchState::Error;
    if (errorStateComponent) {
      errorStateComponent->configureFromError(state.results.searchError);
      errorStateComponent->setVisible(true);
    }
  } else if (userResults.isEmpty() && postResults.isEmpty()) {
    searchState = SearchState::NoResults;
  } else {
    searchState = SearchState::Results;
    if (errorStateComponent) {
      errorStateComponent->setVisible(false);
    }
  }

  Log::debug("Search: Store state changed - " + juce::String(userResults.size()) + " users, " +
             juce::String(postResults.size()) + " posts, " + juce::String(availableGenres.size()) + " genres");
  repaint();
}

// ==============================================================================
void Search::setStreamChatClient(StreamChatClient *client) {
  streamChatClient = client;
  Log::info("Search::setStreamChatClient: StreamChatClient set " +
            juce::String(client != nullptr ? "(valid)" : "(null)"));
}

void Search::updateUserPresence(const juce::String &userId, bool isOnline, const juce::String &status) {
  if (userId.isEmpty())
    return;

  bool isInStudio = (status == "in_studio" || status == "in studio" || status == "recording");

  // Update presence in user results
  for (auto &user : userResults) {
    if (user.id == userId) {
      user.isOnline = isOnline;
      user.isInStudio = isInStudio;

      // Update corresponding UserCard
      for (auto *card : userCards) {
        if (card->getUser().id == userId) {
          auto updatedUser = card->getUser();
          updatedUser.isOnline = isOnline;
          updatedUser.isInStudio = isInStudio;
          card->setUser(updatedUser);
          break;
        }
      }
      break;
    }
  }

  // Update presence in post results (for post authors)
  for (auto &post : postResults) {
    if (post.userId == userId) {
      post.isOnline = isOnline;
      post.isInStudio = isInStudio;

      // Update corresponding PostCard
      for (auto *card : postCards) {
        auto cardPost = card->getPost();
        if (cardPost && cardPost->userId == userId) {
          // Create a shared_ptr copy with modifications
          auto updatedPost = std::make_shared<Sidechain::FeedPost>(*cardPost);
          updatedPost->isOnline = isOnline;
          updatedPost->isInStudio = isInStudio;
          card->setPost(updatedPost);
          break;
        }
      }
    }
  }

  // Repaint to show updated online indicators
  repaint();
}

// ==============================================================================
void Search::paint(juce::Graphics &g) {
  auto bounds = getLocalBounds();

  // Background
  g.setColour(SidechainColors::background());
  g.fillRect(bounds);

  // Draw sections based on state
  drawHeader(g);
  drawTabs(g);

  if (searchState == SearchState::Empty) {
    drawEmptyState(g);
  } else if (searchState == SearchState::Searching) {
    g.setColour(SidechainColors::textMuted());
    g.setFont(16.0f);
    g.drawText("Searching...", getResultsBounds(), juce::Justification::centred);
  } else if (searchState == SearchState::NoResults) {
    drawNoResultsState(g);
  } else if (searchState == SearchState::Error) {
    // ErrorState component handles the error UI as a child component
    // Just draw the header/tabs and let the component render in the results
    // area
  } else if (searchState == SearchState::Results) {
    drawResults(g);
  }

  // Draw filters if we have a query
  if (!currentQuery.isEmpty()) {
    drawFilters(g);
  }
}

void Search::resized() {
  layoutComponents();
}

void Search::mouseUp(const juce::MouseEvent &event) {
  auto pos = event.getPosition();

  // Handle tab clicks
  if (usersTabBounds.contains(pos) && currentTab != ResultTab::Users) {
    switchTab(ResultTab::Users);
    repaint();
  } else if (postsTabBounds.contains(pos) && currentTab != ResultTab::Posts) {
    switchTab(ResultTab::Posts);
    repaint();
  }

  // Handle back button
  if (backButtonBounds.contains(pos)) {
    if (onBackPressed)
      onBackPressed();
  }

  // Handle clear button
  if (clearButtonBounds.contains(pos) && !currentQuery.isEmpty()) {
    clearSearch();
  }

  // Handle filter clicks (genre, BPM, key)
  if (genreFilterBounds.contains(pos)) {
    showGenrePicker();
    return;
  }
  if (bpmFilterBounds.contains(pos)) {
    showBPMPicker();
    return;
  }
  if (keyFilterBounds.contains(pos)) {
    showKeyPicker();
    return;
  }

  // Handle recent/trending search clicks
  if (searchState == SearchState::Empty) {
    // Check if clicked on recent search item
    int yOffset = HEADER_HEIGHT + FILTER_HEIGHT + 40;
    for (int i = 0; i < static_cast<int>(recentSearches.size()) && i < 5; ++i) {
      juce::Rectangle<int> itemBounds(20, yOffset + i * 40, getWidth() - 40, 35);
      if (itemBounds.contains(pos)) {
        searchInput->setText(recentSearches[i]);
        currentQuery = recentSearches[i];
        performSearch();
        return;
      }
    }

    // Check if clicked on trending search item
    yOffset = HEADER_HEIGHT + FILTER_HEIGHT + 40 +
              (static_cast<int>(recentSearches.size()) > 0 ? (static_cast<int>(recentSearches.size()) * 40 + 40) : 0);
    for (int i = 0; i < static_cast<int>(trendingSearches.size()) && i < 5; ++i) {
      juce::Rectangle<int> itemBounds(20, yOffset + i * 40, getWidth() - 40, 35);
      if (itemBounds.contains(pos)) {
        searchInput->setText(trendingSearches[i]);
        currentQuery = trendingSearches[i];
        performSearch();
        return;
      }
    }
  }
}

void Search::mouseWheelMove(const juce::MouseEvent &event, const juce::MouseWheelDetails &wheel) {
  // Only scroll results if wheel is within results area (not search input)
  if (scrollBar->isVisible() && event.y > SEARCH_INPUT_HEIGHT) {
    double newPos = scrollPosition - wheel.deltaY * 30.0;
    newPos =
        juce::jmax(0.0, juce::jmin(newPos, static_cast<double>(totalContentHeight - getResultsBounds().getHeight())));
    scrollPosition = newPos;
    scrollBar->setCurrentRangeStart(newPos);
    repaint();
  }
}

// ==============================================================================
void Search::textEditorTextChanged(juce::TextEditor &editor) {
  if (&editor == searchInput.get()) {
    juce::String newQuery = editor.getText().trim();

    if (newQuery != currentQuery) {
      currentQuery = newQuery;

      if (currentQuery.isEmpty()) {
        searchState = SearchState::Empty;
        userResults.clear();
        postResults.clear();
        selectedResultIndex = -1; // Reset keyboard navigation
        repaint();
      } else {
        // Push query to RxCpp subject for debounced search
        querySubject_.get_subscriber().on_next(newQuery);
      }
    }
  }
}

void Search::textEditorReturnKeyPressed(juce::TextEditor &editor) {
  if (&editor == searchInput.get()) {
    performSearch();
  }
}

void Search::textEditorEscapeKeyPressed(juce::TextEditor &editor) {
  if (&editor == searchInput.get()) {
    if (onBackPressed)
      onBackPressed();
  }
}

// ==============================================================================
void Search::scrollBarMoved(juce::ScrollBar *movedScrollBar, double newRangeStart) {
  if (movedScrollBar == this->scrollBar.get()) {
    scrollPosition = newRangeStart;
    repaint();
  }
}

// ==============================================================================
bool Search::keyPressed(const juce::KeyPress &key, juce::Component *originatingComponent) {
  // Only handle keyboard if search component has focus or originated from here
  if (originatingComponent != nullptr && originatingComponent != this &&
      (searchInput == nullptr || originatingComponent != searchInput.get())) {
    return false; // Let other components handle their own keyboard events
  }

  // Tab key to switch between Users/Posts
  if (key.getKeyCode() == juce::KeyPress::tabKey) {
    switchTab(currentTab == ResultTab::Users ? ResultTab::Posts : ResultTab::Users);
    return true;
  }

  // Up/Down arrows for navigation
  if (searchState == SearchState::Results) {
    int maxResults =
        (currentTab == ResultTab::Users) ? static_cast<int>(userResults.size()) : static_cast<int>(postResults.size());

    if (key.getKeyCode() == juce::KeyPress::downKey) {
      if (selectedResultIndex < maxResults - 1) {
        selectedResultIndex++;
        // Auto-scroll to keep selected item visible
        auto bounds = getResultsBounds();
        int itemY = HEADER_HEIGHT + FILTER_HEIGHT + 40 + (selectedResultIndex * CARD_HEIGHT);
        if (itemY + CARD_HEIGHT > bounds.getBottom()) {
          scrollPosition = static_cast<double>((selectedResultIndex + 1) * CARD_HEIGHT - bounds.getHeight());
          scrollBar->setCurrentRangeStart(scrollPosition);
        }
        repaint();
      }
      return true;
    } else if (key.getKeyCode() == juce::KeyPress::upKey) {
      if (selectedResultIndex > 0) {
        selectedResultIndex--;
        // Auto-scroll to keep selected item visible
        auto bounds = getResultsBounds();
        int itemY = HEADER_HEIGHT + FILTER_HEIGHT + 40 + (selectedResultIndex * CARD_HEIGHT);
        if (itemY < bounds.getY()) {
          scrollPosition = static_cast<double>(selectedResultIndex * CARD_HEIGHT);
          scrollBar->setCurrentRangeStart(scrollPosition);
        }
        repaint();
      }
      return true;
    } else if (key.getKeyCode() == juce::KeyPress::returnKey) {
      // Select the highlighted item
      if (selectedResultIndex >= 0 && selectedResultIndex < maxResults) {
        if (currentTab == ResultTab::Users && selectedResultIndex < static_cast<int>(userResults.size())) {
          if (onUserSelected)
            onUserSelected(userResults[selectedResultIndex].id);
        } else if (currentTab == ResultTab::Posts && selectedResultIndex < static_cast<int>(postResults.size())) {
          if (onPostSelected)
            onPostSelected(postResults[selectedResultIndex]);
        }
      }
      return true;
    }
  }

  return false;
}

// ==============================================================================
void Search::setupDebouncedSearch() {
  Log::info("Search: Setting up RxCpp debounced search pipeline");

  searchSubscription_ = rxcpp::composite_subscription();
  juce::Component::SafePointer<Search> safeThis(this);

  // Use RxCpp debounce operator with 300ms delay
  querySubject_.get_observable()
      .debounce(std::chrono::milliseconds(300), Sidechain::Rx::observe_on_juce_thread())
      .distinct_until_changed()
      .subscribe(
          searchSubscription_,
          [safeThis](const juce::String &query) {
            if (!safeThis)
              return;

            Log::debug("Search: Debounced query triggered: " + query);
            if (!query.isEmpty()) {
              safeThis->performSearch();
            }
          },
          [](std::exception_ptr) { Log::warn("Search: Debounced search error"); });
}

// ==============================================================================
void Search::focusSearchInput() {
  searchInput->grabKeyboardFocus();
  searchInput->selectAll();
}

void Search::clearSearch() {
  searchInput->clear();
  currentQuery.clear();
  searchState = SearchState::Empty;
  userResults.clear();
  postResults.clear();
  selectedResultIndex = -1; // Reset keyboard navigation
  repaint();
}

// ==============================================================================
void Search::performSearch() {
  if (currentQuery.isEmpty()) {
    Log::warn("Search: Cannot perform search - query empty");
    return;
  }

  if (!appStore) {
    Log::warn("Search: Cannot perform search - AppStore is null");
    return;
  }

  Log::info("Search: Performing search - query: \"" + currentQuery +
            "\", tab: " + (currentTab == ResultTab::Users ? "Users" : "Posts"));

  isSearching = true;
  searchState = SearchState::Searching;
  selectedResultIndex = -1; // Reset keyboard navigation
  repaint();

  // Add to recent searches
  addToRecentSearches(currentQuery);

  // Perform search through AppStore based on current tab
  // State updates will come through onAppStateChanged subscription
  if (currentTab == ResultTab::Users) {
    // Use reactive observable for user search (with caching)
    juce::Component::SafePointer<Search> safeThis(this);
    appStore->searchUsersObservable(currentQuery)
        .subscribe(
            [safeThis](const juce::Array<juce::var> &users) {
              if (safeThis == nullptr)
                return;
              Log::debug("Search: User search completed with " + juce::String(users.size()) + " results");
            },
            [safeThis](std::exception_ptr) {
              if (safeThis == nullptr)
                return;
              Log::error("Search: User search failed");
            });
  } else // Posts tab
  {
    appStore->searchPosts(currentQuery);
  }

  // Note: Results and state updates will be delivered via onAppStateChanged callback
}

void Search::loadRecentSearches() {
  // Load from file: ~/.local/share/Sidechain/recent_searches.txt
  juce::File searchDir =
      juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("Sidechain");
  if (!searchDir.exists())
    searchDir.createDirectory();

  juce::File searchFile = searchDir.getChildFile("recent_searches.txt");
  if (searchFile.existsAsFile()) {
    juce::StringArray lines;
    searchFile.readLines(lines);
    recentSearches.clear();
    for (int i = 0; i < static_cast<int>(lines.size()) && i < MAX_RECENT_SEARCHES; ++i) {
      if (!lines[i].trim().isEmpty())
        recentSearches.add(lines[i].trim());
    }
  }
}

void Search::saveRecentSearches() {
  juce::File searchDir =
      juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("Sidechain");
  if (!searchDir.exists())
    searchDir.createDirectory();

  juce::File searchFile = searchDir.getChildFile("recent_searches.txt");
  juce::StringArray lines;
  for (const auto &search : recentSearches) {
    lines.add(search);
  }
  searchFile.replaceWithText(lines.joinIntoString("\n"));
}

void Search::addToRecentSearches(const juce::String &query) {
  // Remove if already exists
  recentSearches.removeAllInstancesOf(query);

  // Add to front
  recentSearches.insert(0, query);

  // Limit size
  while (static_cast<int>(recentSearches.size()) > MAX_RECENT_SEARCHES)
    recentSearches.removeLast();

  saveRecentSearches();
}

void Search::loadTrendingSearches() {
  // Load trending search terms via available genres or fallback
  if (availableGenres.isEmpty()) {
    // Use hardcoded fallback if genres not loaded yet
    trendingSearches = {"electronic", "hip-hop", "techno",    "house", "trap",
                        "ambient",    "lofi",    "synthwave", "dnb",   "jungle"};
  } else {
    // Use available genres as trending searches
    trendingSearches.clear();
    // Add top 10 genres as trending searches
    for (int i = 0; i < juce::jmin(10, availableGenres.size()); ++i) {
      trendingSearches.add(availableGenres[i].toLowerCase());
    }
  }

  Log::info("Search::loadTrendingSearches: Loaded " + juce::String(trendingSearches.size()) + " trending searches");
}

void Search::useGenresAsTrendingFallback() {
  // Use available genres as trending searches fallback
  trendingSearches.clear();
  if (static_cast<int>(availableGenres.size()) > 0) {
    for (int i = 0; i < juce::jmin(5, static_cast<int>(availableGenres.size())); ++i) {
      trendingSearches.add(availableGenres[i]);
    }
  } else {
    // Final fallback to hardcoded genres
    trendingSearches = {"electronic", "hip-hop", "techno", "house", "trap"};
  }
  juce::MessageManager::callAsync([this]() { repaint(); });
}

void Search::loadAvailableGenres() {
  // Load available genres via AppStore
  if (!appStore) {
    Log::warn("Search::loadAvailableGenres: AppStore not set");
    // Fallback to hardcoded genres
    availableGenres = {"Electronic", "Hip-Hop", "House", "Techno", "Ambient",
                       "Trap",       "Dubstep", "DNB",   "Jungle", "Lofi"};
    return;
  }

  Log::info("Search::loadAvailableGenres: Loading genres from AppStore");
  appStore->loadGenres();
  // Genres will be synced via onAppStateChanged callback
}

void Search::applyFilters() {
  if (!currentQuery.isEmpty()) {
    performSearch();
  }
}

void Search::switchTab(ResultTab tab) {
  currentTab = tab;
  selectedResultIndex = -1; // Reset keyboard navigation when switching tabs

  // If we have a query, search in the new tab
  if (!currentQuery.isEmpty()) {
    performSearch();
  }

  repaint();
}

// ==============================================================================
void Search::drawHeader(juce::Graphics &g) {
  auto bounds = getHeaderBounds();

  // Back button
  backButtonBounds = bounds.removeFromLeft(50).reduced(10);
  g.setColour(SidechainColors::textPrimary());
  g.setFont(20.0f);
  g.drawText(juce::CharPointer_UTF8("\xe2\x86\x90"), backButtonBounds,
             juce::Justification::centred); // ←

  // Search input bounds
  auto searchBounds = bounds.removeFromLeft(bounds.getWidth() - 60).reduced(10, 5);
  searchInput->setBounds(searchBounds);

  // Clear button (X) if there's text
  if (!currentQuery.isEmpty()) {
    clearButtonBounds = bounds.removeFromLeft(40).reduced(10);
    g.setColour(SidechainColors::textMuted());
    g.setFont(18.0f);
    g.drawText(juce::CharPointer_UTF8("\xc3\x97"), clearButtonBounds,
               juce::Justification::centred); // ×
  }
}

void Search::drawFilters(juce::Graphics &g) {
  auto bounds = getFilterBounds();
  int filterWidth = bounds.getWidth() / 3;

  // Genre filter
  genreFilterBounds = bounds.removeFromLeft(filterWidth).reduced(5);
  g.setColour(SidechainColors::surface());
  g.fillRoundedRectangle(genreFilterBounds.toFloat(), 6.0f);
  g.setColour(SidechainColors::border());
  g.drawRoundedRectangle(genreFilterBounds.toFloat(), 6.0f, 1.0f);
  g.setColour(SidechainColors::textPrimary());
  g.setFont(12.0f);
  juce::String genreText = selectedGenre.isEmpty() ? "All Genres" : selectedGenre;
  g.drawText(genreText, genreFilterBounds.reduced(10, 5), juce::Justification::centredLeft);

  // BPM filter
  bpmFilterBounds = bounds.removeFromLeft(filterWidth).reduced(5);
  g.setColour(SidechainColors::surface());
  g.fillRoundedRectangle(bpmFilterBounds.toFloat(), 6.0f);
  g.setColour(SidechainColors::border());
  g.drawRoundedRectangle(bpmFilterBounds.toFloat(), 6.0f, 1.0f);
  g.setColour(SidechainColors::textPrimary());
  g.setFont(12.0f);
  juce::String bpmText = (bpmMin == 0 && bpmMax == 200) ? "All BPM" : juce::String(bpmMin) + "-" + juce::String(bpmMax);
  g.drawText(bpmText, bpmFilterBounds.reduced(10, 5), juce::Justification::centredLeft);

  // Key filter
  keyFilterBounds = bounds.reduced(5);
  g.setColour(SidechainColors::surface());
  g.fillRoundedRectangle(keyFilterBounds.toFloat(), 6.0f);
  g.setColour(SidechainColors::border());
  g.drawRoundedRectangle(keyFilterBounds.toFloat(), 6.0f, 1.0f);
  g.setColour(SidechainColors::textPrimary());
  g.setFont(12.0f);
  juce::String keyText = selectedKey.isEmpty() ? "All Keys" : selectedKey;
  g.drawText(keyText, keyFilterBounds.reduced(10, 5), juce::Justification::centredLeft);
}

void Search::drawTabs(juce::Graphics &g) {
  auto bounds = getTabBounds();
  int tabWidth = bounds.getWidth() / 2;

  // Users tab
  usersTabBounds = bounds.removeFromLeft(tabWidth);
  g.setColour(currentTab == ResultTab::Users ? SidechainColors::accent() : SidechainColors::surface());
  g.fillRect(usersTabBounds);
  g.setColour(SidechainColors::border());
  g.drawRect(usersTabBounds);
  g.setColour(SidechainColors::textPrimary());
  g.setFont(14.0f);
  juce::String usersText = "Users";
  if (currentTab == ResultTab::Users && totalUserResults > 0)
    usersText += " (" + juce::String(totalUserResults) + ")";
  g.drawText(usersText, usersTabBounds, juce::Justification::centred);

  // Posts tab
  postsTabBounds = bounds;
  g.setColour(currentTab == ResultTab::Posts ? SidechainColors::accent() : SidechainColors::surface());
  g.fillRect(postsTabBounds);
  g.setColour(SidechainColors::border());
  g.drawRect(postsTabBounds);
  g.setColour(SidechainColors::textPrimary());
  g.setFont(14.0f);
  juce::String postsText = "Posts";
  if (currentTab == ResultTab::Posts && totalPostResults > 0)
    postsText += " (" + juce::String(totalPostResults) + ")";
  g.drawText(postsText, postsTabBounds, juce::Justification::centred);
}

void Search::drawResults(juce::Graphics &g) {
  auto bounds = getResultsBounds();
  int yPos = bounds.getY() - static_cast<int>(scrollPosition);

  if (currentTab == ResultTab::Users) {
    // Draw user cards
    for (int i = 0; i < static_cast<int>(userResults.size()); ++i) {
      juce::Rectangle<int> cardBounds(10, yPos + i * CARD_HEIGHT, bounds.getWidth() - 20, CARD_HEIGHT - 5);
      if (cardBounds.getBottom() < bounds.getY() || cardBounds.getY() > bounds.getBottom())
        continue; // Off screen

      // Create user card if needed
      while (static_cast<int>(userCards.size()) <= i) {
        auto *card = new UserCard();
        // Setup callbacks for user interactions
        card->onUserClicked = [this](const DiscoveredUser &user) {
          if (onUserSelected)
            onUserSelected(user.id);
        };
        card->onFollowToggled = [this](const DiscoveredUser &user, bool willFollow) {
          if (appStore != nullptr) {
            // Update UI optimistically
            for (auto *searchCard : userCards) {
              if (searchCard && searchCard->getUserId() == user.id) {
                searchCard->setIsFollowing(willFollow);
                break;
              }
            }
            // Use AppStore reactive observables to handle follow/unfollow with cache invalidation
            juce::Component::SafePointer<Search> safeThis(this);
            if (willFollow) {
              appStore->followUserObservable(user.id).subscribe(
                  [safeThis](int) {
                    if (safeThis == nullptr)
                      return;
                    Log::debug("Search: User followed successfully");
                  },
                  [safeThis](std::exception_ptr error) {
                    if (safeThis == nullptr)
                      return;
                    juce::String errorMsg = "Search: Failed to follow user";
                    if (error) {
                      try {
                        std::rethrow_exception(error);
                      } catch (const std::exception &e) {
                        errorMsg += " - " + juce::String(e.what());
                      }
                    }
                    Log::error(errorMsg);
                  });
            } else {
              appStore->unfollowUserObservable(user.id).subscribe(
                  [safeThis](int) {
                    if (safeThis == nullptr)
                      return;
                    Log::debug("Search: User unfollowed successfully");
                  },
                  [safeThis](std::exception_ptr error) {
                    if (safeThis == nullptr)
                      return;
                    juce::String errorMsg = "Search: Failed to unfollow user";
                    if (error) {
                      try {
                        std::rethrow_exception(error);
                      } catch (const std::exception &e) {
                        errorMsg += " - " + juce::String(e.what());
                      }
                    }
                    Log::error(errorMsg);
                  });
            }
          }
        };
        addAndMakeVisible(card);
        userCards.add(card);
      }

      if (userCards[i] != nullptr) {
        userCards[i]->setUser(userResults[i]);
        userCards[i]->setBounds(cardBounds);

        // Highlight if keyboard-selected
        if (i == selectedResultIndex) {
          g.setColour(SidechainColors::accent().withAlpha(0.3f));
          g.fillRoundedRectangle(cardBounds.toFloat(), 4.0f);
        }
      }
    }
  } else // Posts tab
  {
    // Draw post cards
    for (int i = 0; i < static_cast<int>(postResults.size()); ++i) {
      juce::Rectangle<int> cardBounds(10, yPos + i * CARD_HEIGHT, bounds.getWidth() - 20, CARD_HEIGHT - 5);
      if (cardBounds.getBottom() < bounds.getY() || cardBounds.getY() > bounds.getBottom())
        continue; // Off screen

      // Create post card if needed
      while (static_cast<int>(postCards.size()) <= i) {
        auto *card = new PostCard();
        addAndMakeVisible(card);
        postCards.add(card);
      }

      if (postCards[i] != nullptr) {
        postCards[i]->setPost(postResults[i]);
        postCards[i]->setBounds(cardBounds);

        // Highlight if keyboard-selected
        if (i == selectedResultIndex) {
          g.setColour(SidechainColors::accent().withAlpha(0.3f));
          g.fillRoundedRectangle(cardBounds.toFloat(), 4.0f);
        }
      }
    }
  }

  // Update scrollbar
  totalContentHeight =
      (currentTab == ResultTab::Users ? static_cast<int>(userResults.size()) : static_cast<int>(postResults.size())) *
      CARD_HEIGHT;
  int visibleHeight = bounds.getHeight();
  if (totalContentHeight > visibleHeight) {
    scrollBar->setRangeLimits(0.0, static_cast<double>(totalContentHeight - visibleHeight));
    scrollBar->setCurrentRangeStart(scrollPosition);
    scrollBar->setVisible(true);
  } else {
    scrollBar->setVisible(false);
  }
}

void Search::drawEmptyState(juce::Graphics &g) {
  auto bounds = getResultsBounds();

  g.setColour(SidechainColors::textMuted());
  g.setFont(18.0f);
  g.drawText("Start typing to search...", bounds.removeFromTop(30), juce::Justification::centred);

  // Draw recent searches
  drawRecentSearches(g);

  // Draw trending searches
  drawTrendingSearches(g);
}

void Search::drawNoResultsState(juce::Graphics &g) {
  auto bounds = getResultsBounds();

  g.setColour(SidechainColors::textMuted());
  g.setFont(18.0f);
  g.drawText("No results found", bounds.removeFromTop(30), juce::Justification::centred);

  g.setFont(14.0f);
  g.drawText("Try a different search term or adjust your filters", bounds.removeFromTop(25),
             juce::Justification::centred);

  // Show suggestions
  g.setFont(12.0f);
  g.drawText("Suggestions:", bounds.removeFromTop(20).translated(20, 0), juce::Justification::centredLeft);
  bounds.removeFromTop(10);

  juce::Array<juce::String> suggestions = {"Try a different keyword", "Remove filters", "Check spelling"};
  for (int i = 0; i < suggestions.size(); ++i) {
    g.drawText(juce::String::fromUTF8("• ") + suggestions[i], bounds.removeFromTop(20),
               juce::Justification::centredLeft);
  }
}

void Search::drawErrorState(juce::Graphics &g) {
  auto bounds = getResultsBounds();

  g.setColour(SidechainColors::error());
  g.setFont(16.0f);
  g.drawText("Error searching. Please try again.", bounds, juce::Justification::centred);
}

void Search::drawRecentSearches(juce::Graphics &g) {
  if (recentSearches.isEmpty())
    return;

  int yPos = HEADER_HEIGHT + FILTER_HEIGHT + 40;

  g.setColour(SidechainColors::textPrimary());
  g.setFont(14.0f);
  g.drawText("Recent Searches", juce::Rectangle<int>(20, yPos, getWidth() - 40, 25), juce::Justification::centredLeft);
  yPos += 30;

  g.setColour(SidechainColors::textMuted());
  g.setFont(12.0f);
  for (int i = 0; i < static_cast<int>(recentSearches.size()) && i < 5; ++i) {
    juce::Rectangle<int> itemBounds(20, yPos + i * 40, getWidth() - 40, 35);
    g.setColour(SidechainColors::surface());
    g.fillRoundedRectangle(itemBounds.toFloat(), 6.0f);
    g.setColour(SidechainColors::textPrimary());
    g.drawText(recentSearches[i], itemBounds.reduced(10, 5), juce::Justification::centredLeft);
  }
}

void Search::drawTrendingSearches(juce::Graphics &g) {
  if (trendingSearches.isEmpty())
    return;

  int yPos = HEADER_HEIGHT + FILTER_HEIGHT + 40 +
             (static_cast<int>(recentSearches.size()) > 0 ? (static_cast<int>(recentSearches.size()) * 40 + 40) : 0);

  g.setColour(SidechainColors::textPrimary());
  g.setFont(14.0f);
  g.drawText("Trending Searches", juce::Rectangle<int>(20, yPos, getWidth() - 40, 25),
             juce::Justification::centredLeft);
  yPos += 30;

  g.setColour(SidechainColors::textMuted());
  g.setFont(12.0f);
  for (int i = 0; i < static_cast<int>(trendingSearches.size()) && i < 5; ++i) {
    juce::Rectangle<int> itemBounds(20, yPos + i * 40, getWidth() - 40, 35);
    g.setColour(SidechainColors::surface());
    g.fillRoundedRectangle(itemBounds.toFloat(), 6.0f);
    g.setColour(SidechainColors::textPrimary());
    g.drawText(trendingSearches[i], itemBounds.reduced(10, 5), juce::Justification::centredLeft);
  }
}

// ==============================================================================
void Search::layoutComponents() {
  auto bounds = getLocalBounds();

  // Scrollbar (with null check for safety)
  if (scrollBar != nullptr) {
    scrollBar->setBounds(bounds.removeFromRight(12));
  }

  // Position error state component in results area
  if (errorStateComponent != nullptr) {
    errorStateComponent->setBounds(getResultsBounds());
  }

  // Header, filters, tabs, and results are drawn in paint
}

juce::Rectangle<int> Search::getHeaderBounds() const {
  return juce::Rectangle<int>(0, 0, getWidth(), HEADER_HEIGHT);
}

juce::Rectangle<int> Search::getFilterBounds() const {
  return juce::Rectangle<int>(0, HEADER_HEIGHT, getWidth(), FILTER_HEIGHT);
}

juce::Rectangle<int> Search::getTabBounds() const {
  return juce::Rectangle<int>(0, HEADER_HEIGHT + FILTER_HEIGHT, getWidth(), 40);
}

juce::Rectangle<int> Search::getResultsBounds() const {
  int top = HEADER_HEIGHT + FILTER_HEIGHT + 40;
  int scrollBarWidth = (scrollBar != nullptr && scrollBar->isVisible()) ? 12 : 0;
  return juce::Rectangle<int>(0, top, getWidth() - scrollBarWidth, getHeight() - top);
}

// ==============================================================================
// Filter picker implementations
// ==============================================================================

const std::array<juce::String, 12> &Search::getAvailableGenres() {
  static const std::array<juce::String, 12> genres = {{"Electronic", "Hip-Hop / Trap", "House", "Techno", "Drum & Bass",
                                                       "Dubstep", "Pop", "R&B / Soul", "Rock", "Lo-Fi", "Ambient",
                                                       "Other"}};
  return genres;
}

const std::array<juce::String, 24> &Search::getMusicalKeys() {
  static const std::array<juce::String, 24> keys = {
      {"C Major",       "C# / Db Major", "D Major",       "D# / Eb Major", "E Major",       "F Major",
       "F# / Gb Major", "G Major",       "G# / Ab Major", "A Major",       "A# / Bb Major", "B Major",
       "C Minor",       "C# / Db Minor", "D Minor",       "D# / Eb Minor", "E Minor",       "F Minor",
       "F# / Gb Minor", "G Minor",       "G# / Ab Minor", "A Minor",       "A# / Bb Minor", "B Minor"}};
  return keys;
}

void Search::showGenrePicker() {
  juce::PopupMenu menu;
  auto &genres = getAvailableGenres();

  // Add "All Genres" option
  menu.addItem(1, "All Genres", true, selectedGenre.isEmpty());

  // Add genre options
  for (size_t i = 0; i < genres.size(); ++i) {
    menu.addItem(static_cast<int>(i + 2), genres[i], true, selectedGenre == genres[i]);
  }

  menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withTargetScreenArea(
                         genreFilterBounds.translated(getScreenX(), getScreenY())),
                     [this](int result) {
                       if (result == 1) {
                         selectedGenre = juce::String();
                       } else if (result > 1) {
                         auto &genreList = getAvailableGenres();
                         int index = result - 2;
                         if (index >= 0 && index < static_cast<int>(genreList.size())) {
                           selectedGenre = genreList[static_cast<size_t>(index)];
                         }
                       }
                       applyFilters();
                       repaint();
                     });
}

void Search::showBPMPicker() {
  juce::PopupMenu menu;

  // Common BPM ranges
  struct BPMPreset {
    juce::String name;
    int min;
    int max;
  };

  static const std::array<BPMPreset, 8> presets = {{{"All BPM", 0, 200},
                                                    {"60-80 (Downtempo)", 60, 80},
                                                    {"80-100 (Hip-Hop)", 80, 100},
                                                    {"100-120 (House)", 100, 120},
                                                    {"120-130 (House/Techno)", 120, 130},
                                                    {"130-150 (Techno/Trance)", 130, 150},
                                                    {"150-180 (Drum & Bass)", 150, 180},
                                                    {"Custom...", -1, -1}}};

  for (size_t i = 0; i < presets.size(); ++i) {
    bool isSelected =
        (bpmMin == presets[i].min && bpmMax == presets[i].max) || (i == 0 && bpmMin == 0 && bpmMax == 200);
    menu.addItem(static_cast<int>(i + 1), presets[i].name, true, isSelected);
  }

  menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withTargetScreenArea(
                         bpmFilterBounds.translated(getScreenX(), getScreenY())),
                     [this](int result) {
                       struct BPMPreset {
                         int min;
                         int max;
                       };
                       static const std::array<BPMPreset, 8> bpmPresets = {{
                           {0, 200},   // All
                           {60, 80},   // Downtempo
                           {80, 100},  // Hip-Hop
                           {100, 120}, // House
                           {120, 130}, // House/Techno
                           {130, 150}, // Techno/Trance
                           {150, 180}, // Drum & Bass
                           {-1, -1}    // Custom (implemented in showCustomBPMDialog)
                       }};

                       if (result > 0 && result <= (int)bpmPresets.size()) {
                         int index = result - 1;
                         if (bpmPresets[static_cast<size_t>(index)].min >= 0) // Not custom
                         {
                           bpmMin = bpmPresets[static_cast<size_t>(index)].min;
                           bpmMax = bpmPresets[static_cast<size_t>(index)].max;
                           applyFilters();
                           repaint();
                         } else {
                           // Custom BPM range - show dialog
                           showCustomBPMDialog();
                         }
                       }
                     });
}

void Search::showCustomBPMDialog() {
  // Use heap-allocated AlertWindow to avoid use-after-free
  // (enterModalState is async, so stack allocation would be destroyed before callback)
  auto *alert = new juce::AlertWindow("Custom BPM Range",
                                      "Enter minimum and maximum BPM values:", juce::MessageBoxIconType::QuestionIcon);

  alert->addTextEditor("bpmMin", juce::String(bpmMin), "Minimum BPM:", false);
  alert->addTextEditor("bpmMax", juce::String(bpmMax), "Maximum BPM:", false);

  auto *minEditor = alert->getTextEditor("bpmMin");
  auto *maxEditor = alert->getTextEditor("bpmMax");

  if (minEditor != nullptr)
    minEditor->setInputRestrictions(3, "0123456789");
  if (maxEditor != nullptr)
    maxEditor->setInputRestrictions(3, "0123456789");

  alert->addButton("Apply", 1, juce::KeyPress(juce::KeyPress::returnKey));
  alert->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

  alert->enterModalState(true, juce::ModalCallbackFunction::create([this, alert](int result) {
                           if (result == 1) {
                             juce::String minText = alert->getTextEditorContents("bpmMin").trim();
                             juce::String maxText = alert->getTextEditorContents("bpmMax").trim();

                             int newMin = minText.getIntValue();
                             int newMax = maxText.getIntValue();

                             // Validate range
                             if (newMin >= 0 && newMax > newMin && newMax <= 300) {
                               bpmMin = newMin;
                               bpmMax = newMax;
                               applyFilters();
                               repaint();
                             } else {
                               juce::AlertWindow::showMessageBoxAsync(
                                   juce::MessageBoxIconType::WarningIcon, "Invalid Range",
                                   "Please enter a valid BPM range:\n- Minimum: 0-299\n- Maximum: "
                                   "1-300\n- Maximum must be greater than minimum");
                             }
                           }
                           // Note: alert is deleted by JUCE (deleteWhenDismissed=true)
                         }),
                         true); // deleteWhenDismissed=true - JUCE handles cleanup
}

void Search::showKeyPicker() {
  juce::PopupMenu menu;
  auto &keys = getMusicalKeys();

  // Add "All Keys" option
  menu.addItem(1, "All Keys", true, selectedKey.isEmpty());

  // Add key options
  for (size_t i = 0; i < keys.size(); ++i) {
    menu.addItem(static_cast<int>(i + 2), keys[i], true, selectedKey == keys[i]);
  }

  menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withTargetScreenArea(
                         keyFilterBounds.translated(getScreenX(), getScreenY())),
                     [this](int result) {
                       if (result == 1) {
                         selectedKey = juce::String();
                       } else if (result > 1) {
                         auto &keyList = getMusicalKeys();
                         int index = result - 2;
                         if (index >= 0 && index < static_cast<int>(keyList.size())) {
                           selectedKey = keyList[static_cast<size_t>(index)];
                         }
                       }
                       applyFilters();
                       repaint();
                     });
}
