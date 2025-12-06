#include "UserDiscovery.h"
#include "../../network/NetworkClient.h"
#include "../../network/StreamChatClient.h"
#include "../../util/Json.h"
#include "../../util/Log.h"
#include "../../util/Result.h"
#include <set>
#include <vector>

//==============================================================================
UserDiscovery::UserDiscovery()
{
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

    // Note: Online status is now implemented - queries getstream.io Chat presence and shows online indicators on UserCards
}

UserDiscovery::~UserDiscovery()
{
    Log::debug("UserDiscovery: Destroying");
    searchBox->removeListener(this);
    scrollBar.removeListener(this);
}

//==============================================================================
void UserDiscovery::setNetworkClient(NetworkClient* client)
{
    networkClient = client;
    Log::debug("UserDiscovery: NetworkClient set " + juce::String(client != nullptr ? "(valid)" : "(null)"));
}

void UserDiscovery::setStreamChatClient(StreamChatClient* client)
{
    streamChatClient = client;
    Log::info("UserDiscovery::setStreamChatClient: StreamChatClient set " + juce::String(client != nullptr ? "(valid)" : "(null)"));
}

//==============================================================================
void UserDiscovery::paint(juce::Graphics& g)
{
    // Background
    g.fillAll(Colors::background);

    // Header
    drawHeader(g);

    // Content area based on view mode
    auto contentBounds = getContentBounds();
    contentBounds.translate(0, -scrollOffset);

    switch (currentViewMode)
    {
        case ViewMode::Discovery:
        {
            // Show recent searches if search box has focus
            if (searchBox->hasKeyboardFocus(true) && recentSearches.size() > 0 && currentSearchQuery.isEmpty())
            {
                drawRecentSearches(g, contentBounds);
            }

            // Genre chips for filtering
            drawGenreChips(g, contentBounds);
            contentBounds.removeFromTop(8);  // spacing

            // Trending section
            if (!trendingUsers.isEmpty())
            {
                drawSectionHeader(g, contentBounds.removeFromTop(SECTION_HEADER_HEIGHT), "Trending");
                drawTrendingSection(g, contentBounds);
                contentBounds.removeFromTop(16);
            }

            // Featured section
            if (!featuredProducers.isEmpty())
            {
                drawSectionHeader(g, contentBounds.removeFromTop(SECTION_HEADER_HEIGHT), "Featured Producers");
                drawFeaturedSection(g, contentBounds);
                contentBounds.removeFromTop(16);
            }

            // Suggested section
            if (!suggestedUsers.isEmpty())
            {
                drawSectionHeader(g, contentBounds.removeFromTop(SECTION_HEADER_HEIGHT), "Suggested For You");
                drawSuggestedSection(g, contentBounds);
            }

            // Loading state
            if (isTrendingLoading && isFeaturedLoading && isSuggestedLoading)
            {
                drawLoadingState(g, getContentBounds());
            }
            else if (trendingUsers.isEmpty() && featuredProducers.isEmpty() && suggestedUsers.isEmpty())
            {
                drawEmptyState(g, getContentBounds(), "No users to discover yet.\nBe the first to share your music!");
            }
            break;
        }

        case ViewMode::SearchResults:
        {
            if (isSearching)
            {
                drawLoadingState(g, contentBounds);
            }
            else if (searchResults.isEmpty())
            {
                drawEmptyState(g, contentBounds, "No users found for \"" + currentSearchQuery + "\"");
            }
            else
            {
                drawSearchResults(g, contentBounds);
            }
            break;
        }

        case ViewMode::GenreFilter:
        {
            drawGenreChips(g, contentBounds);
            contentBounds.removeFromTop(8);

            drawSectionHeader(g, contentBounds.removeFromTop(SECTION_HEADER_HEIGHT), selectedGenre + " Producers");

            if (genreUsers.isEmpty())
            {
                drawEmptyState(g, contentBounds, "No producers found in " + selectedGenre);
            }
            break;
        }
    }
}

void UserDiscovery::resized()
{
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
void UserDiscovery::drawHeader(juce::Graphics& g)
{
    auto headerBounds = getLocalBounds().removeFromTop(HEADER_HEIGHT);

    // Header background
    g.setColour(Colors::headerBg);
    g.fillRect(headerBounds);

    // Back button
    auto backBounds = getBackButtonBounds();
    g.setColour(Colors::textPrimary);
    g.setFont(juce::Font(24.0f));
    g.drawText("<", backBounds, juce::Justification::centred);

    // Title
    g.setFont(juce::Font(18.0f).boldened());
    auto titleBounds = headerBounds.withTrimmedLeft(50);
    g.drawText("Discover", titleBounds, juce::Justification::centredLeft);

    // Search bar area
    auto searchBounds = getSearchBoxBounds();
    g.setColour(Colors::searchBg);
    g.fillRoundedRectangle(searchBounds.reduced(4).toFloat(), 8.0f);

    // Search icon
    g.setColour(Colors::textPlaceholder);
    g.setFont(juce::Font(14.0f));
    auto iconBounds = searchBounds.removeFromLeft(40);
    g.drawText(juce::CharPointer_UTF8("\xF0\x9F\x94\x8D"), iconBounds, juce::Justification::centred);  // magnifying glass emoji

    // Clear button (X) when there's text
    if (currentSearchQuery.isNotEmpty())
    {
        auto clearBounds = getClearSearchBounds();
        g.setColour(Colors::textSecondary);
        g.setFont(juce::Font(16.0f));
        g.drawText("x", clearBounds, juce::Justification::centred);
    }
}

void UserDiscovery::drawRecentSearches(juce::Graphics& g, juce::Rectangle<int>& bounds)
{
    g.setFont(juce::Font(12.0f).boldened());
    g.setColour(Colors::sectionHeader);

    auto headerBounds = bounds.removeFromTop(30);
    headerBounds.removeFromLeft(PADDING);
    g.drawText("RECENT SEARCHES", headerBounds, juce::Justification::centredLeft);

    g.setFont(juce::Font(14.0f));
    g.setColour(Colors::textPrimary);

    for (int i = 0; i < recentSearches.size() && i < MAX_RECENT_SEARCHES; ++i)
    {
        auto itemBounds = bounds.removeFromTop(36);
        itemBounds.removeFromLeft(PADDING);

        // Clock icon
        g.setColour(Colors::textSecondary);
        g.drawText(juce::CharPointer_UTF8("\xE2\x8F\xB1"), itemBounds.removeFromLeft(24), juce::Justification::centredLeft);  // clock icon

        g.setColour(Colors::textPrimary);
        g.drawText(recentSearches[i], itemBounds, juce::Justification::centredLeft);
    }

    bounds.removeFromTop(8);
}

void UserDiscovery::drawSectionHeader(juce::Graphics& g, juce::Rectangle<int> bounds, const juce::String& title)
{
    bounds.removeFromLeft(PADDING);

    g.setColour(Colors::textPrimary);
    g.setFont(juce::Font(14.0f).boldened());
    g.drawText(title, bounds, juce::Justification::centredLeft);
}

void UserDiscovery::drawTrendingSection(juce::Graphics& g, juce::Rectangle<int>& bounds)
{
    // User cards are drawn by their own components
    for (int i = 0; i < juce::jmin(5, trendingUsers.size()); ++i)
    {
        bounds.removeFromTop(USER_CARD_HEIGHT);
    }
}

void UserDiscovery::drawFeaturedSection(juce::Graphics& g, juce::Rectangle<int>& bounds)
{
    for (int i = 0; i < juce::jmin(5, featuredProducers.size()); ++i)
    {
        bounds.removeFromTop(USER_CARD_HEIGHT);
    }
}

void UserDiscovery::drawSuggestedSection(juce::Graphics& g, juce::Rectangle<int>& bounds)
{
    for (int i = 0; i < juce::jmin(5, suggestedUsers.size()); ++i)
    {
        bounds.removeFromTop(USER_CARD_HEIGHT);
    }
}

void UserDiscovery::drawGenreChips(juce::Graphics& g, juce::Rectangle<int>& bounds)
{
    if (availableGenres.isEmpty())
        return;

    auto chipArea = bounds.removeFromTop(GENRE_CHIP_HEIGHT + 16);
    chipArea = chipArea.reduced(PADDING, 8);

    g.setFont(juce::Font(12.0f));

    int x = chipArea.getX();
    int y = chipArea.getY();
    int maxWidth = chipArea.getRight() - PADDING;

    for (int i = 0; i < availableGenres.size(); ++i)
    {
        auto genre = availableGenres[i];
        auto textWidth = g.getCurrentFont().getStringWidth(genre);
        auto chipWidth = textWidth + 20;

        // Wrap to next line if needed
        if (x + chipWidth > maxWidth)
        {
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

void UserDiscovery::drawSearchResults(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    // Search results are drawn by UserCardComponents
    g.setColour(Colors::textSecondary);
    g.setFont(juce::Font(12.0f));

    auto resultCount = bounds.removeFromTop(30).reduced(PADDING, 0);
    g.drawText(juce::String(searchResults.size()) + " results for \"" + currentSearchQuery + "\"",
               resultCount, juce::Justification::centredLeft);
}

void UserDiscovery::drawLoadingState(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    g.setColour(Colors::textSecondary);
    g.setFont(juce::Font(14.0f));
    g.drawText("Loading...", bounds, juce::Justification::centred);
}

void UserDiscovery::drawEmptyState(juce::Graphics& g, juce::Rectangle<int> bounds, const juce::String& message)
{
    g.setColour(Colors::textSecondary);
    g.setFont(juce::Font(14.0f));

    // Center the message
    auto textBounds = bounds.withSizeKeepingCentre(bounds.getWidth() - 40, 60);
    g.drawFittedText(message, textBounds, juce::Justification::centred, 3);
}

//==============================================================================
juce::Rectangle<int> UserDiscovery::getBackButtonBounds() const
{
    return { 8, 12, 40, 36 };
}

juce::Rectangle<int> UserDiscovery::getSearchBoxBounds() const
{
    auto bounds = getLocalBounds();
    bounds.removeFromTop(HEADER_HEIGHT);
    return bounds.removeFromTop(SEARCH_BAR_HEIGHT + 8).reduced(PADDING - 8, 4);
}

juce::Rectangle<int> UserDiscovery::getClearSearchBounds() const
{
    auto searchBounds = getSearchBoxBounds();
    return searchBounds.removeFromRight(36);
}

juce::Rectangle<int> UserDiscovery::getContentBounds() const
{
    auto bounds = getLocalBounds();
    bounds.removeFromTop(HEADER_HEIGHT + SEARCH_BAR_HEIGHT + 8);
    return bounds;
}

//==============================================================================
void UserDiscovery::mouseUp(const juce::MouseEvent& event)
{
    auto point = event.getPosition();

    // Back button
    if (getBackButtonBounds().contains(point))
    {
        if (currentViewMode == ViewMode::SearchResults || currentViewMode == ViewMode::GenreFilter)
        {
            // Go back to discovery view
            currentViewMode = ViewMode::Discovery;
            currentSearchQuery.clear();
            selectedGenre.clear();
            searchBox->clear();
            rebuildUserCards();
            repaint();
        }
        else if (onBackPressed)
        {
            onBackPressed();
        }
        return;
    }

    // Clear search button
    if (currentSearchQuery.isNotEmpty() && getClearSearchBounds().contains(point))
    {
        currentSearchQuery.clear();
        searchBox->clear();
        currentViewMode = ViewMode::Discovery;
        searchResults.clear();
        rebuildUserCards();
        repaint();
        return;
    }

    // Genre chips
    if (point.y > HEADER_HEIGHT + SEARCH_BAR_HEIGHT && point.y < HEADER_HEIGHT + SEARCH_BAR_HEIGHT + GENRE_CHIP_HEIGHT + 24)
    {
        // Check which genre was clicked
        for (int i = 0; i < availableGenres.size(); ++i)
        {
            auto chipBounds = getGenreChipBounds(i);
            if (chipBounds.contains(point))
            {
                if (selectedGenre == availableGenres[i])
                {
                    // Deselect - go back to discovery
                    selectedGenre.clear();
                    currentViewMode = ViewMode::Discovery;
                    rebuildUserCards();
                }
                else
                {
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
    if (searchBox->hasKeyboardFocus(true) && currentSearchQuery.isEmpty())
    {
        for (int i = 0; i < recentSearches.size(); ++i)
        {
            auto itemBounds = getRecentSearchBounds(i);
            if (itemBounds.contains(point))
            {
                searchBox->setText(recentSearches[i]);
                performSearch(recentSearches[i]);
                return;
            }
        }
    }
}

void UserDiscovery::textEditorTextChanged(juce::TextEditor& editor)
{
    currentSearchQuery = editor.getText();

    if (currentSearchQuery.isEmpty())
    {
        currentViewMode = ViewMode::Discovery;
        searchResults.clear();
        rebuildUserCards();
        repaint();
    }
}

void UserDiscovery::textEditorReturnKeyPressed(juce::TextEditor& editor)
{
    auto query = editor.getText().trim();
    if (query.isNotEmpty())
    {
        performSearch(query);
    }
}

void UserDiscovery::scrollBarMoved(juce::ScrollBar* bar, double newRangeStart)
{
    scrollOffset = static_cast<int>(newRangeStart);
    updateUserCardPositions();
    repaint();
}

//==============================================================================
void UserDiscovery::loadDiscoveryData()
{
    if (networkClient == nullptr)
    {
        Log::warn("UserDiscovery: Cannot load discovery data - network client null");
        return;
    }

    Log::info("UserDiscovery: Loading discovery data");
    fetchTrendingUsers();
    fetchFeaturedProducers();
    fetchSuggestedUsers();
    fetchAvailableGenres();
}

void UserDiscovery::refresh()
{
    trendingUsers.clear();
    featuredProducers.clear();
    suggestedUsers.clear();
    genreUsers.clear();

    isTrendingLoading = true;
    isFeaturedLoading = true;
    isSuggestedLoading = true;

    userCards.clear();
    loadDiscoveryData();
    repaint();
}

//==============================================================================
void UserDiscovery::performSearch(const juce::String& query)
{
    if (networkClient == nullptr)
    {
        Log::warn("UserDiscovery: Cannot perform search - network client null");
        return;
    }

    Log::info("UserDiscovery: Performing search - query: \"" + query + "\"");
    currentSearchQuery = query;
    currentViewMode = ViewMode::SearchResults;
    isSearching = true;
    searchResults.clear();
    addToRecentSearches(query);
    repaint();

    networkClient->searchUsers(query, 30, 0, [this](Outcome<juce::var> responseOutcome) {
        isSearching = false;

        if (responseOutcome.isOk())
        {
            auto response = responseOutcome.getValue();
            if (Json::isObject(response))
        {
            auto users = Json::getArray(response, "users");
            if (Json::isArray(users))
            {
                for (int i = 0; i < users.size(); ++i)
                {
                    searchResults.add(DiscoveredUser::fromJson(users[i]));
                }
            }
                Log::info("UserDiscovery: Search completed - results: " + juce::String(searchResults.size()));
            }
            else
            {
                Log::error("UserDiscovery: Invalid search response");
            }
        }
        else
        {
            Log::error("UserDiscovery: Search failed - " + responseOutcome.getError());
        }

        rebuildUserCards();
        repaint();
    });
}

void UserDiscovery::fetchTrendingUsers()
{
    if (networkClient == nullptr)
        return;

    isTrendingLoading = true;

    networkClient->getTrendingUsers(10, [this](Outcome<juce::var> responseOutcome) {
        isTrendingLoading = false;

        if (responseOutcome.isOk())
        {
            auto response = responseOutcome.getValue();
            if (Json::isObject(response))
            {
                auto users = Json::getArray(response, "users");
                if (Json::isArray(users))
                {
                    trendingUsers.clear();
                    for (int i = 0; i < users.size(); ++i)
                    {
                        trendingUsers.add(DiscoveredUser::fromJson(users[i]));
                    }
                    Log::info("UserDiscovery: Loaded " + juce::String(trendingUsers.size()) + " trending users");
                }
            }
            else
            {
                Log::error("UserDiscovery: Invalid trending users response");
            }
        }
        else
        {
            Log::error("UserDiscovery: Failed to load trending users - " + responseOutcome.getError());
        }

        rebuildUserCards();
        repaint();
    });
}

void UserDiscovery::fetchFeaturedProducers()
{
    if (networkClient == nullptr)
        return;

    isFeaturedLoading = true;

    networkClient->getFeaturedProducers(10, [this](Outcome<juce::var> responseOutcome) {
        isFeaturedLoading = false;

        if (responseOutcome.isOk())
        {
            auto response = responseOutcome.getValue();
            if (Json::isObject(response))
            {
                auto users = Json::getArray(response, "users");
                if (Json::isArray(users))
                {
                    featuredProducers.clear();
                    for (int i = 0; i < users.size(); ++i)
                    {
                        featuredProducers.add(DiscoveredUser::fromJson(users[i]));
                    }
                }
            }
            else
            {
                Log::error("UserDiscovery: Invalid featured producers response");
            }
        }
        else
        {
            Log::error("UserDiscovery: Failed to load featured producers - " + responseOutcome.getError());
        }

        rebuildUserCards();
        repaint();
    });
}

void UserDiscovery::fetchSuggestedUsers()
{
    if (networkClient == nullptr)
        return;

    isSuggestedLoading = true;

    networkClient->getSuggestedUsers(10, [this](Outcome<juce::var> responseOutcome) {
        isSuggestedLoading = false;

        if (responseOutcome.isOk())
        {
            auto response = responseOutcome.getValue();
            if (Json::isObject(response))
            {
                auto users = Json::getArray(response, "users");
                if (Json::isArray(users))
                {
                    suggestedUsers.clear();
                    for (int i = 0; i < users.size(); ++i)
                    {
                        suggestedUsers.add(DiscoveredUser::fromJson(users[i]));
                    }
                }
            }
            else
            {
                Log::error("UserDiscovery: Invalid suggested users response");
            }
        }
        else
        {
            Log::error("UserDiscovery: Failed to load suggested users - " + responseOutcome.getError());
        }

        rebuildUserCards();
        repaint();
    });
}

void UserDiscovery::fetchAvailableGenres()
{
    if (networkClient == nullptr)
        return;

    isGenresLoading = true;

    networkClient->getAvailableGenres([this](Outcome<juce::var> responseOutcome) {
        isGenresLoading = false;

        if (responseOutcome.isOk())
        {
            auto response = responseOutcome.getValue();
            if (Json::isObject(response))
            {
                auto genres = Json::getArray(response, "genres");
                if (Json::isArray(genres))
                {
                    availableGenres.clear();
                    for (int i = 0; i < Json::arraySize(genres); ++i)
                    {
                        availableGenres.add(Json::getStringAt(genres, i));
                    }
                }
            }
            else
            {
                Log::error("UserDiscovery: Invalid genres response");
            }
        }
        else
        {
            Log::error("UserDiscovery: Failed to load genres - " + responseOutcome.getError());
        }

        repaint();
    });
}

void UserDiscovery::fetchUsersByGenre(const juce::String& genre)
{
    if (networkClient == nullptr)
        return;

    genreUsers.clear();
    repaint();

    networkClient->getUsersByGenre(genre, 30, 0, [this](Outcome<juce::var> responseOutcome) {
        if (responseOutcome.isOk())
        {
            auto response = responseOutcome.getValue();
            if (Json::isObject(response))
            {
                auto users = Json::getArray(response, "users");
                if (Json::isArray(users))
                {
                    for (int i = 0; i < users.size(); ++i)
                    {
                        genreUsers.add(DiscoveredUser::fromJson(users[i]));
                    }
                }
            }
            else
            {
                Log::error("UserDiscovery: Invalid genre users response");
            }
        }
        else
        {
            Log::error("UserDiscovery: Failed to load genre users - " + responseOutcome.getError());
        }

        rebuildUserCards();
        repaint();
    });
}

void UserDiscovery::handleFollowToggle(const DiscoveredUser& user, bool willFollow)
{
    if (networkClient == nullptr)
        return;

    // Optimistic UI update
    for (auto* card : userCards)
    {
        if (card->getUserId() == user.id)
        {
            card->setIsFollowing(willFollow);
            break;
        }
    }

    // Send to backend
    if (willFollow)
    {
        networkClient->followUser(user.id);
    }
    // Note: unfollowUser would go here if implemented
}

//==============================================================================
void UserDiscovery::loadRecentSearches()
{
    auto file = getRecentSearchesFile();
    if (file.existsAsFile())
    {
        auto content = file.loadFileAsString();
        recentSearches = juce::StringArray::fromLines(content);

        // Keep only MAX_RECENT_SEARCHES
        while (recentSearches.size() > MAX_RECENT_SEARCHES)
            recentSearches.remove(recentSearches.size() - 1);
    }
}

void UserDiscovery::saveRecentSearches()
{
    auto file = getRecentSearchesFile();
    file.getParentDirectory().createDirectory();
    file.replaceWithText(recentSearches.joinIntoString("\n"));
}

void UserDiscovery::addToRecentSearches(const juce::String& query)
{
    // Remove if already exists
    recentSearches.removeString(query);

    // Add to front
    recentSearches.insert(0, query);

    // Trim to max
    while (recentSearches.size() > MAX_RECENT_SEARCHES)
        recentSearches.remove(recentSearches.size() - 1);

    saveRecentSearches();
}

void UserDiscovery::clearRecentSearches()
{
    recentSearches.clear();
    saveRecentSearches();
    repaint();
}

juce::File UserDiscovery::getRecentSearchesFile() const
{
    auto dataDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                       .getChildFile("Sidechain");
    return dataDir.getChildFile("recent_searches.txt");
}

//==============================================================================
void UserDiscovery::rebuildUserCards()
{
    userCards.clear();

    // Build cards based on view mode
    juce::Array<DiscoveredUser>* sourceArray = nullptr;

    switch (currentViewMode)
    {
        case ViewMode::SearchResults:
            sourceArray = &searchResults;
            break;

        case ViewMode::GenreFilter:
            sourceArray = &genreUsers;
            break;

        case ViewMode::Discovery:
        {
            // Combine all discovery sections
            // Trending users
            for (auto& user : trendingUsers)
            {
                auto* card = userCards.add(new UserCard());
                card->setUser(user);
                setupUserCardCallbacks(card);
                addAndMakeVisible(card);
            }

            // Featured producers
            for (auto& user : featuredProducers)
            {
                auto* card = userCards.add(new UserCard());
                card->setUser(user);
                setupUserCardCallbacks(card);
                addAndMakeVisible(card);
            }

            // Suggested users
            for (auto& user : suggestedUsers)
            {
                auto* card = userCards.add(new UserCard());
                card->setUser(user);
                setupUserCardCallbacks(card);
                addAndMakeVisible(card);
            }

            updateUserCardPositions();
            updateScrollBounds();
            return;
        }
    }

    if (sourceArray != nullptr)
    {
        for (auto& user : *sourceArray)
        {
            auto* card = userCards.add(new UserCard());
            card->setUser(user);
            setupUserCardCallbacks(card);
            addAndMakeVisible(card);
        }
    }

    updateUserCardPositions();
    updateScrollBounds();

    // Query presence for all users after rebuilding cards
    juce::Array<DiscoveredUser> allUsers;
    switch (currentViewMode)
    {
        case ViewMode::SearchResults:
            allUsers = searchResults;
            break;
        case ViewMode::GenreFilter:
            allUsers = genreUsers;
            break;
        case ViewMode::Discovery:
            // Combine all discovery sections
            allUsers.addArray(trendingUsers);
            allUsers.addArray(featuredProducers);
            allUsers.addArray(suggestedUsers);
            break;
    }

    if (!allUsers.isEmpty())
    {
        queryPresenceForUsers(allUsers);
    }
}

void UserDiscovery::updateUserCardPositions()
{
    auto contentBounds = getContentBounds();
    contentBounds.removeFromRight(14);  // scrollbar space

    int y = contentBounds.getY() - scrollOffset;

    // Account for genre chips and section headers based on view mode
    switch (currentViewMode)
    {
        case ViewMode::Discovery:
        {
            y += GENRE_CHIP_HEIGHT + 24;  // Genre chips

            int cardIndex = 0;

            // Trending section
            if (!trendingUsers.isEmpty())
            {
                y += SECTION_HEADER_HEIGHT;
                for (int i = 0; i < trendingUsers.size() && cardIndex < userCards.size(); ++i, ++cardIndex)
                {
                    userCards[cardIndex]->setBounds(contentBounds.getX(), y, contentBounds.getWidth(), USER_CARD_HEIGHT);
                    y += USER_CARD_HEIGHT;
                }
                y += 16;  // section spacing
            }

            // Featured section
            if (!featuredProducers.isEmpty())
            {
                y += SECTION_HEADER_HEIGHT;
                for (int i = 0; i < featuredProducers.size() && cardIndex < userCards.size(); ++i, ++cardIndex)
                {
                    userCards[cardIndex]->setBounds(contentBounds.getX(), y, contentBounds.getWidth(), USER_CARD_HEIGHT);
                    y += USER_CARD_HEIGHT;
                }
                y += 16;
            }

            // Suggested section
            if (!suggestedUsers.isEmpty())
            {
                y += SECTION_HEADER_HEIGHT;
                for (int i = 0; i < suggestedUsers.size() && cardIndex < userCards.size(); ++i, ++cardIndex)
                {
                    userCards[cardIndex]->setBounds(contentBounds.getX(), y, contentBounds.getWidth(), USER_CARD_HEIGHT);
                    y += USER_CARD_HEIGHT;
                }
            }
            break;
        }

        case ViewMode::SearchResults:
        {
            y += 30;  // Result count header
            for (auto* card : userCards)
            {
                card->setBounds(contentBounds.getX(), y, contentBounds.getWidth(), USER_CARD_HEIGHT);
                y += USER_CARD_HEIGHT;
            }
            break;
        }

        case ViewMode::GenreFilter:
        {
            y += GENRE_CHIP_HEIGHT + 24 + SECTION_HEADER_HEIGHT;
            for (auto* card : userCards)
            {
                card->setBounds(contentBounds.getX(), y, contentBounds.getWidth(), USER_CARD_HEIGHT);
                y += USER_CARD_HEIGHT;
            }
            break;
        }
    }
}

void UserDiscovery::setupUserCardCallbacks(UserCard* card)
{
    card->onUserClicked = [this](const DiscoveredUser& user) {
        if (onUserSelected)
            onUserSelected(user);
    };

    card->onFollowToggled = [this](const DiscoveredUser& user, bool willFollow) {
        handleFollowToggle(user, willFollow);
    };
}

int UserDiscovery::calculateContentHeight() const
{
    int height = GENRE_CHIP_HEIGHT + 24;  // genre chips

    switch (currentViewMode)
    {
        case ViewMode::Discovery:
            if (!trendingUsers.isEmpty())
                height += SECTION_HEADER_HEIGHT + (trendingUsers.size() * USER_CARD_HEIGHT) + 16;
            if (!featuredProducers.isEmpty())
                height += SECTION_HEADER_HEIGHT + (featuredProducers.size() * USER_CARD_HEIGHT) + 16;
            if (!suggestedUsers.isEmpty())
                height += SECTION_HEADER_HEIGHT + (suggestedUsers.size() * USER_CARD_HEIGHT);
            break;

        case ViewMode::SearchResults:
            height = 30 + (searchResults.size() * USER_CARD_HEIGHT);
            break;

        case ViewMode::GenreFilter:
            height += SECTION_HEADER_HEIGHT + (genreUsers.size() * USER_CARD_HEIGHT);
            break;
    }

    return height + 50;  // extra padding at bottom
}

void UserDiscovery::updateScrollBounds()
{
    auto contentBounds = getContentBounds();
    int totalHeight = calculateContentHeight();
    int visibleHeight = contentBounds.getHeight();

    scrollBar.setRangeLimits(0.0, static_cast<double>(juce::jmax(0, totalHeight - visibleHeight + 50)));
    scrollBar.setCurrentRange(scrollOffset, visibleHeight);
}

juce::Rectangle<int> UserDiscovery::getRecentSearchBounds(int index) const
{
    auto contentBounds = getContentBounds();
    int y = contentBounds.getY() + 30 + (index * 36);
    return { PADDING, y, contentBounds.getWidth() - PADDING * 2, 36 };
}

juce::Rectangle<int> UserDiscovery::getGenreChipBounds(int index) const
{
    if (index >= availableGenres.size())
        return {};

    auto contentBounds = getContentBounds();
    int x = PADDING;
    int y = contentBounds.getY() + 8;
    int maxWidth = contentBounds.getRight() - PADDING;

    juce::Font font(12.0f);

    for (int i = 0; i <= index; ++i)
    {
        int textWidth = font.getStringWidth(availableGenres[i]);
        int chipWidth = textWidth + 20;

        if (x + chipWidth > maxWidth)
        {
            x = PADDING;
            y += GENRE_CHIP_HEIGHT + 8;
        }

        if (i == index)
        {
            return { x, y - scrollOffset, chipWidth, GENRE_CHIP_HEIGHT };
        }

        x += chipWidth + 8;
    }

    return {};
}

//==============================================================================
void UserDiscovery::queryPresenceForUsers(const juce::Array<DiscoveredUser>& users)
{
    if (!streamChatClient || users.isEmpty())
    {
        Log::debug("UserDiscovery::queryPresenceForUsers: Skipping - streamChatClient is null or no users");
        return;
    }

    // Collect unique user IDs
    std::set<juce::String> uniqueUserIds;
    for (const auto& user : users)
    {
        if (user.id.isNotEmpty())
        {
            uniqueUserIds.insert(user.id);
        }
    }

    if (uniqueUserIds.empty())
    {
        Log::debug("UserDiscovery::queryPresenceForUsers: No unique user IDs to query");
        return;
    }

    // Convert to vector for queryPresence
    std::vector<juce::String> userIds(uniqueUserIds.begin(), uniqueUserIds.end());

    Log::debug("UserDiscovery::queryPresenceForUsers: Querying presence for " + juce::String(userIds.size()) + " users");

    // Query presence
    streamChatClient->queryPresence(userIds, [this](Outcome<std::vector<StreamChatClient::UserPresence>> result) {
        if (result.isError())
        {
            Log::warn("UserDiscovery::queryPresenceForUsers: Failed to query presence: " + result.getError());
            return;
        }

        auto presenceList = result.getValue();
        Log::debug("UserDiscovery::queryPresenceForUsers: Received presence data for " + juce::String(presenceList.size()) + " users");

        // Update user arrays with presence data
        auto updateUserPresence = [&presenceList](juce::Array<DiscoveredUser>& userArray) {
            for (auto& user : userArray)
            {
                for (const auto& presence : presenceList)
                {
                    if (presence.userId == user.id)
                    {
                        user.isOnline = presence.online;
                        user.isInStudio = (presence.status == "in_studio" || presence.status == "in studio");
                        break;
                    }
                }
            }
        };

        updateUserPresence(searchResults);
        updateUserPresence(trendingUsers);
        updateUserPresence(featuredProducers);
        updateUserPresence(suggestedUsers);
        updateUserPresence(genreUsers);

        // Update corresponding UserCards
        for (auto* card : userCards)
        {
            auto user = card->getUser();
            for (const auto& presence : presenceList)
            {
                if (presence.userId == user.id)
                {
                    user.isOnline = presence.online;
                    user.isInStudio = (presence.status == "in_studio" || presence.status == "in studio");
                    card->setUser(user);
                    break;
                }
            }
        }

        // Repaint to show online indicators
        repaint();
    });
}
