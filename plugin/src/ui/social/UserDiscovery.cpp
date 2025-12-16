#include "UserDiscovery.h"
#include "../../network/NetworkClient.h"
#include "../../network/StreamChatClient.h"
#include "../../util/Json.h"
#include "../../util/Log.h"
#include "../../util/Result.h"
#include <set>
#include <vector>

//==============================================================================
UserDiscovery::UserDiscovery() {
  Log::info("UserDiscovery: Initializing");

  // Create search box
  searchBox = std::make_unique<juce::TextEditor>();
  searchBox->setMultiLine(false);
  searchBox->setReturnKeyStartsNewLine(false);
  searchBox->setScrollbarsShown(false);
  searchBox->setCaretVisible(true);
  searchBox->setPopupMenuEnabled(false);
  searchBox->setTextToShowWhenEmpty("Search users...", Colors::textPlaceholder);
  searchBox->setColour(juce::TextEditor::backgroundColourId, Colors::searchBg);
  searchBox->setColour(juce::TextEditor::textColourId, Colors::textPrimary);
  searchBox->setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
  searchBox->setColour(juce::TextEditor::focusedOutlineColourId, Colors::accent);
  searchBox->addListener(this);
  addAndMakeVisible(*searchBox);

  // Set up scroll bar
  scrollBar.setRangeLimits(0.0, 100.0);
  scrollBar.addListener(this);
  addAndMakeVisible(scrollBar);

  // Load recent searches from disk
  loadRecentSearches();

  // Note: Online status is now implemented - queries getstream.io Chat presence
  // and shows online indicators on UserCards
}

UserDiscovery::~UserDiscovery() {
  Log::debug("UserDiscovery: Destroying");
  searchBox->removeListener(this);
  scrollBar.removeListener(this);
  if (storeUnsubscriber) {
    storeUnsubscriber();
  }
}

//==============================================================================
void UserDiscovery::setNetworkClient(NetworkClient *client) {
  networkClient = client;
  Log::debug("UserDiscovery: NetworkClient set " + juce::String(client != nullptr ? "(valid)" : "(null)"));
}

void UserDiscovery::setStreamChatClient(StreamChatClient *client) {
  streamChatClient = client;
  Log::info("UserDiscovery::setStreamChatClient: StreamChatClient set " +
            juce::String(client != nullptr ? "(valid)" : "(null)"));
}

void UserDiscovery::setUserStore(std::shared_ptr<Sidechain::Stores::AppStore> store) {
  // Unsubscribe from old store
  if (storeUnsubscriber) {
    storeUnsubscriber();
  }

  userStore = store;
  jassert(userStore != nullptr); // UserStore is required

  Log::info("UserDiscovery: UserStore set (Phase 2: full integration pending)");
  // Phase 2: Will subscribe to UserStore for discovery data
  // TODO: Implement discovery data in UserStore (loadTrendingUsers, loadSuggestedUsers, etc.)
}

//==============================================================================
void UserDiscovery::paint(juce::Graphics &g) {
  // Background
  g.fillAll(Colors::background);

  // Header
  drawHeader(g);

  // Content area based on view mode
  auto contentBounds = getContentBounds();
  contentBounds.translate(0, -scrollOffset);

  switch (currentViewMode) {
  case ViewMode::Discovery: {
    // Use local member variables (store integration is Phase 2)
    auto useTrendingUsers = trendingUsers;
    auto useFeaturedProducers = featuredProducers;
    auto useSuggestedUsers = suggestedUsers;
    auto useSimilarProducers = similarProducers;
    auto useRecommendedToFollow = recommendedToFollow;
    auto useAvailableGenres = availableGenres;
    bool isLoading = (isTrendingLoading || isFeaturedLoading || isSuggestedLoading || isSimilarLoading ||
                      isRecommendedLoading || isGenresLoading);

    // Show recent searches if search box has focus
    if (searchBox && searchBox->hasKeyboardFocus(true) && recentSearches.size() > 0 && currentSearchQuery.isEmpty()) {
      drawRecentSearches(g, contentBounds);
    }

    // Genre chips for filtering (get from store or member variable)
    drawGenreChips(g, contentBounds, useAvailableGenres);
    contentBounds.removeFromTop(8); // spacing

    // Trending section
    if (!useTrendingUsers.isEmpty()) {
      drawSectionHeader(g, contentBounds.removeFromTop(SECTION_HEADER_HEIGHT), "Trending");
      drawTrendingSection(g, contentBounds);
      contentBounds.removeFromTop(16);
    }

    // Featured section
    if (!useFeaturedProducers.isEmpty()) {
      drawSectionHeader(g, contentBounds.removeFromTop(SECTION_HEADER_HEIGHT), "Featured Producers");
      drawFeaturedSection(g, contentBounds);
      contentBounds.removeFromTop(16);
    }

    // Suggested section
    if (!useSuggestedUsers.isEmpty()) {
      drawSectionHeader(g, contentBounds.removeFromTop(SECTION_HEADER_HEIGHT), "Suggested For You");
      drawSuggestedSection(g, contentBounds);
      contentBounds.removeFromTop(16);
    }

    // Similar producers section
    if (!useSimilarProducers.isEmpty()) {
      drawSectionHeader(g, contentBounds.removeFromTop(SECTION_HEADER_HEIGHT), "Producers You Might Like");
      drawSimilarSection(g, contentBounds);
      contentBounds.removeFromTop(16);
    }

    // Recommended to follow section (Gorse collaborative filtering)
    if (!useRecommendedToFollow.isEmpty()) {
      drawSectionHeader(g, contentBounds.removeFromTop(SECTION_HEADER_HEIGHT), "Recommended to Follow");
      drawRecommendedSection(g, contentBounds);
    }

    // Loading state
    if (isLoading) {
      drawLoadingState(g, getContentBounds());
    } else if (useTrendingUsers.isEmpty() && useFeaturedProducers.isEmpty() && useSuggestedUsers.isEmpty() &&
               useSimilarProducers.isEmpty() && useRecommendedToFollow.isEmpty()) {
      drawEmptyState(g, getContentBounds(), "No users to discover yet.\nBe the first to share your music!");
    }
    break;
  }

  case ViewMode::SearchResults: {
    if (isSearching) {
      drawLoadingState(g, contentBounds);
    } else if (searchResults.isEmpty()) {
      drawEmptyState(g, contentBounds, "No users found for \"" + currentSearchQuery + "\"");
    } else {
      drawSearchResults(g, contentBounds);
    }
    break;
  }

  case ViewMode::GenreFilter: {
    drawGenreChips(g, contentBounds, availableGenres);
    contentBounds.removeFromTop(8);

    drawSectionHeader(g, contentBounds.removeFromTop(SECTION_HEADER_HEIGHT), selectedGenre + " Producers");

    if (genreUsers.isEmpty()) {
      drawEmptyState(g, contentBounds, "No producers found in " + selectedGenre);
    }
    break;
  }
  }
}

void UserDiscovery::resized() {
  auto bounds = getLocalBounds();

  // Header area
  bounds.removeFromTop(HEADER_HEIGHT);

  // Search box
  auto searchBounds = getSearchBoxBounds();
  searchBox->setBounds(searchBounds.reduced(8, 4));

  // Update scroll bar
  auto contentBounds = getContentBounds();
  scrollBar.setBounds(contentBounds.removeFromRight(12));

  // Position user cards
  updateUserCardPositions();
  updateScrollBounds();
}

//==============================================================================
void UserDiscovery::drawHeader(juce::Graphics &g) {
  auto headerBounds = getLocalBounds().removeFromTop(HEADER_HEIGHT);

  // Header background
  g.setColour(Colors::headerBg);
  g.fillRect(headerBounds);

  // Back button
  auto backBounds = getBackButtonBounds();
  g.setColour(Colors::textPrimary);
  g.setFont(juce::Font(juce::FontOptions().withHeight(24.0f)));
  g.drawText("<", backBounds, juce::Justification::centred);

  // Title
  g.setFont(juce::Font(juce::FontOptions().withHeight(18.0f).withStyle("Bold")));
  auto titleBounds = headerBounds.withTrimmedLeft(50);
  g.drawText("Discover", titleBounds, juce::Justification::centredLeft);

  // Search bar area
  auto searchBounds = getSearchBoxBounds();
  g.setColour(Colors::searchBg);
  g.fillRoundedRectangle(searchBounds.reduced(4).toFloat(), 8.0f);

  // Search icon (avoid emoji for Linux font compatibility)
  g.setColour(Colors::textPlaceholder);
  g.setFont(juce::Font(juce::FontOptions().withHeight(14.0f)));
  auto iconBounds = searchBounds.removeFromLeft(40);
  g.drawText("[?]", iconBounds, juce::Justification::centred);

  // Clear button (X) when there's text
  if (currentSearchQuery.isNotEmpty()) {
    auto clearBounds = getClearSearchBounds();
    g.setColour(Colors::textSecondary);
    g.setFont(juce::Font(juce::FontOptions().withHeight(16.0f)));
    g.drawText("x", clearBounds, juce::Justification::centred);
  }
}

void UserDiscovery::drawRecentSearches(juce::Graphics &g, juce::Rectangle<int> &bounds) {
  g.setFont(juce::Font(juce::FontOptions().withHeight(12.0f).withStyle("Bold")));
  g.setColour(Colors::sectionHeader);

  auto headerBounds = bounds.removeFromTop(30);
  headerBounds.removeFromLeft(PADDING);
  g.drawText("RECENT SEARCHES", headerBounds, juce::Justification::centredLeft);

  g.setFont(juce::Font(juce::FontOptions().withHeight(14.0f)));
  g.setColour(Colors::textPrimary);

  for (int i = 0; i < recentSearches.size() && i < MAX_RECENT_SEARCHES; ++i) {
    auto itemBounds = bounds.removeFromTop(36);
    itemBounds.removeFromLeft(PADDING);

    // Recent search indicator (avoid emoji for Linux font compatibility)
    g.setColour(Colors::textSecondary);
    g.drawText(">", itemBounds.removeFromLeft(24), juce::Justification::centredLeft);

    g.setColour(Colors::textPrimary);
    g.drawText(recentSearches[i], itemBounds, juce::Justification::centredLeft);
  }

  bounds.removeFromTop(8);
}

void UserDiscovery::drawSectionHeader(juce::Graphics &g, juce::Rectangle<int> bounds, const juce::String &title) {
  bounds.removeFromLeft(PADDING);

  g.setColour(Colors::textPrimary);
  g.setFont(juce::Font(juce::FontOptions().withHeight(14.0f).withStyle("Bold")));
  g.drawText(title, bounds, juce::Justification::centredLeft);
}

void UserDiscovery::drawTrendingSection(juce::Graphics &g, juce::Rectangle<int> &bounds) {
  // Draw separators between trending user cards
  int count = juce::jmin(5, trendingUsers.size());

  for (int i = 0; i < count; ++i) {
    bounds.removeFromTop(USER_CARD_HEIGHT);

    // Draw separator line between cards
    if (i < count - 1) {
      g.setColour(Colors::textSecondary.withAlpha(0.2f));
      g.drawLine(static_cast<float>(bounds.getX() + PADDING), static_cast<float>(bounds.getY()),
                 static_cast<float>(bounds.getRight() - PADDING), static_cast<float>(bounds.getY()), 0.5f);
    }
  }
}

void UserDiscovery::drawFeaturedSection(juce::Graphics &g, juce::Rectangle<int> &bounds) {
  // Draw separators between featured producer cards
  int count = juce::jmin(5, featuredProducers.size());

  for (int i = 0; i < count; ++i) {
    bounds.removeFromTop(USER_CARD_HEIGHT);

    // Draw separator line between cards
    if (i < count - 1) {
      g.setColour(Colors::textSecondary.withAlpha(0.2f));
      g.drawLine(static_cast<float>(bounds.getX() + PADDING), static_cast<float>(bounds.getY()),
                 static_cast<float>(bounds.getRight() - PADDING), static_cast<float>(bounds.getY()), 0.5f);
    }
  }
}

void UserDiscovery::drawSuggestedSection(juce::Graphics &g, juce::Rectangle<int> &bounds) {
  // Draw separators between suggested user cards
  int count = juce::jmin(5, suggestedUsers.size());

  for (int i = 0; i < count; ++i) {
    bounds.removeFromTop(USER_CARD_HEIGHT);

    // Draw separator line between cards
    if (i < count - 1) {
      g.setColour(Colors::textSecondary.withAlpha(0.2f));
      g.drawLine(static_cast<float>(bounds.getX() + PADDING), static_cast<float>(bounds.getY()),
                 static_cast<float>(bounds.getRight() - PADDING), static_cast<float>(bounds.getY()), 0.5f);
    }
  }
}

void UserDiscovery::drawSimilarSection(juce::Graphics &g, juce::Rectangle<int> &bounds) {
  // Draw separators between similar producer cards
  int count = juce::jmin(5, similarProducers.size());

  for (int i = 0; i < count; ++i) {
    bounds.removeFromTop(USER_CARD_HEIGHT);

    // Draw separator line between cards
    if (i < count - 1) {
      g.setColour(Colors::textSecondary.withAlpha(0.2f));
      g.drawLine(static_cast<float>(bounds.getX() + PADDING), static_cast<float>(bounds.getY()),
                 static_cast<float>(bounds.getRight() - PADDING), static_cast<float>(bounds.getY()), 0.5f);
    }
  }
}

void UserDiscovery::drawRecommendedSection(juce::Graphics &g, juce::Rectangle<int> &bounds) {
  // Draw separators between recommended user cards
  int count = juce::jmin(5, recommendedToFollow.size());

  for (int i = 0; i < count; ++i) {
    bounds.removeFromTop(USER_CARD_HEIGHT);

    // Draw separator line between cards
    if (i < count - 1) {
      g.setColour(Colors::textSecondary.withAlpha(0.2f));
      g.drawLine(static_cast<float>(bounds.getX() + PADDING), static_cast<float>(bounds.getY()),
                 static_cast<float>(bounds.getRight() - PADDING), static_cast<float>(bounds.getY()), 0.5f);
    }
  }
}

void UserDiscovery::drawGenreChips(juce::Graphics &g, juce::Rectangle<int> &bounds,
                                   const juce::StringArray &availableGenres) {
  if (availableGenres.isEmpty())
    return;

  auto chipArea = bounds.removeFromTop(GENRE_CHIP_HEIGHT + 16);
  chipArea = chipArea.reduced(PADDING, 8);

  g.setFont(juce::Font(juce::FontOptions().withHeight(12.0f)));

  int x = chipArea.getX();
  int y = chipArea.getY();
  int maxWidth = chipArea.getRight() - PADDING;

  for (int i = 0; i < availableGenres.size(); ++i) {
    auto genre = availableGenres[i];
    juce::GlyphArrangement glyphs;
    glyphs.addLineOfText(g.getCurrentFont(), genre, 0, 0);
    auto textWidth = static_cast<int>(glyphs.getBoundingBox(0, -1, true).getWidth());
    auto chipWidth = textWidth + 20;

    // Wrap to next line if needed
    if (x + chipWidth > maxWidth) {
      x = chipArea.getX();
      y += GENRE_CHIP_HEIGHT + 8;
      bounds.removeFromTop(GENRE_CHIP_HEIGHT + 8);
    }

    juce::Rectangle<int> chipBounds(x, y, chipWidth, GENRE_CHIP_HEIGHT);

    // Draw chip
    bool isSelected = (selectedGenre == genre);
    g.setColour(isSelected ? Colors::chipSelected : Colors::chipBg);
    g.fillRoundedRectangle(chipBounds.toFloat(), GENRE_CHIP_HEIGHT / 2.0f);

    g.setColour(isSelected ? juce::Colours::black : Colors::textPrimary);
    g.drawText(genre, chipBounds, juce::Justification::centred);

    x += chipWidth + 8;
  }
}

void UserDiscovery::drawSearchResults(juce::Graphics &g, juce::Rectangle<int> bounds) {
  // Search results are drawn by UserCardComponents
  g.setColour(Colors::textSecondary);
  g.setFont(juce::Font(juce::FontOptions().withHeight(12.0f)));

  auto resultCount = bounds.removeFromTop(30).reduced(PADDING, 0);
  g.drawText(juce::String(searchResults.size()) + " results for \"" + currentSearchQuery + "\"", resultCount,
             juce::Justification::centredLeft);
}

void UserDiscovery::drawLoadingState(juce::Graphics &g, juce::Rectangle<int> bounds) {
  g.setColour(Colors::textSecondary);
  g.setFont(juce::Font(juce::FontOptions().withHeight(14.0f)));
  g.drawText("Loading...", bounds, juce::Justification::centred);
}

void UserDiscovery::drawEmptyState(juce::Graphics &g, juce::Rectangle<int> bounds, const juce::String &message) {
  g.setColour(Colors::textSecondary);
  g.setFont(juce::Font(juce::FontOptions().withHeight(14.0f)));

  // Center the message
  auto textBounds = bounds.withSizeKeepingCentre(bounds.getWidth() - 40, 60);
  g.drawFittedText(message, textBounds, juce::Justification::centred, 3);
}

//==============================================================================
juce::Rectangle<int> UserDiscovery::getBackButtonBounds() const {
  return {8, 12, 40, 36};
}

juce::Rectangle<int> UserDiscovery::getSearchBoxBounds() const {
  auto bounds = getLocalBounds();
  bounds.removeFromTop(HEADER_HEIGHT);
  return bounds.removeFromTop(SEARCH_BAR_HEIGHT + 8).reduced(PADDING - 8, 4);
}

juce::Rectangle<int> UserDiscovery::getClearSearchBounds() const {
  auto searchBounds = getSearchBoxBounds();
  return searchBounds.removeFromRight(36);
}

juce::Rectangle<int> UserDiscovery::getContentBounds() const {
  auto bounds = getLocalBounds();
  bounds.removeFromTop(HEADER_HEIGHT + SEARCH_BAR_HEIGHT + 8);
  return bounds;
}

//==============================================================================
void UserDiscovery::mouseUp(const juce::MouseEvent &event) {
  auto point = event.getPosition();

  // Back button
  if (getBackButtonBounds().contains(point)) {
    if (currentViewMode == ViewMode::SearchResults || currentViewMode == ViewMode::GenreFilter) {
      // Go back to discovery view
      currentViewMode = ViewMode::Discovery;
      currentSearchQuery.clear();
      selectedGenre.clear();
      searchBox->clear();
      rebuildUserCards();
      repaint();
    } else if (onBackPressed) {
      onBackPressed();
    }
    return;
  }

  // Clear search button
  if (currentSearchQuery.isNotEmpty() && getClearSearchBounds().contains(point)) {
    currentSearchQuery.clear();
    searchBox->clear();
    currentViewMode = ViewMode::Discovery;
    searchResults.clear();
    rebuildUserCards();
    repaint();
    return;
  }

  // Genre chips
  if (point.y > HEADER_HEIGHT + SEARCH_BAR_HEIGHT &&
      point.y < HEADER_HEIGHT + SEARCH_BAR_HEIGHT + GENRE_CHIP_HEIGHT + 24) {
    // Check which genre was clicked
    for (int i = 0; i < availableGenres.size(); ++i) {
      auto chipBounds = getGenreChipBounds(i);
      if (chipBounds.contains(point)) {
        if (selectedGenre == availableGenres[i]) {
          // Deselect - go back to discovery
          selectedGenre.clear();
          currentViewMode = ViewMode::Discovery;
          rebuildUserCards();
        } else {
          // Select genre
          selectedGenre = availableGenres[i];
          currentViewMode = ViewMode::GenreFilter;
          fetchUsersByGenre(selectedGenre);
        }
        repaint();
        return;
      }
    }
  }

  // Recent searches
  if (searchBox->hasKeyboardFocus(true) && currentSearchQuery.isEmpty()) {
    for (int i = 0; i < recentSearches.size(); ++i) {
      auto itemBounds = getRecentSearchBounds(i);
      if (itemBounds.contains(point)) {
        searchBox->setText(recentSearches[i]);
        performSearch(recentSearches[i]);
        return;
      }
    }
  }
}

void UserDiscovery::textEditorTextChanged(juce::TextEditor &editor) {
  currentSearchQuery = editor.getText();

  if (currentSearchQuery.isEmpty()) {
    currentViewMode = ViewMode::Discovery;
    searchResults.clear();
    rebuildUserCards();
    repaint();
  }
}

void UserDiscovery::textEditorReturnKeyPressed(juce::TextEditor &editor) {
  auto query = editor.getText().trim();
  if (query.isNotEmpty()) {
    performSearch(query);
  }
}

void UserDiscovery::scrollBarMoved(juce::ScrollBar *bar, double newRangeStart) {
  // Verify scroll bar callback is from our scroll bar instance
  if (bar == &scrollBar) {
    scrollOffset = static_cast<int>(newRangeStart);
    updateUserCardPositions();
    repaint();
  }
}

//==============================================================================
void UserDiscovery::loadDiscoveryData() {
  jassert(userStore != nullptr);
  if (!userStore) {
    Log::error("UserDiscovery: Cannot load discovery data - UserStore is null");
    return;
  }

  Log::info("UserDiscovery: Loading discovery data from UserStore");
  // UserStore will trigger state changes that notify our subscription
}

void UserDiscovery::refresh() {
  jassert(userStore != nullptr);
  loadDiscoveryData();
  repaint();
}

//==============================================================================
void UserDiscovery::performSearch(const juce::String &query) {
  jassert(userStore != nullptr);
  if (!userStore) {
    Log::warn("UserDiscovery: Cannot perform search - UserStore null");
    return;
  }

  Log::info("UserDiscovery: Performing search - query: \"" + query + "\"");
  currentSearchQuery = query;
  currentViewMode = ViewMode::SearchResults;
  isSearching = true;
  searchResults.clear();
  addToRecentSearches(query);

  // UserStore handles the search - will notify via subscription when results arrive
  // TODO: Add searchUsers method to UserStore
  rebuildUserCards();
  repaint();
}

void UserDiscovery::fetchTrendingUsers() {}
void UserDiscovery::fetchFeaturedProducers() {}
void UserDiscovery::fetchSuggestedUsers() {}
void UserDiscovery::fetchSimilarProducers() {}
void UserDiscovery::fetchRecommendedToFollow() {}
void UserDiscovery::fetchAvailableGenres() {}
void UserDiscovery::fetchUsersByGenre(const juce::String &genre) {}

void UserDiscovery::handleFollowToggle(const DiscoveredUser &user, bool willFollow) {
  jassert(userStore != nullptr);
  if (!userStore)
    return;

  // Optimistic UI update
  for (auto *card : userCards) {
    if (card->getUserId() == user.id) {
      card->setIsFollowing(willFollow);
      break;
    }
  }

  // TODO: Phase 2 - Add followUser/unfollowUser to UserStore
  // For now, store will handle follow operations when these methods are added
}

//==============================================================================
void UserDiscovery::loadRecentSearches() {
  auto file = getRecentSearchesFile();
  if (file.existsAsFile()) {
    auto content = file.loadFileAsString();
    recentSearches = juce::StringArray::fromLines(content);

    // Keep only MAX_RECENT_SEARCHES
    while (recentSearches.size() > MAX_RECENT_SEARCHES)
      recentSearches.remove(recentSearches.size() - 1);
  }
}

void UserDiscovery::saveRecentSearches() {
  auto file = getRecentSearchesFile();
  file.getParentDirectory().createDirectory();
  file.replaceWithText(recentSearches.joinIntoString("\n"));
}

void UserDiscovery::addToRecentSearches(const juce::String &query) {
  // Remove if already exists
  recentSearches.removeString(query);

  // Add to front
  recentSearches.insert(0, query);

  // Trim to max
  while (recentSearches.size() > MAX_RECENT_SEARCHES)
    recentSearches.remove(recentSearches.size() - 1);

  saveRecentSearches();
}

void UserDiscovery::clearRecentSearches() {
  recentSearches.clear();
  saveRecentSearches();
  repaint();
}

juce::File UserDiscovery::getRecentSearchesFile() const {
  auto dataDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("Sidechain");
  return dataDir.getChildFile("recent_searches.txt");
}

//==============================================================================
void UserDiscovery::rebuildUserCards() {
  // Remove all cards from component tree before deleting
  for (auto *card : userCards) {
    removeChildComponent(card);
  }
  userCards.clear();

  // Build cards based on view mode
  juce::Array<DiscoveredUser> *sourceArray = nullptr;

  switch (currentViewMode) {
  case ViewMode::SearchResults:
    sourceArray = &searchResults;
    break;

  case ViewMode::GenreFilter:
    sourceArray = &genreUsers;
    break;

  case ViewMode::Discovery: {
    // Combine all discovery sections from local member variables
    // Trending users
    for (const auto &user : trendingUsers) {
      auto *card = userCards.add(new UserCard());
      card->setUser(user);
      setupUserCardCallbacks(card);
      addAndMakeVisible(card);
    }

    // Featured producers
    for (const auto &user : featuredProducers) {
      auto *card = userCards.add(new UserCard());
      card->setUser(user);
      setupUserCardCallbacks(card);
      addAndMakeVisible(card);
    }

    // Suggested users
    for (const auto &user : suggestedUsers) {
      auto *card = userCards.add(new UserCard());
      card->setUser(user);
      setupUserCardCallbacks(card);
      addAndMakeVisible(card);
    }

    // Similar producers
    for (const auto &user : similarProducers) {
      auto *card = userCards.add(new UserCard());
      card->setUser(user);
      setupUserCardCallbacks(card);
      addAndMakeVisible(card);
    }

    // Recommended to follow
    for (const auto &user : recommendedToFollow) {
      auto *card = userCards.add(new UserCard());
      card->setUser(user);
      setupUserCardCallbacks(card);
      addAndMakeVisible(card);
    }

    updateUserCardPositions();
    updateScrollBounds();
    return;
  }
  }

  if (sourceArray != nullptr) {
    for (auto &user : *sourceArray) {
      auto *card = userCards.add(new UserCard());
      card->setUser(user);
      setupUserCardCallbacks(card);
      addAndMakeVisible(card);
    }
  }

  updateUserCardPositions();
  updateScrollBounds();

  // Query presence for all users after rebuilding cards
  juce::Array<DiscoveredUser> allUsers;
  switch (currentViewMode) {
  case ViewMode::SearchResults:
    allUsers = searchResults;
    break;
  case ViewMode::GenreFilter:
    allUsers = genreUsers;
    break;
  case ViewMode::Discovery:
    // Combine all discovery sections from local variables
    allUsers.addArray(trendingUsers);
    allUsers.addArray(featuredProducers);
    allUsers.addArray(suggestedUsers);
    allUsers.addArray(similarProducers);
    allUsers.addArray(recommendedToFollow);
    break;
  }

  if (!allUsers.isEmpty()) {
    queryPresenceForUsers(allUsers);
  }
}

void UserDiscovery::updateUserCardPositions() {
  auto contentBounds = getContentBounds();
  contentBounds.removeFromRight(14); // scrollbar space

  int y = contentBounds.getY() - scrollOffset;

  // Account for genre chips and section headers based on view mode
  switch (currentViewMode) {
  case ViewMode::Discovery: {
    y += GENRE_CHIP_HEIGHT + 24; // Genre chips
    int cardIndex = 0;

    // Trending section
    if (!trendingUsers.isEmpty()) {
      y += SECTION_HEADER_HEIGHT;
      for (int i = 0; i < trendingUsers.size() && cardIndex < userCards.size(); ++i, ++cardIndex) {
        userCards[cardIndex]->setBounds(contentBounds.getX(), y, contentBounds.getWidth(), USER_CARD_HEIGHT);
        y += USER_CARD_HEIGHT;
      }
      y += 16; // section spacing
    }

    // Featured section
    if (!featuredProducers.isEmpty()) {
      y += SECTION_HEADER_HEIGHT;
      for (int i = 0; i < featuredProducers.size() && cardIndex < userCards.size(); ++i, ++cardIndex) {
        userCards[cardIndex]->setBounds(contentBounds.getX(), y, contentBounds.getWidth(), USER_CARD_HEIGHT);
        y += USER_CARD_HEIGHT;
      }
      y += 16;
    }

    // Suggested section
    if (!suggestedUsers.isEmpty()) {
      y += SECTION_HEADER_HEIGHT;
      for (int i = 0; i < suggestedUsers.size() && cardIndex < userCards.size(); ++i, ++cardIndex) {
        userCards[cardIndex]->setBounds(contentBounds.getX(), y, contentBounds.getWidth(), USER_CARD_HEIGHT);
        y += USER_CARD_HEIGHT;
      }
      y += 16;
    }

    // Similar producers section
    if (!similarProducers.isEmpty()) {
      y += SECTION_HEADER_HEIGHT;
      for (int i = 0; i < similarProducers.size() && cardIndex < userCards.size(); ++i, ++cardIndex) {
        userCards[cardIndex]->setBounds(contentBounds.getX(), y, contentBounds.getWidth(), USER_CARD_HEIGHT);
        y += USER_CARD_HEIGHT;
      }
      y += 16;
    }

    // Recommended to follow section
    if (!recommendedToFollow.isEmpty()) {
      y += SECTION_HEADER_HEIGHT;
      for (int i = 0; i < recommendedToFollow.size() && cardIndex < userCards.size(); ++i, ++cardIndex) {
        userCards[cardIndex]->setBounds(contentBounds.getX(), y, contentBounds.getWidth(), USER_CARD_HEIGHT);
        y += USER_CARD_HEIGHT;
      }
    }
    break;
  }

  case ViewMode::SearchResults: {
    y += 30; // Result count header
    for (auto *card : userCards) {
      card->setBounds(contentBounds.getX(), y, contentBounds.getWidth(), USER_CARD_HEIGHT);
      y += USER_CARD_HEIGHT;
    }
    break;
  }

  case ViewMode::GenreFilter: {
    y += GENRE_CHIP_HEIGHT + 24 + SECTION_HEADER_HEIGHT;
    for (auto *card : userCards) {
      card->setBounds(contentBounds.getX(), y, contentBounds.getWidth(), USER_CARD_HEIGHT);
      y += USER_CARD_HEIGHT;
    }
    break;
  }
  }
}

void UserDiscovery::setupUserCardCallbacks(UserCard *card) {
  card->onUserClicked = [this](const DiscoveredUser &user) {
    if (onUserSelected)
      onUserSelected(user);
  };

  card->onFollowToggled = [this](const DiscoveredUser &user, bool willFollow) { handleFollowToggle(user, willFollow); };
}

int UserDiscovery::calculateContentHeight() const {
  int height = GENRE_CHIP_HEIGHT + 24; // genre chips

  switch (currentViewMode) {
  case ViewMode::Discovery:
    if (!trendingUsers.isEmpty())
      height += SECTION_HEADER_HEIGHT + (trendingUsers.size() * USER_CARD_HEIGHT) + 16;
    if (!featuredProducers.isEmpty())
      height += SECTION_HEADER_HEIGHT + (featuredProducers.size() * USER_CARD_HEIGHT) + 16;
    if (!suggestedUsers.isEmpty())
      height += SECTION_HEADER_HEIGHT + (suggestedUsers.size() * USER_CARD_HEIGHT) + 16;
    if (!similarProducers.isEmpty())
      height += SECTION_HEADER_HEIGHT + (similarProducers.size() * USER_CARD_HEIGHT) + 16;
    if (!recommendedToFollow.isEmpty())
      height += SECTION_HEADER_HEIGHT + (recommendedToFollow.size() * USER_CARD_HEIGHT);
    break;

  case ViewMode::SearchResults:
    height = 30 + (searchResults.size() * USER_CARD_HEIGHT);
    break;

  case ViewMode::GenreFilter:
    height += SECTION_HEADER_HEIGHT + (genreUsers.size() * USER_CARD_HEIGHT);
    break;
  }

  return height + 50; // extra padding at bottom
}

void UserDiscovery::updateScrollBounds() {
  auto contentBounds = getContentBounds();
  int totalHeight = calculateContentHeight();
  int visibleHeight = contentBounds.getHeight();

  scrollBar.setRangeLimits(0.0, static_cast<double>(juce::jmax(0, totalHeight - visibleHeight + 50)));
  scrollBar.setCurrentRange(scrollOffset, visibleHeight);
}

juce::Rectangle<int> UserDiscovery::getRecentSearchBounds(int index) const {
  auto contentBounds = getContentBounds();
  int y = contentBounds.getY() + 30 + (index * 36);
  return {PADDING, y, contentBounds.getWidth() - PADDING * 2, 36};
}

juce::Rectangle<int> UserDiscovery::getGenreChipBounds(int index) const {
  if (index >= availableGenres.size())
    return {};

  auto contentBounds = getContentBounds();
  int x = PADDING;
  int y = contentBounds.getY() + 8;
  int maxWidth = contentBounds.getRight() - PADDING;

  juce::Font font(juce::FontOptions().withHeight(12.0f));

  for (int i = 0; i <= index; ++i) {
    juce::GlyphArrangement glyphs;
    glyphs.addLineOfText(font, availableGenres[i], 0, 0);
    int textWidth = static_cast<int>(glyphs.getBoundingBox(0, -1, true).getWidth());
    int chipWidth = textWidth + 20;

    if (x + chipWidth > maxWidth) {
      x = PADDING;
      y += GENRE_CHIP_HEIGHT + 8;
    }

    if (i == index) {
      return {x, y - scrollOffset, chipWidth, GENRE_CHIP_HEIGHT};
    }

    x += chipWidth + 8;
  }

  return {};
}

//==============================================================================
void UserDiscovery::queryPresenceForUsers(const juce::Array<DiscoveredUser> &users) {
  if (!streamChatClient || users.isEmpty()) {
    Log::debug("UserDiscovery::queryPresenceForUsers: Skipping - "
               "streamChatClient is null or no users");
    return;
  }

  // Collect unique user IDs
  std::set<juce::String> uniqueUserIds;
  for (const auto &user : users) {
    if (user.id.isNotEmpty()) {
      uniqueUserIds.insert(user.id);
    }
  }

  if (uniqueUserIds.empty()) {
    Log::debug("UserDiscovery::queryPresenceForUsers: No unique user IDs to query");
    return;
  }

  // Convert to vector for queryPresence
  std::vector<juce::String> userIds(uniqueUserIds.begin(), uniqueUserIds.end());

  Log::debug("UserDiscovery::queryPresenceForUsers: Querying presence for " + juce::String(userIds.size()) + " users");

  // Query presence
  juce::Component::SafePointer<UserDiscovery> safeThis(this);
  streamChatClient->queryPresence(userIds, [safeThis](Outcome<std::vector<StreamChatClient::UserPresence>> result) {
    if (safeThis == nullptr)
      return; // Component was deleted

    if (result.isError()) {
      Log::warn("UserDiscovery::queryPresenceForUsers: Failed to query presence: " + result.getError());
      return;
    }

    auto presenceList = result.getValue();
    Log::debug("UserDiscovery::queryPresenceForUsers: Received presence data for " + juce::String(presenceList.size()) +
               " users");

    // Update user arrays with presence data
    auto updateUserPresenceInArray = [&presenceList](juce::Array<DiscoveredUser> &userArray) {
      for (auto &user : userArray) {
        for (const auto &presence : presenceList) {
          if (presence.userId == user.id) {
            user.isOnline = presence.online;
            user.isInStudio = (presence.status == "in_studio" || presence.status == "in studio");
            break;
          }
        }
      }
    };

    updateUserPresenceInArray(safeThis->searchResults);
    updateUserPresenceInArray(safeThis->trendingUsers);
    updateUserPresenceInArray(safeThis->featuredProducers);
    updateUserPresenceInArray(safeThis->suggestedUsers);
    updateUserPresenceInArray(safeThis->similarProducers);
    updateUserPresenceInArray(safeThis->recommendedToFollow);
    updateUserPresenceInArray(safeThis->genreUsers);

    // Update corresponding UserCards
    for (auto *card : safeThis->userCards) {
      auto user = card->getUser();
      for (const auto &presence : presenceList) {
        if (presence.userId == user.id) {
          user.isOnline = presence.online;
          user.isInStudio = (presence.status == "in_studio" || presence.status == "in studio");
          card->setUser(user);
          break;
        }
      }
    }

    // Repaint to show online indicators
    safeThis->repaint();
  });
}

void UserDiscovery::updateUserPresence(const juce::String &userId, bool isOnline, const juce::String &status) {
  if (userId.isEmpty())
    return;

  bool isInStudio = (status == "in_studio" || status == "in studio" || status == "recording");

  // Update presence in all user arrays
  auto updatePresenceInArray = [userId, isOnline, isInStudio](juce::Array<DiscoveredUser> &userArray) {
    for (auto &user : userArray) {
      if (user.id == userId) {
        user.isOnline = isOnline;
        user.isInStudio = isInStudio;
        break;
      }
    }
  };

  updatePresenceInArray(searchResults);
  updatePresenceInArray(trendingUsers);
  updatePresenceInArray(featuredProducers);
  updatePresenceInArray(suggestedUsers);
  updatePresenceInArray(similarProducers);
  updatePresenceInArray(recommendedToFollow);
  updatePresenceInArray(genreUsers);

  // Update corresponding UserCards
  for (auto *card : userCards) {
    auto user = card->getUser();
    if (user.id == userId) {
      user.isOnline = isOnline;
      user.isInStudio = isInStudio;
      card->setUser(user);
      break;
    }
  }

  // Repaint to show updated online indicators
  repaint();
}
