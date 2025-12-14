#include "UserPickerDialog.h"
#include "../../network/NetworkClient.h"
#include "../../util/Log.h"
#include "../../util/Colors.h"

UserPickerDialog::UserPickerDialog()
{
    Log::info("UserPickerDialog: Initializing");
    
    // Setup search input
    searchInput.setMultiLine(false);
    searchInput.setReturnKeyStartsNewLine(false);
    searchInput.setPopupMenuEnabled(true);
    searchInput.setTextToShowWhenEmpty("Search for people...", SidechainColors::textMuted());
    searchInput.setColour(juce::TextEditor::backgroundColourId, SidechainColors::inputBg());
    searchInput.setColour(juce::TextEditor::outlineColourId, SidechainColors::inputBorder());
    searchInput.setColour(juce::TextEditor::focusedOutlineColourId, SidechainColors::accent());
    searchInput.setColour(juce::TextEditor::textColourId, SidechainColors::textPrimary());
    searchInput.setFont(juce::FontOptions(14.0f));
    searchInput.addListener(this);
    addAndMakeVisible(searchInput);
    
    // Setup group name input (hidden by default)
    groupNameInput.setMultiLine(false);
    groupNameInput.setReturnKeyStartsNewLine(false);
    groupNameInput.setPopupMenuEnabled(true);
    groupNameInput.setTextToShowWhenEmpty("Group name (optional)", SidechainColors::textMuted());
    groupNameInput.setColour(juce::TextEditor::backgroundColourId, SidechainColors::inputBg());
    groupNameInput.setColour(juce::TextEditor::outlineColourId, SidechainColors::inputBorder());
    groupNameInput.setColour(juce::TextEditor::focusedOutlineColourId, SidechainColors::accent());
    groupNameInput.setColour(juce::TextEditor::textColourId, SidechainColors::textPrimary());
    groupNameInput.setFont(juce::FontOptions(14.0f));
    groupNameInput.setVisible(false);
    addAndMakeVisible(groupNameInput);
    
    // Setup scroll bar
    scrollBar.setAutoHide(false);
    scrollBar.addListener(this);
    addAndMakeVisible(scrollBar);
    
    // Setup error state component
    errorStateComponent = std::make_unique<ErrorState>();
    addChildComponent(errorStateComponent.get());
    
    setSize(500, 700);
}

UserPickerDialog::~UserPickerDialog()
{
    Log::debug("UserPickerDialog: Destroying");
}

void UserPickerDialog::paint(juce::Graphics& g)
{
    // Background
    g.fillAll(SidechainColors::background());
    
    if (dialogState == DialogState::Error)
    {
        drawErrorState(g);
        return;
    }
    
    if (dialogState == DialogState::Loading)
    {
        drawLoadingState(g);
        return;
    }
    
    // Draw UI elements
    drawHeader(g);
    drawSearchInput(g);
    
    if (showGroupNameInput)
        drawGroupNameInput(g);
    
    // Calculate content area
    int contentY = HEADER_HEIGHT + SEARCH_INPUT_HEIGHT;
    if (showGroupNameInput)
        contentY += GROUP_NAME_INPUT_HEIGHT;
    
    int contentHeight = getHeight() - contentY - BOTTOM_PADDING;
    auto contentBounds = juce::Rectangle<int>(0, contentY, getWidth() - scrollBar.getWidth(), contentHeight);
    
    // Draw sections
    int y = contentY - static_cast<int>(scrollPosition);
    
    // Recent conversations section
    if (!recentUsers.empty() && currentSearchQuery.isEmpty())
    {
        drawSectionHeader(g, "Recent", y);
        y += SECTION_HEADER_HEIGHT;
        
        for (size_t i = 0; i < recentUsers.size(); ++i)
        {
            bool isSelected = selectedUserIds.find(recentUsers[i].userId) != selectedUserIds.end();
            drawUserItem(g, recentUsers[i], y, isSelected);
            y += USER_ITEM_HEIGHT;
        }
        
        y += 10; // Section spacing
    }
    
    // Suggested users section
    if (!suggestedUsers.empty() && currentSearchQuery.isEmpty())
    {
        drawSectionHeader(g, "Suggested", y);
        y += SECTION_HEADER_HEIGHT;
        
        for (size_t i = 0; i < suggestedUsers.size(); ++i)
        {
            bool isSelected = selectedUserIds.find(suggestedUsers[i].userId) != selectedUserIds.end();
            drawUserItem(g, suggestedUsers[i], y, isSelected);
            y += USER_ITEM_HEIGHT;
        }
    }
    
    // Search results section
    if (!currentSearchQuery.isEmpty())
    {
        if (isSearching)
        {
            g.setColour(SidechainColors::textSecondary());
            g.setFont(juce::FontOptions(14.0f));
            g.drawText("Searching...", 0, y, getWidth(), 40, juce::Justification::centred);
        }
        else if (searchResults.empty())
        {
            g.setColour(SidechainColors::textMuted());
            g.setFont(juce::FontOptions(14.0f));
            g.drawText("No users found", 0, y, getWidth(), 40, juce::Justification::centred);
        }
        else
        {
            drawSectionHeader(g, "Results", y);
            y += SECTION_HEADER_HEIGHT;
            
            for (size_t i = 0; i < searchResults.size(); ++i)
            {
                bool isSelected = selectedUserIds.find(searchResults[i].userId) != selectedUserIds.end();
                drawUserItem(g, searchResults[i], y, isSelected);
                y += USER_ITEM_HEIGHT;
            }
        }
    }
    
    // Draw action buttons at bottom
    drawActionButtons(g);
}

void UserPickerDialog::resized()
{
    auto bounds = getLocalBounds();
    
    // Position scroll bar on right
    scrollBar.setBounds(bounds.removeFromRight(16));
    
    // Position search input
    int searchY = HEADER_HEIGHT;
    searchInput.setBounds(15, searchY + 10, bounds.getWidth() - 30, SEARCH_INPUT_HEIGHT - 20);
    
    // Position group name input (if visible)
    if (showGroupNameInput)
    {
        int groupY = searchY + SEARCH_INPUT_HEIGHT;
        groupNameInput.setBounds(15, groupY + 10, bounds.getWidth() - 30, GROUP_NAME_INPUT_HEIGHT - 20);
    }
    
    // Position error state
    if (errorStateComponent)
        errorStateComponent->setBounds(bounds);
    
    // Update scroll bounds
    int contentHeight = calculateContentHeight();
    int visibleHeight = getHeight() - HEADER_HEIGHT - SEARCH_INPUT_HEIGHT - BOTTOM_PADDING;
    if (showGroupNameInput)
        visibleHeight -= GROUP_NAME_INPUT_HEIGHT;
    
    scrollBar.setRangeLimits(0.0, juce::jmax(0.0, static_cast<double>(contentHeight - visibleHeight)));
    scrollBar.setCurrentRange(scrollPosition, visibleHeight, juce::dontSendNotification);
}

void UserPickerDialog::mouseUp(const juce::MouseEvent& event)
{
    auto pos = event.getPosition();
    
    // Close button
    if (getCloseButtonBounds().contains(pos))
    {
        cancel();
        return;
    }
    
    // Cancel button
    if (getCancelButtonBounds().contains(pos))
    {
        cancel();
        return;
    }
    
    // Create/Send button
    if (getCreateButtonBounds().contains(pos))
    {
        createConversation();
        return;
    }
    
    // Check for clicks on user items
    int contentY = HEADER_HEIGHT + SEARCH_INPUT_HEIGHT;
    if (showGroupNameInput)
        contentY += GROUP_NAME_INPUT_HEIGHT;
    
    int y = contentY - static_cast<int>(scrollPosition);
    
    // Recent users
    if (!recentUsers.empty() && currentSearchQuery.isEmpty())
    {
        y += SECTION_HEADER_HEIGHT;
        for (size_t i = 0; i < recentUsers.size(); ++i)
        {
            auto itemBounds = getUserItemBounds(i, y);
            if (itemBounds.contains(pos))
            {
                toggleUserSelection(recentUsers[i].userId);
                return;
            }
            y += USER_ITEM_HEIGHT;
        }
        y += 10;
    }
    
    // Suggested users
    if (!suggestedUsers.empty() && currentSearchQuery.isEmpty())
    {
        y += SECTION_HEADER_HEIGHT;
        for (size_t i = 0; i < suggestedUsers.size(); ++i)
        {
            auto itemBounds = getUserItemBounds(i, y);
            if (itemBounds.contains(pos))
            {
                toggleUserSelection(suggestedUsers[i].userId);
                return;
            }
            y += USER_ITEM_HEIGHT;
        }
    }
    
    // Search results
    if (!currentSearchQuery.isEmpty() && !searchResults.empty())
    {
        y += SECTION_HEADER_HEIGHT;
        for (size_t i = 0; i < searchResults.size(); ++i)
        {
            auto itemBounds = getUserItemBounds(i, y);
            if (itemBounds.contains(pos))
            {
                toggleUserSelection(searchResults[i].userId);
                return;
            }
            y += USER_ITEM_HEIGHT;
        }
    }
}

void UserPickerDialog::mouseWheelMove([[maybe_unused]] const juce::MouseEvent& event, 
                                      const juce::MouseWheelDetails& wheel)
{
    scrollPosition -= wheel.deltaY * 30.0;
    scrollPosition = juce::jlimit(0.0, scrollBar.getMaximumRangeLimit(), scrollPosition);
    scrollBar.setCurrentRangeStart(scrollPosition, juce::dontSendNotification);
    repaint();
}

void UserPickerDialog::textEditorTextChanged(juce::TextEditor& editor)
{
    if (&editor == &searchInput)
    {
        // Start debounce timer for search
        stopTimer();
        startTimer(SEARCH_DEBOUNCE_MS);
    }
}

void UserPickerDialog::textEditorReturnKeyPressed(juce::TextEditor& editor)
{
    if (&editor == &searchInput)
    {
        // Trigger search immediately
        stopTimer();
        performSearch(searchInput.getText());
    }
    else if (&editor == &groupNameInput)
    {
        // Enter key in group name = create conversation
        createConversation();
    }
}

void UserPickerDialog::timerCallback()
{
    // Debounced search triggered
    stopTimer();
    performSearch(searchInput.getText());
}

void UserPickerDialog::scrollBarMoved([[maybe_unused]] juce::ScrollBar* scrollBarMoved, 
                                      double newRangeStart)
{
    scrollPosition = newRangeStart;
    repaint();
}

void UserPickerDialog::setStreamChatClient(StreamChatClient* client)
{
    streamChatClient = client;
}

void UserPickerDialog::setNetworkClient(NetworkClient* client)
{
    networkClient = client;
}

void UserPickerDialog::loadRecentConversations()
{
    if (!streamChatClient)
    {
        Log::error("UserPickerDialog: Cannot load recent conversations - no StreamChatClient");
        return;
    }
    
    // TODO: Implement loading recent conversation users from StreamChatClient
    // For now, leave empty - will be implemented when StreamChatClient has the method
    Log::info("UserPickerDialog: Loading recent conversations");
    dialogState = DialogState::Showing;
    repaint();
}

void UserPickerDialog::loadSuggestedUsers()
{
    if (!networkClient)
    {
        Log::error("UserPickerDialog: Cannot load suggested users - no NetworkClient");
        return;
    }
    
    // TODO: Implement loading suggested users from NetworkClient
    // For now, leave empty - will be implemented when NetworkClient has the method
    Log::info("UserPickerDialog: Loading suggested users");
}

//==============================================================================
// Drawing helpers

void UserPickerDialog::drawHeader(juce::Graphics& g)
{
    auto headerBounds = juce::Rectangle<int>(0, 0, getWidth(), HEADER_HEIGHT);
    
    // Header background
    g.setColour(SidechainColors::surface());
    g.fillRect(headerBounds);
    
    // Title
    g.setColour(SidechainColors::textPrimary());
    g.setFont(juce::FontOptions(18.0f).withStyle("Bold"));
    
    juce::String title = "New Message";
    if (selectedUserIds.size() > 1)
        title = "New Group (" + juce::String(selectedUserIds.size()) + ")";
    
    g.drawText(title, 15, 0, getWidth() - 30, HEADER_HEIGHT, juce::Justification::centredLeft);
    
    // Close button (X)
    auto closeBounds = getCloseButtonBounds();
    g.setColour(SidechainColors::textSecondary());
    g.setFont(juce::FontOptions(20.0f));
    g.drawText("×", closeBounds, juce::Justification::centred);
    
    // Bottom border
    g.setColour(SidechainColors::border());
    g.drawLine(0.0f, static_cast<float>(HEADER_HEIGHT), 
               static_cast<float>(getWidth()), static_cast<float>(HEADER_HEIGHT), 1.0f);
}

void UserPickerDialog::drawSearchInput([[maybe_unused]] juce::Graphics& g)
{
    // Search input is drawn by the TextEditor component
}

void UserPickerDialog::drawGroupNameInput([[maybe_unused]] juce::Graphics& g)
{
    // Group name input is drawn by the TextEditor component
}

void UserPickerDialog::drawSectionHeader(juce::Graphics& g, const juce::String& title, int y)
{
    g.setColour(SidechainColors::textSecondary());
    g.setFont(juce::FontOptions(12.0f).withStyle("Bold"));
    g.drawText(title.toUpperCase(), 15, y + 10, getWidth() - 30, 20, juce::Justification::centredLeft);
}

void UserPickerDialog::drawUserItem(juce::Graphics& g, const UserItem& user, int y, bool isSelected)
{
    auto itemBounds = getUserItemBounds(0, y);
    
    // Selection background
    if (isSelected)
    {
        g.setColour(SidechainColors::accent().withAlpha(0.1f));
        g.fillRoundedRectangle(itemBounds.reduced(5, 2).toFloat(), 8.0f);
    }
    
    int x = 15;
    
    // Avatar (placeholder circle)
    int avatarSize = 50;
    int avatarY = y + (USER_ITEM_HEIGHT - avatarSize) / 2;
    
    g.setColour(SidechainColors::surface());
    g.fillEllipse(static_cast<float>(x), static_cast<float>(avatarY), 
                  static_cast<float>(avatarSize), static_cast<float>(avatarSize));
    
    // Avatar initials
    g.setColour(SidechainColors::textSecondary());
    g.setFont(juce::FontOptions(18.0f).withStyle("Bold"));
    juce::String initials;
    if (user.displayName.isNotEmpty())
        initials = user.displayName.substring(0, 1).toUpperCase();
    else if (user.username.isNotEmpty())
        initials = user.username.substring(0, 1).toUpperCase();
    g.drawText(initials, x, avatarY, avatarSize, avatarSize, juce::Justification::centred);
    
    // Online indicator
    if (user.isOnline)
    {
        int dotSize = 14;
        int dotX = x + avatarSize - dotSize + 2;
        int dotY = avatarY + avatarSize - dotSize + 2;
        
        // White border
        g.setColour(SidechainColors::background());
        g.fillEllipse(static_cast<float>(dotX - 1), static_cast<float>(dotY - 1), 
                      static_cast<float>(dotSize + 2), static_cast<float>(dotSize + 2));
        
        // Green dot
        g.setColour(SidechainColors::onlineIndicator());
        g.fillEllipse(static_cast<float>(dotX), static_cast<float>(dotY), 
                      static_cast<float>(dotSize), static_cast<float>(dotSize));
    }
    
    x += avatarSize + 12;
    
    // Display name
    g.setColour(SidechainColors::textPrimary());
    g.setFont(juce::FontOptions(15.0f).withStyle("Bold"));
    juce::String displayName = user.displayName.isNotEmpty() ? user.displayName : user.username;
    g.drawText(displayName, x, y + 12, getWidth() - x - 100, 20, juce::Justification::centredLeft);
    
    // Username
    g.setColour(SidechainColors::textSecondary());
    g.setFont(juce::FontOptions(13.0f));
    g.drawText("@" + user.username, x, y + 32, getWidth() - x - 100, 18, juce::Justification::centredLeft);
    
    // Follow status badge
    int badgeX = getWidth() - 90;
    if (user.isFollowing && user.followsMe)
    {
        g.setColour(SidechainColors::accent().withAlpha(0.2f));
        g.fillRoundedRectangle(static_cast<float>(badgeX), static_cast<float>(y + 20), 
                               75.0f, 24.0f, 12.0f);
        g.setColour(SidechainColors::accent());
        g.setFont(juce::FontOptions(11.0f).withStyle("Bold"));
        g.drawText("MUTUAL", badgeX, y + 20, 75, 24, juce::Justification::centred);
    }
    else if (user.followsMe)
    {
        g.setColour(SidechainColors::textMuted());
        g.setFont(juce::FontOptions(11.0f));
        g.drawText("Follows you", badgeX, y + 25, 75, 20, juce::Justification::centredRight);
    }
    
    // Selection checkmark
    if (isSelected)
    {
        int checkX = getWidth() - scrollBar.getWidth() - 40;
        g.setColour(SidechainColors::accent());
        g.setFont(juce::FontOptions(24.0f));
        g.drawText("✓", checkX, y, 30, USER_ITEM_HEIGHT, juce::Justification::centred);
    }
}

void UserPickerDialog::drawActionButtons(juce::Graphics& g)
{
    auto createBounds = getCreateButtonBounds();
    auto cancelBounds = getCancelButtonBounds();
    
    // Create/Send button
    bool canCreate = !selectedUserIds.empty();
    g.setColour(canCreate ? SidechainColors::accent() : SidechainColors::accent().withAlpha(0.5f));
    g.fillRoundedRectangle(createBounds.toFloat(), 8.0f);
    
    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(15.0f).withStyle("Bold"));
    juce::String buttonText = selectedUserIds.size() > 1 ? "Create Group" : "Send Message";
    g.drawText(buttonText, createBounds, juce::Justification::centred);
    
    // Cancel button
    g.setColour(SidechainColors::textSecondary());
    g.drawRoundedRectangle(cancelBounds.toFloat(), 8.0f, 1.5f);
    g.setFont(juce::FontOptions(15.0f));
    g.drawText("Cancel", cancelBounds, juce::Justification::centred);
}

void UserPickerDialog::drawLoadingState(juce::Graphics& g)
{
    g.setColour(SidechainColors::textSecondary());
    g.setFont(juce::FontOptions(16.0f));
    g.drawText("Loading...", 0, 0, getWidth(), getHeight(), juce::Justification::centred);
}

void UserPickerDialog::drawErrorState(juce::Graphics& g)
{
    if (errorStateComponent)
    {
        errorStateComponent->setVisible(true);
        errorStateComponent->setErrorMessage(errorMessage);
    }
}

void UserPickerDialog::drawEmptyState([[maybe_unused]] juce::Graphics& g)
{
    // Not currently used - we always show search input
}

//==============================================================================
// Helper methods

int UserPickerDialog::calculateContentHeight()
{
    int height = 0;
    
    // Recent users
    if (!recentUsers.empty() && currentSearchQuery.isEmpty())
    {
        height += SECTION_HEADER_HEIGHT;
        height += static_cast<int>(recentUsers.size()) * USER_ITEM_HEIGHT;
        height += 10; // Section spacing
    }
    
    // Suggested users
    if (!suggestedUsers.empty() && currentSearchQuery.isEmpty())
    {
        height += SECTION_HEADER_HEIGHT;
        height += static_cast<int>(suggestedUsers.size()) * USER_ITEM_HEIGHT;
    }
    
    // Search results
    if (!currentSearchQuery.isEmpty() && !searchResults.empty())
    {
        height += SECTION_HEADER_HEIGHT;
        height += static_cast<int>(searchResults.size()) * USER_ITEM_HEIGHT;
    }
    
    return height;
}

void UserPickerDialog::performSearch(const juce::String& query)
{
    currentSearchQuery = query.trim();
    
    if (currentSearchQuery.isEmpty())
    {
        searchResults.clear();
        isSearching = false;
        repaint();
        return;
    }
    
    if (!networkClient)
    {
        Log::error("UserPickerDialog: Cannot search - no NetworkClient");
        return;
    }
    
    isSearching = true;
    repaint();
    
    // TODO: Implement actual search via NetworkClient
    // For now, just clear results after a delay to show the searching state
    juce::MessageManager::callAsync([this]() {
        isSearching = false;
        searchResults.clear();
        repaint();
    });
    
    Log::info("UserPickerDialog: Searching for: " + currentSearchQuery);
}

void UserPickerDialog::toggleUserSelection(const juce::String& userId)
{
    if (selectedUserIds.find(userId) != selectedUserIds.end())
    {
        selectedUserIds.erase(userId);
    }
    else
    {
        selectedUserIds.insert(userId);
    }
    
    updateGroupNameInputVisibility();
    repaint();
}

bool UserPickerDialog::isUserSelected(const juce::String& userId) const
{
    return selectedUserIds.find(userId) != selectedUserIds.end();
}

void UserPickerDialog::updateGroupNameInputVisibility()
{
    bool shouldShow = selectedUserIds.size() >= 2;
    if (shouldShow != showGroupNameInput)
    {
        showGroupNameInput = shouldShow;
        groupNameInput.setVisible(shouldShow);
        resized();
    }
}

void UserPickerDialog::createConversation()
{
    if (selectedUserIds.empty())
    {
        Log::warning("UserPickerDialog: No users selected");
        return;
    }
    
    if (selectedUserIds.size() == 1)
    {
        // Single user - create DM
        juce::String userId = *selectedUserIds.begin();
        Log::info("UserPickerDialog: Creating DM with user: " + userId);
        
        if (onUserSelected)
            onUserSelected(userId);
    }
    else
    {
        // Multiple users - create group
        std::vector<juce::String> userIds(selectedUserIds.begin(), selectedUserIds.end());
        juce::String groupName = groupNameInput.getText().trim();
        
        Log::info("UserPickerDialog: Creating group with " + juce::String(userIds.size()) + " users");
        
        if (onGroupCreated)
            onGroupCreated(userIds, groupName);
    }
}

void UserPickerDialog::cancel()
{
    Log::info("UserPickerDialog: Cancelled");
    
    if (onCancelled)
        onCancelled();
}

//==============================================================================
// Hit test bounds

juce::Rectangle<int> UserPickerDialog::getUserItemBounds(int index, int yOffset) const
{
    return juce::Rectangle<int>(0, yOffset + index * USER_ITEM_HEIGHT, 
                                getWidth() - scrollBar.getWidth(), USER_ITEM_HEIGHT);
}

juce::Rectangle<int> UserPickerDialog::getCreateButtonBounds() const
{
    int y = getHeight() - BOTTOM_PADDING + 15;
    int width = (getWidth() - scrollBar.getWidth() - 45) / 2;
    return juce::Rectangle<int>(getWidth() - scrollBar.getWidth() - 15 - width, y, width, BUTTON_HEIGHT);
}

juce::Rectangle<int> UserPickerDialog::getCancelButtonBounds() const
{
    int y = getHeight() - BOTTOM_PADDING + 15;
    int width = (getWidth() - scrollBar.getWidth() - 45) / 2;
    return juce::Rectangle<int>(15, y, width, BUTTON_HEIGHT);
}

juce::Rectangle<int> UserPickerDialog::getCloseButtonBounds() const
{
    return juce::Rectangle<int>(getWidth() - 50, 0, 50, HEADER_HEIGHT);
}

juce::Rectangle<int> UserPickerDialog::getDoneButtonBounds() const
{
    return getCreateButtonBounds();
}

//==============================================================================
// API helpers

void UserPickerDialog::handleSearchResults(const std::vector<UserItem>& results)
{
    searchResults = results;
    isSearching = false;
    repaint();
}

void UserPickerDialog::handleError(const juce::String& error)
{
    errorMessage = error;
    dialogState = DialogState::Error;
    repaint();
}
