#include "SearchComponent.h"
#include "../../network/NetworkClient.h"
#include "../../util/Colors.h"
#include "../../util/Constants.h"
#include "../../util/Time.h"
#include "../../util/Log.h"
#include "../social/UserCardComponent.h"
#include <fstream>

//==============================================================================
SearchComponent::SearchComponent()
{
    setSize(1000, 700);
    Log::info("SearchComponent: Initializing");

    // Create search input
    searchInput = std::make_unique<juce::TextEditor>();
    searchInput->setMultiLine(false);
    searchInput->setReturnKeyStartsNewLine(false);
    searchInput->setReadOnly(false);
    searchInput->setScrollbarsShown(false);
    searchInput->setCaretVisible(true);
    searchInput->setPopupMenuEnabled(true);
    searchInput->setTextToShowWhenEmpty("Search users and posts...", SidechainColors::textMuted());
    searchInput->setFont(juce::Font(16.0f));
    searchInput->addListener(this);
    addAndMakeVisible(searchInput.get());

    // Create scrollbar (vertical)
    scrollBar = std::make_unique<juce::ScrollBar>(true);
    scrollBar->addListener(this);
    addAndMakeVisible(scrollBar.get());

    // Load recent searches
    loadRecentSearches();
    loadTrendingSearches(); // Load trending searches (7.3.6)
    loadAvailableGenres();

    // Start timer for debouncing search
    startTimer(300); // 300ms debounce
}

SearchComponent::~SearchComponent()
{
    stopTimer();
}

//==============================================================================
void SearchComponent::setNetworkClient(NetworkClient* client)
{
    networkClient = client;
    Log::debug("SearchComponent: NetworkClient set " + juce::String(client != nullptr ? "(valid)" : "(null)"));
}

//==============================================================================
void SearchComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    // Background
    g.setColour(SidechainColors::background());
    g.fillRect(bounds);

    // Draw sections based on state
    drawHeader(g);
    drawTabs(g);

    if (searchState == SearchState::Empty)
    {
        drawEmptyState(g);
    }
    else if (searchState == SearchState::Searching)
    {
        g.setColour(SidechainColors::textMuted());
        g.setFont(16.0f);
        g.drawText("Searching...", getResultsBounds(), juce::Justification::centred);
    }
    else if (searchState == SearchState::NoResults)
    {
        drawNoResultsState(g);
    }
    else if (searchState == SearchState::Error)
    {
        drawErrorState(g);
    }
    else if (searchState == SearchState::Results)
    {
        drawResults(g);
    }

    // Draw filters if we have a query
    if (!currentQuery.isEmpty())
    {
        drawFilters(g);
    }
}

void SearchComponent::resized()
{
    layoutComponents();
}

void SearchComponent::mouseUp(const juce::MouseEvent& event)
{
    auto pos = event.getPosition();

    // Handle tab clicks
    if (usersTabBounds.contains(pos) && currentTab != ResultTab::Users)
    {
        switchTab(ResultTab::Users);
        repaint();
    }
    else if (postsTabBounds.contains(pos) && currentTab != ResultTab::Posts)
    {
        switchTab(ResultTab::Posts);
        repaint();
    }

    // Handle back button
    if (backButtonBounds.contains(pos))
    {
        if (onBackPressed)
            onBackPressed();
    }

    // Handle clear button
    if (clearButtonBounds.contains(pos) && !currentQuery.isEmpty())
    {
        clearSearch();
    }

    // Handle filter clicks (genre, BPM, key) (7.3.4)
    if (genreFilterBounds.contains(pos))
    {
        showGenrePicker();
        return;
    }
    if (bpmFilterBounds.contains(pos))
    {
        showBPMPicker();
        return;
    }
    if (keyFilterBounds.contains(pos))
    {
        showKeyPicker();
        return;
    }

    // Handle recent/trending search clicks
    if (searchState == SearchState::Empty)
    {
        // Check if clicked on recent search item
        auto recentBounds = getResultsBounds();
        int yOffset = HEADER_HEIGHT + FILTER_HEIGHT + 40;
        for (int i = 0; i < recentSearches.size() && i < 5; ++i)
        {
            juce::Rectangle<int> itemBounds(20, yOffset + i * 40, getWidth() - 40, 35);
            if (itemBounds.contains(pos))
            {
                searchInput->setText(recentSearches[i]);
                currentQuery = recentSearches[i];
                performSearch();
                return;
            }
        }
        
        // Check if clicked on trending search item (7.3.6)
        yOffset = HEADER_HEIGHT + FILTER_HEIGHT + 40 + (recentSearches.size() > 0 ? (recentSearches.size() * 40 + 40) : 0);
        for (int i = 0; i < trendingSearches.size() && i < 5; ++i)
        {
            juce::Rectangle<int> itemBounds(20, yOffset + i * 40, getWidth() - 40, 35);
            if (itemBounds.contains(pos))
            {
                searchInput->setText(trendingSearches[i]);
                currentQuery = trendingSearches[i];
                performSearch();
                return;
            }
        }
    }
}

void SearchComponent::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    if (scrollBar->isVisible())
    {
        double newPos = scrollPosition - wheel.deltaY * 30.0;
        newPos = juce::jmax(0.0, juce::jmin(newPos, static_cast<double>(totalContentHeight - getResultsBounds().getHeight())));
        scrollPosition = newPos;
        scrollBar->setCurrentRangeStart(newPos);
        repaint();
    }
}

//==============================================================================
void SearchComponent::textEditorTextChanged(juce::TextEditor& editor)
{
    if (&editor == searchInput.get())
    {
        juce::String newQuery = editor.getText().trim();
        
        if (newQuery != currentQuery)
        {
            currentQuery = newQuery;
            
            if (currentQuery.isEmpty())
            {
                searchState = SearchState::Empty;
                userResults.clear();
                postResults.clear();
                selectedResultIndex = -1; // Reset keyboard navigation (7.3.8)
                repaint();
            }
            else
            {
                // Restart timer for debouncing
                stopTimer();
                startTimer(300);
            }
        }
    }
}

void SearchComponent::textEditorReturnKeyPressed(juce::TextEditor& editor)
{
    if (&editor == searchInput.get())
    {
        performSearch();
    }
}

void SearchComponent::textEditorEscapeKeyPressed(juce::TextEditor& editor)
{
    if (&editor == searchInput.get())
    {
        if (onBackPressed)
            onBackPressed();
    }
}

//==============================================================================
void SearchComponent::scrollBarMoved(juce::ScrollBar* scrollBar, double newRangeStart)
{
    if (scrollBar == this->scrollBar.get())
    {
        scrollPosition = newRangeStart;
        repaint();
    }
}

//==============================================================================
bool SearchComponent::keyPressed(const juce::KeyPress& key, juce::Component* originatingComponent)
{
    // Tab key to switch between Users/Posts
    if (key.getKeyCode() == juce::KeyPress::tabKey)
    {
        switchTab(currentTab == ResultTab::Users ? ResultTab::Posts : ResultTab::Users);
        return true;
    }

    // Up/Down arrows for navigation (7.3.8)
    if (searchState == SearchState::Results)
    {
        int maxResults = (currentTab == ResultTab::Users) ? userResults.size() : postResults.size();
        
        if (key.getKeyCode() == juce::KeyPress::downKey)
        {
            if (selectedResultIndex < maxResults - 1)
            {
                selectedResultIndex++;
                // Auto-scroll to keep selected item visible
                auto bounds = getResultsBounds();
                int itemY = HEADER_HEIGHT + FILTER_HEIGHT + 40 + (selectedResultIndex * CARD_HEIGHT);
                if (itemY + CARD_HEIGHT > bounds.getBottom())
                {
                    scrollPosition = static_cast<double>((selectedResultIndex + 1) * CARD_HEIGHT - bounds.getHeight());
                    scrollBar->setCurrentRangeStart(scrollPosition);
                }
                repaint();
            }
            return true;
        }
        else if (key.getKeyCode() == juce::KeyPress::upKey)
        {
            if (selectedResultIndex > 0)
            {
                selectedResultIndex--;
                // Auto-scroll to keep selected item visible
                auto bounds = getResultsBounds();
                int itemY = HEADER_HEIGHT + FILTER_HEIGHT + 40 + (selectedResultIndex * CARD_HEIGHT);
                if (itemY < bounds.getY())
                {
                    scrollPosition = static_cast<double>(selectedResultIndex * CARD_HEIGHT);
                    scrollBar->setCurrentRangeStart(scrollPosition);
                }
                repaint();
            }
            return true;
        }
        else if (key.getKeyCode() == juce::KeyPress::returnKey)
        {
            // Select the highlighted item
            if (selectedResultIndex >= 0 && selectedResultIndex < maxResults)
            {
                if (currentTab == ResultTab::Users && selectedResultIndex < userResults.size())
                {
                    if (onUserSelected)
                        onUserSelected(userResults[selectedResultIndex].id);
                }
                else if (currentTab == ResultTab::Posts && selectedResultIndex < postResults.size())
                {
                    if (onPostSelected)
                        onPostSelected(postResults[selectedResultIndex]);
                }
            }
            return true;
        }
    }

    return false;
}

//==============================================================================
void SearchComponent::timerCallback()
{
    stopTimer();
    
    if (!currentQuery.isEmpty())
    {
        performSearch();
    }
}

//==============================================================================
void SearchComponent::focusSearchInput()
{
    searchInput->grabKeyboardFocus();
    searchInput->selectAll();
}

void SearchComponent::clearSearch()
{
    searchInput->clear();
    currentQuery.clear();
    searchState = SearchState::Empty;
    userResults.clear();
    postResults.clear();
    selectedResultIndex = -1; // Reset keyboard navigation (7.3.8)
    repaint();
}

//==============================================================================
void SearchComponent::performSearch()
{
    if (currentQuery.isEmpty() || networkClient == nullptr)
    {
        Log::warn("SearchComponent: Cannot perform search - query empty or network client null");
        return;
    }

    Log::info("SearchComponent: Performing search - query: \"" + currentQuery + "\", tab: " + (currentTab == ResultTab::Users ? "Users" : "Posts"));

    isSearching = true;
    searchState = SearchState::Searching;
    selectedResultIndex = -1; // Reset keyboard navigation (7.3.8)
    repaint();

    // Add to recent searches
    addToRecentSearches(currentQuery);

    // Perform search based on current tab
    if (currentTab == ResultTab::Users)
    {
        networkClient->searchUsers(currentQuery, 20, 0, [this](bool success, const juce::var& response) {
            isSearching = false;

            if (success && response.isObject())
            {
                userResults.clear();
                auto usersArray = response.getProperty("users", juce::var());
                if (usersArray.isArray())
                {
                    for (int i = 0; i < usersArray.size(); ++i)
                    {
                        auto userJson = usersArray[i];
                        userResults.add(DiscoveredUser::fromJson(userJson));
                    }
                }
                totalUserResults = static_cast<int>(response.getProperty("meta", juce::var()).getProperty("total", 0));

                Log::info("SearchComponent: User search completed - results: " + juce::String(userResults.size()) + ", total: " + juce::String(totalUserResults));
                searchState = userResults.isEmpty() ? SearchState::NoResults : SearchState::Results;
                selectedResultIndex = -1; // Reset keyboard navigation when new results arrive (7.3.8)
            }
            else
            {
                Log::error("SearchComponent: User search failed");
                searchState = SearchState::Error;
            }

            repaint();
        });
    }
    else // Posts tab
    {
        networkClient->searchPosts(currentQuery, selectedGenre, bpmMin, bpmMax, selectedKey, 20, 0,
            [this](bool success, const juce::var& response) {
                isSearching = false;

                if (success && response.isObject())
                {
                    postResults.clear();
                    auto postsArray = response.getProperty("posts", juce::var());
                    if (postsArray.isArray())
                    {
                        for (int i = 0; i < postsArray.size(); ++i)
                        {
                            auto postJson = postsArray[i];
                            FeedPost post = FeedPost::fromJson(postJson);
                            if (post.isValid())
                            {
                                postResults.add(post);
                            }
                        }
                    }
                    totalPostResults = static_cast<int>(response.getProperty("meta", juce::var()).getProperty("total", 0));

                    searchState = postResults.isEmpty() ? SearchState::NoResults : SearchState::Results;
                    selectedResultIndex = -1; // Reset keyboard navigation when new results arrive (7.3.8)
                }
                else
                {
                    searchState = SearchState::Error;
                }

                repaint();
            });
    }
}

void SearchComponent::loadRecentSearches()
{
    // Load from file: ~/.local/share/Sidechain/recent_searches.txt
    juce::File searchDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                          .getChildFile("Sidechain");
    if (!searchDir.exists())
        searchDir.createDirectory();

    juce::File searchFile = searchDir.getChildFile("recent_searches.txt");
    if (searchFile.existsAsFile())
    {
        juce::StringArray lines;
        searchFile.readLines(lines);
        recentSearches.clear();
        for (int i = 0; i < lines.size() && i < MAX_RECENT_SEARCHES; ++i)
        {
            if (!lines[i].trim().isEmpty())
                recentSearches.add(lines[i].trim());
        }
    }
}

void SearchComponent::saveRecentSearches()
{
    juce::File searchDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                          .getChildFile("Sidechain");
    if (!searchDir.exists())
        searchDir.createDirectory();

    juce::File searchFile = searchDir.getChildFile("recent_searches.txt");
    juce::StringArray lines;
    for (const auto& search : recentSearches)
    {
        lines.add(search);
    }
    searchFile.replaceWithText(lines.joinIntoString("\n"));
}

void SearchComponent::addToRecentSearches(const juce::String& query)
{
    // Remove if already exists
    recentSearches.removeAllInstancesOf(query);

    // Add to front
    recentSearches.insert(0, query);

    // Limit size
    while (recentSearches.size() > MAX_RECENT_SEARCHES)
        recentSearches.removeLast();

    saveRecentSearches();
}

void SearchComponent::loadTrendingSearches()
{
    // TODO: Load from backend endpoint /api/v1/search/trending
    // For now, use placeholder
    trendingSearches = { "electronic", "hip-hop", "techno", "house", "trap" };
}

void SearchComponent::loadAvailableGenres()
{
    if (networkClient == nullptr)
        return;

    networkClient->getAvailableGenres([this](bool success, const juce::var& response) {
        if (success && response.isObject())
        {
            availableGenres.clear();
            auto genresArray = response.getProperty("genres", juce::var());
            if (genresArray.isArray())
            {
                for (int i = 0; i < genresArray.size(); ++i)
                {
                    availableGenres.add(genresArray[i].toString());
                }
            }
        }
    });
}

void SearchComponent::applyFilters()
{
    if (!currentQuery.isEmpty())
    {
        performSearch();
    }
}

void SearchComponent::switchTab(ResultTab tab)
{
    currentTab = tab;
    selectedResultIndex = -1; // Reset keyboard navigation when switching tabs (7.3.8)
    
    // If we have a query, search in the new tab
    if (!currentQuery.isEmpty())
    {
        performSearch();
    }
    
    repaint();
}

//==============================================================================
void SearchComponent::drawHeader(juce::Graphics& g)
{
    auto bounds = getHeaderBounds();
    
    // Back button
    backButtonBounds = bounds.removeFromLeft(50).reduced(10);
    g.setColour(SidechainColors::textPrimary());
    g.setFont(20.0f);
    g.drawText("←", backButtonBounds, juce::Justification::centred);

    // Search input bounds
    auto searchBounds = bounds.removeFromLeft(bounds.getWidth() - 60).reduced(10, 5);
    searchInput->setBounds(searchBounds);

    // Clear button (X) if there's text
    if (!currentQuery.isEmpty())
    {
        clearButtonBounds = bounds.removeFromLeft(40).reduced(10);
        g.setColour(SidechainColors::textMuted());
        g.setFont(18.0f);
        g.drawText("×", clearButtonBounds, juce::Justification::centred);
    }
}

void SearchComponent::drawFilters(juce::Graphics& g)
{
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

void SearchComponent::drawTabs(juce::Graphics& g)
{
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

void SearchComponent::drawResults(juce::Graphics& g)
{
    auto bounds = getResultsBounds();
    int yPos = bounds.getY() - static_cast<int>(scrollPosition);

    if (currentTab == ResultTab::Users)
    {
        // Draw user cards
        for (int i = 0; i < userResults.size(); ++i)
        {
            juce::Rectangle<int> cardBounds(10, yPos + i * CARD_HEIGHT, bounds.getWidth() - 20, CARD_HEIGHT - 5);
            if (cardBounds.getBottom() < bounds.getY() || cardBounds.getY() > bounds.getBottom())
                continue; // Off screen

            // Create user card if needed
            while (userCards.size() <= i)
            {
                auto* card = new UserCardComponent();
                // Setup callbacks for user interactions
                card->onUserClicked = [this](const DiscoveredUser& user) {
                    if (onUserSelected)
                        onUserSelected(user.id);
                };
                card->onFollowToggled = [this](const DiscoveredUser& user, bool willFollow) {
                    if (networkClient != nullptr)
                    {
                        if (willFollow)
                        {
                            networkClient->followUser(user.id);
                        }
                        else
                        {
                            networkClient->unfollowUser(user.id);
                        }
                        // Update UI optimistically
                        for (auto* card : userCards)
                        {
                            if (card && card->getUserId() == user.id)
                            {
                                card->setIsFollowing(willFollow);
                                break;
                            }
                        }
                    }
                };
                addAndMakeVisible(card);
                userCards.add(card);
            }

            if (userCards[i] != nullptr)
            {
                userCards[i]->setUser(userResults[i]);
                userCards[i]->setBounds(cardBounds);
                
                // Highlight if keyboard-selected (7.3.8)
                if (i == selectedResultIndex)
                {
                    g.setColour(SidechainColors::accent().withAlpha(0.3f));
                    g.fillRoundedRectangle(cardBounds.toFloat(), 4.0f);
                }
            }
        }
    }
    else // Posts tab
    {
        // Draw post cards
        for (int i = 0; i < postResults.size(); ++i)
        {
            juce::Rectangle<int> cardBounds(10, yPos + i * CARD_HEIGHT, bounds.getWidth() - 20, CARD_HEIGHT - 5);
            if (cardBounds.getBottom() < bounds.getY() || cardBounds.getY() > bounds.getBottom())
                continue; // Off screen

            // Create post card if needed
            while (postCards.size() <= i)
            {
                auto* card = new PostCardComponent();
                addAndMakeVisible(card);
                postCards.add(card);
            }

            if (postCards[i] != nullptr)
            {
                postCards[i]->setPost(postResults[i]);
                postCards[i]->setBounds(cardBounds);
                
                // Highlight if keyboard-selected (7.3.8)
                if (i == selectedResultIndex)
                {
                    g.setColour(SidechainColors::accent().withAlpha(0.3f));
                    g.fillRoundedRectangle(cardBounds.toFloat(), 4.0f);
                }
            }
        }
    }

    // Update scrollbar
    totalContentHeight = (currentTab == ResultTab::Users ? userResults.size() : postResults.size()) * CARD_HEIGHT;
    int visibleHeight = bounds.getHeight();
    if (totalContentHeight > visibleHeight)
    {
        scrollBar->setRangeLimits(0.0, static_cast<double>(totalContentHeight - visibleHeight));
        scrollBar->setCurrentRangeStart(scrollPosition);
        scrollBar->setVisible(true);
    }
    else
    {
        scrollBar->setVisible(false);
    }
}

void SearchComponent::drawEmptyState(juce::Graphics& g)
{
    auto bounds = getResultsBounds();
    
    g.setColour(SidechainColors::textMuted());
    g.setFont(18.0f);
    g.drawText("Start typing to search...", bounds.removeFromTop(30), juce::Justification::centred);

    // Draw recent searches
    drawRecentSearches(g);

    // Draw trending searches
    drawTrendingSearches(g);
}

void SearchComponent::drawNoResultsState(juce::Graphics& g)
{
    auto bounds = getResultsBounds();
    
    g.setColour(SidechainColors::textMuted());
    g.setFont(18.0f);
    g.drawText("No results found", bounds.removeFromTop(30), juce::Justification::centred);

    g.setFont(14.0f);
    g.drawText("Try a different search term or adjust your filters", 
               bounds.removeFromTop(25), juce::Justification::centred);

    // Show suggestions
    g.setFont(12.0f);
    g.drawText("Suggestions:", bounds.removeFromTop(20).translated(20, 0), juce::Justification::centredLeft);
    bounds.removeFromTop(10);
    
    juce::Array<juce::String> suggestions = { "Try a different keyword", "Remove filters", "Check spelling" };
    for (int i = 0; i < suggestions.size(); ++i)
    {
        g.drawText("• " + suggestions[i], bounds.removeFromTop(20), juce::Justification::centredLeft);
    }
}

void SearchComponent::drawErrorState(juce::Graphics& g)
{
    auto bounds = getResultsBounds();
    
    g.setColour(SidechainColors::error());
    g.setFont(16.0f);
    g.drawText("Error searching. Please try again.", bounds, juce::Justification::centred);
}

void SearchComponent::drawRecentSearches(juce::Graphics& g)
{
    if (recentSearches.isEmpty())
        return;

    auto bounds = getResultsBounds();
    int yPos = HEADER_HEIGHT + FILTER_HEIGHT + 40;

    g.setColour(SidechainColors::textPrimary());
    g.setFont(14.0f);
    g.drawText("Recent Searches", juce::Rectangle<int>(20, yPos, getWidth() - 40, 25), juce::Justification::centredLeft);
    yPos += 30;

    g.setColour(SidechainColors::textMuted());
    g.setFont(12.0f);
    for (int i = 0; i < recentSearches.size() && i < 5; ++i)
    {
        juce::Rectangle<int> itemBounds(20, yPos + i * 40, getWidth() - 40, 35);
        g.setColour(SidechainColors::surface());
        g.fillRoundedRectangle(itemBounds.toFloat(), 6.0f);
        g.setColour(SidechainColors::textPrimary());
        g.drawText(recentSearches[i], itemBounds.reduced(10, 5), juce::Justification::centredLeft);
    }
}

void SearchComponent::drawTrendingSearches(juce::Graphics& g)
{
    if (trendingSearches.isEmpty())
        return;

    auto bounds = getResultsBounds();
    int yPos = HEADER_HEIGHT + FILTER_HEIGHT + 40 + (recentSearches.size() > 0 ? (recentSearches.size() * 40 + 40) : 0);

    g.setColour(SidechainColors::textPrimary());
    g.setFont(14.0f);
    g.drawText("Trending Searches", juce::Rectangle<int>(20, yPos, getWidth() - 40, 25), juce::Justification::centredLeft);
    yPos += 30;

    g.setColour(SidechainColors::textMuted());
    g.setFont(12.0f);
    for (int i = 0; i < trendingSearches.size() && i < 5; ++i)
    {
        juce::Rectangle<int> itemBounds(20, yPos + i * 40, getWidth() - 40, 35);
        g.setColour(SidechainColors::surface());
        g.fillRoundedRectangle(itemBounds.toFloat(), 6.0f);
        g.setColour(SidechainColors::textPrimary());
        g.drawText(trendingSearches[i], itemBounds.reduced(10, 5), juce::Justification::centredLeft);
    }
}

//==============================================================================
void SearchComponent::layoutComponents()
{
    auto bounds = getLocalBounds();

    // Scrollbar
    scrollBar->setBounds(bounds.removeFromRight(12));

    // Header, filters, tabs, and results are drawn in paint()
}

juce::Rectangle<int> SearchComponent::getHeaderBounds() const
{
    return juce::Rectangle<int>(0, 0, getWidth(), HEADER_HEIGHT);
}

juce::Rectangle<int> SearchComponent::getFilterBounds() const
{
    return juce::Rectangle<int>(0, HEADER_HEIGHT, getWidth(), FILTER_HEIGHT);
}

juce::Rectangle<int> SearchComponent::getTabBounds() const
{
    return juce::Rectangle<int>(0, HEADER_HEIGHT + FILTER_HEIGHT, getWidth(), 40);
}

juce::Rectangle<int> SearchComponent::getResultsBounds() const
{
    int top = HEADER_HEIGHT + FILTER_HEIGHT + 40;
    return juce::Rectangle<int>(0, top, getWidth() - (scrollBar->isVisible() ? 12 : 0), getHeight() - top);
}

//==============================================================================
// Filter picker implementations (7.3.4)
//==============================================================================

const std::array<juce::String, 12>& SearchComponent::getAvailableGenres()
{
    static const std::array<juce::String, 12> genres = {{
        "Electronic",
        "Hip-Hop / Trap",
        "House",
        "Techno",
        "Drum & Bass",
        "Dubstep",
        "Pop",
        "R&B / Soul",
        "Rock",
        "Lo-Fi",
        "Ambient",
        "Other"
    }};
    return genres;
}

const std::array<juce::String, 24>& SearchComponent::getMusicalKeys()
{
    static const std::array<juce::String, 24> keys = {{
        "C Major",
        "C# / Db Major",
        "D Major",
        "D# / Eb Major",
        "E Major",
        "F Major",
        "F# / Gb Major",
        "G Major",
        "G# / Ab Major",
        "A Major",
        "A# / Bb Major",
        "B Major",
        "C Minor",
        "C# / Db Minor",
        "D Minor",
        "D# / Eb Minor",
        "E Minor",
        "F Minor",
        "F# / Gb Minor",
        "G Minor",
        "G# / Ab Minor",
        "A Minor",
        "A# / Bb Minor",
        "B Minor"
    }};
    return keys;
}

void SearchComponent::showGenrePicker()
{
    juce::PopupMenu menu;
    auto& genres = getAvailableGenres();
    
    // Add "All Genres" option
    menu.addItem(1, "All Genres", true, selectedGenre.isEmpty());
    
    // Add genre options
    for (int i = 0; i < (int)genres.size(); ++i)
    {
        menu.addItem(i + 2, genres[i], true, selectedGenre == genres[i]);
    }
    
    menu.showMenuAsync(juce::PopupMenu::Options()
        .withTargetComponent(this)
        .withTargetScreenArea(genreFilterBounds.translated(getScreenX(), getScreenY())),
        [this](int result) {
            if (result == 1)
            {
                selectedGenre = juce::String();
            }
            else if (result > 1)
            {
                auto& genres = getAvailableGenres();
                int index = result - 2;
                if (index >= 0 && index < (int)genres.size())
                {
                    selectedGenre = genres[index];
                }
            }
            applyFilters();
            repaint();
        });
}

void SearchComponent::showBPMPicker()
{
    juce::PopupMenu menu;
    
    // Common BPM ranges
    struct BPMPreset {
        juce::String name;
        int min;
        int max;
    };
    
    static const std::array<BPMPreset, 8> presets = {{
        { "All BPM", 0, 200 },
        { "60-80 (Downtempo)", 60, 80 },
        { "80-100 (Hip-Hop)", 80, 100 },
        { "100-120 (House)", 100, 120 },
        { "120-130 (House/Techno)", 120, 130 },
        { "130-150 (Techno/Trance)", 130, 150 },
        { "150-180 (Drum & Bass)", 150, 180 },
        { "Custom...", -1, -1 }
    }};
    
    for (int i = 0; i < (int)presets.size(); ++i)
    {
        bool isSelected = (bpmMin == presets[i].min && bpmMax == presets[i].max) ||
                          (i == 0 && bpmMin == 0 && bpmMax == 200);
        menu.addItem(i + 1, presets[i].name, true, isSelected);
    }
    
    menu.showMenuAsync(juce::PopupMenu::Options()
        .withTargetComponent(this)
        .withTargetScreenArea(bpmFilterBounds.translated(getScreenX(), getScreenY())),
        [this](int result) {
            struct BPMPreset { int min; int max; };
            static const std::array<BPMPreset, 8> presets = {{
                { 0, 200 },      // All
                { 60, 80 },      // Downtempo
                { 80, 100 },     // Hip-Hop
                { 100, 120 },    // House
                { 120, 130 },    // House/Techno
                { 130, 150 },    // Techno/Trance
                { 150, 180 },    // Drum & Bass
                { -1, -1 }       // Custom (TODO: implement custom dialog)
            }};
            
            if (result > 0 && result <= (int)presets.size())
            {
                int index = result - 1;
                if (presets[index].min >= 0) // Not custom
                {
                    bpmMin = presets[index].min;
                    bpmMax = presets[index].max;
                    applyFilters();
                    repaint();
                }
                // TODO: For custom, show a dialog to input min/max
            }
        });
}

void SearchComponent::showKeyPicker()
{
    juce::PopupMenu menu;
    auto& keys = getMusicalKeys();
    
    // Add "All Keys" option
    menu.addItem(1, "All Keys", true, selectedKey.isEmpty());
    
    // Add key options
    for (int i = 0; i < (int)keys.size(); ++i)
    {
        menu.addItem(i + 2, keys[i], true, selectedKey == keys[i]);
    }
    
    menu.showMenuAsync(juce::PopupMenu::Options()
        .withTargetComponent(this)
        .withTargetScreenArea(keyFilterBounds.translated(getScreenX(), getScreenY())),
        [this](int result) {
            if (result == 1)
            {
                selectedKey = juce::String();
            }
            else if (result > 1)
            {
                auto& keys = getMusicalKeys();
                int index = result - 2;
                if (index >= 0 && index < (int)keys.size())
                {
                    selectedKey = keys[index];
                }
            }
            applyFilters();
            repaint();
        });
}
