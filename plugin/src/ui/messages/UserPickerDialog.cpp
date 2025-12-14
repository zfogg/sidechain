#include "UserPickerDialog.h"
#include "../../network/NetworkClient.h"
#include "../../util/Colors.h"
#include "../../util/Log.h"
#include "../../util/Result.h"
#include "../social/UserCard.h"

//==============================================================================
UserPickerDialog::UserPickerDialog()
    : scrollBar(true)  // Initialize scrollbar as vertical
{
    // Setup search input
    searchInput.setMultiLine(false);
    searchInput.setReturnKeyStartsNewLine(false);
    searchInput.setScrollbarsShown(false);
    searchInput.setCaretVisible(true);
    searchInput.setPopupMenuEnabled(true);
    searchInput.setTextToShowWhenEmpty("Search users...", SidechainColors::textMuted());
    searchInput.setFont(juce::Font(juce::FontOptions().withHeight(16.0f)));
    searchInput.addListener(this);
    addAndMakeVisible(searchInput);

    // Setup group name input
    groupNameInput.setMultiLine(false);
    groupNameInput.setReturnKeyStartsNewLine(false);
    groupNameInput.setScrollbarsShown(false);
    groupNameInput.setCaretVisible(true);
    groupNameInput.setPopupMenuEnabled(true);
    groupNameInput.setTextToShowWhenEmpty("Group name (optional)", SidechainColors::textMuted());
    groupNameInput.setFont(juce::Font(juce::FontOptions().withHeight(16.0f)));
    addChildComponent(groupNameInput);  // Hidden by default

    // Setup scrollbar
    scrollBar.setRangeLimits(0.0, 0.0);
    scrollBar.addListener(this);
    addAndMakeVisible(scrollBar);

    // Create error state component
    errorStateComponent = std::make_unique<ErrorState>();
    addChildComponent(errorStateComponent.get());

    // Start timer for debouncing search
    startTimer(SEARCH_DEBOUNCE_MS);
}

UserPickerDialog::~UserPickerDialog()
{
    stopTimer();
}

//==============================================================================
void UserPickerDialog::paint(juce::Graphics& g)
{
    // Background
    g.fillAll(SidechainColors::background());

    drawHeader(g);
    drawResults(g);
}

void UserPickerDialog::resized()
{
    auto bounds = getLocalBounds();

    // Header with search input
    auto headerBounds = bounds.removeFromTop(HEADER_HEIGHT);
    searchInput.setBounds(headerBounds.reduced(10, 10));

    // Group name input (if visible)
    if (showGroupNameInput)
    {
        auto groupNameBounds = bounds.removeFromTop(GROUP_NAME_INPUT_HEIGHT);
        groupNameInput.setBounds(groupNameBounds.reduced(10, 5));
    }

    // Scrollbar on right
    scrollBar.setBounds(bounds.removeFromRight(12));

    // Update scrollbar range based on content height
    int contentHeight = calculateContentHeight();
    int visibleHeight = bounds.getHeight() - BOTTOM_PADDING;
    scrollBar.setRangeLimits(0.0, static_cast<double>(juce::jmax(0, contentHeight - visibleHeight)));
    scrollBar.setCurrentRangeStart(scrollPosition, juce::dontSendNotification);

    // Error state component
    if (errorStateComponent)
    {
        errorStateComponent->setBounds(getLocalBounds());
    }
}

void UserPickerDialog::mouseUp(const juce::MouseEvent& event)
{
    auto bounds = getLocalBounds();
    bounds.removeFromTop(HEADER_HEIGHT);
    bounds.removeFromBottom(BUTTON_HEIGHT + 20);
    bounds.removeFromRight(12);

    auto pos = event.getPosition();
    if (bounds.contains(pos))
    {
        int adjustedY = pos.y - bounds.getY() + static_cast<int>(scrollPosition);
        int index = adjustedY / USER_ITEM_HEIGHT;
        if (index >= 0 && index < searchResults.size())
        {
            toggleUserSelection(searchResults[index].id);
        }
    }
}

void UserPickerDialog::drawHeader(juce::Graphics& g)
{
    auto bounds = getLocalBounds().removeFromTop(HEADER_HEIGHT);
    g.setColour(SidechainColors::backgroundLight());
    g.fillRect(bounds);

    g.setColour(SidechainColors::textPrimary());
    g.setFont(18.0f);
    g.drawText("Select Users", bounds.reduced(15, 0), juce::Justification::centredLeft);
}

void UserPickerDialog::drawResults(juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    bounds.removeFromTop(HEADER_HEIGHT);
    bounds.removeFromBottom(BUTTON_HEIGHT + 20);
    bounds.removeFromRight(12);  // Scrollbar width

    int yPos = bounds.getY() - static_cast<int>(scrollPosition);

    for (int i = 0; i < searchResults.size(); ++i)
    {
        juce::Rectangle<int> itemBounds(0, yPos + i * USER_ITEM_HEIGHT, bounds.getWidth(), USER_ITEM_HEIGHT);
        if (itemBounds.getBottom() < bounds.getY() || itemBounds.getY() > bounds.getBottom())
            continue;  // Off screen

        drawUserItem(g, searchResults[i], itemBounds.getY(), itemBounds.getWidth(), isUserSelected(searchResults[i].id));
    }
}

void UserPickerDialog::drawUserItem(juce::Graphics& g, const DiscoveredUser& user, int y, int width, bool isSelected)
{
    auto bounds = juce::Rectangle<int>(0, y, width, USER_ITEM_HEIGHT);

    // Background
    if (isSelected)
    {
        g.setColour(SidechainColors::accent().withAlpha(0.2f));
        g.fillRect(bounds);
    }
    else
    {
        g.setColour(SidechainColors::backgroundLight());
        g.fillRect(bounds);
    }

    // Avatar circle
    int avatarSize = 40;
    auto avatarBounds = bounds.withSizeKeepingCentre(avatarSize, avatarSize).withX(15);
    g.setColour(SidechainColors::surface());
    g.fillEllipse(avatarBounds.toFloat());

    // Avatar initials
    g.setColour(SidechainColors::textPrimary());
    g.setFont(14.0f);
    juce::String initials = user.username.substring(0, 1).toUpperCase();
    g.drawText(initials, avatarBounds, juce::Justification::centred);

    // Username
    auto textBounds = bounds.withTrimmedLeft(avatarBounds.getRight() + 15).withTrimmedRight(50);
    g.setColour(SidechainColors::textPrimary());
    g.setFont(16.0f);
    g.drawText(user.username, textBounds, juce::Justification::centredLeft);

    // Selection indicator
    if (isSelected)
    {
        auto checkBounds = bounds.removeFromRight(40);
        g.setColour(SidechainColors::accent());
        g.setFont(16.0f);
        g.drawText("[x]", checkBounds, juce::Justification::centred);
    }
}

//==============================================================================
void UserPickerDialog::textEditorTextChanged(juce::TextEditor& editor)
{
    if (&editor == &searchInput)
    {
        currentSearchQuery = editor.getText().trim();
        lastSearchTime = juce::Time::getCurrentTime().toMilliseconds();
        // Timer will trigger search after debounce
    }
}

void UserPickerDialog::textEditorReturnKeyPressed(juce::TextEditor& editor)
{
    if (&editor == &searchInput)
    {
        performSearch(currentSearchQuery);
    }
}

// Button clicks are handled in mouseUp() since we draw buttons manually

void UserPickerDialog::timerCallback()
{
    // Debounced search
    if (currentSearchQuery.isNotEmpty() && !isSearching)
    {
        int64_t currentTime = juce::Time::getCurrentTime().toMilliseconds();
        if (currentTime - lastSearchTime >= SEARCH_DEBOUNCE_MS)
        {
            performSearch(currentSearchQuery);
        }
    }
}

//==============================================================================
void UserPickerDialog::setNetworkClient(NetworkClient* client)
{
    networkClient = client;
}

void UserPickerDialog::performSearch(const juce::String& query)
{
    if (networkClient == nullptr || query.isEmpty())
    {
        searchResults.clear();
        repaint();
        return;
    }

    isSearching = true;
    networkClient->searchUsers(query, 20, 0, [this](Outcome<juce::var> result) {
        isSearching = false;

        if (result.isOk() && result.getValue().isObject())
        {
            auto response = result.getValue();
            searchResults.clear();
            auto usersArray = response.getProperty("users", juce::var());
            if (usersArray.isArray())
            {
                for (int i = 0; i < usersArray.size(); ++i)
                {
                    auto userJson = usersArray[i];
                    DiscoveredUser user = DiscoveredUser::fromJson(userJson);

                    // Exclude current user and excluded users
                    if (user.id != currentUserId && !excludedUserIds.contains(user.id))
                    {
                        searchResults.add(user);
                    }
                }
            }
        }

        juce::MessageManager::callAsync([this]() {
            repaint();
            resized();
        });
    });
}

void UserPickerDialog::toggleUserSelection(const juce::String& userId)
{
    if (selectedUserIds.count(userId) > 0)
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
    return selectedUserIds.count(userId) > 0;
}

juce::Rectangle<int> UserPickerDialog::getUserItemBounds(int index) const
{
    auto bounds = getLocalBounds();
    bounds.removeFromTop(HEADER_HEIGHT);
    bounds.removeFromBottom(BUTTON_HEIGHT + 20);
    bounds.removeFromRight(12);
    return juce::Rectangle<int>(0, bounds.getY() - static_cast<int>(scrollPosition) + index * USER_ITEM_HEIGHT, bounds.getWidth(), USER_ITEM_HEIGHT);
}

int UserPickerDialog::getItemIndexAtY(int y) const
{
    auto bounds = getLocalBounds();
    bounds.removeFromTop(HEADER_HEIGHT);
    bounds.removeFromBottom(BUTTON_HEIGHT + 20);
    bounds.removeFromRight(12);

    int adjustedY = y + static_cast<int>(scrollPosition) - bounds.getY();
    int index = adjustedY / USER_ITEM_HEIGHT;
    if (index >= 0 && index < searchResults.size())
        return index;
    return -1;
}

void UserPickerDialog::showModal(juce::Component* parentComponent)
{
    if (parentComponent == nullptr)
        return;

    // Default dialog size
    static constexpr int DIALOG_WIDTH = 500;
    static constexpr int DIALOG_HEIGHT = 600;

    // Center dialog
    auto parentBounds = parentComponent->getLocalBounds();
    int x = (parentBounds.getWidth() - DIALOG_WIDTH) / 2;
    int y = (parentBounds.getHeight() - DIALOG_HEIGHT) / 2;
    setBounds(x, y, DIALOG_WIDTH, DIALOG_HEIGHT);

    parentComponent->addAndMakeVisible(this);
    toFront(true);
    searchInput.grabKeyboardFocus();

    // Perform initial search if there's a query
    if (currentSearchQuery.isNotEmpty())
    {
        performSearch(currentSearchQuery);
    }
}
