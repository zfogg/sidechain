#include "UserPickerDialog.h"
#include "../../network/NetworkClient.h"
#include "../../util/Colors.h"
#include "../../util/Log.h"
#include "../../util/Result.h"
#include "../social/UserCard.h"

//==============================================================================
UserPickerDialog::UserPickerDialog()
{
    // Create search input
    searchInput = std::make_unique<juce::TextEditor>();
    searchInput->setMultiLine(false);
    searchInput->setReturnKeyStartsNewLine(false);
    searchInput->setScrollbarsShown(false);
    searchInput->setCaretVisible(true);
    searchInput->setPopupMenuEnabled(true);
    searchInput->setTextToShowWhenEmpty("Search users...", SidechainColors::textMuted());
    searchInput->setFont(juce::Font(juce::FontOptions().withHeight(16.0f)));
    searchInput->addListener(this);
    addAndMakeVisible(searchInput.get());

    // Create buttons
    addButton = std::make_unique<juce::TextButton>("Add");
    addButton->addListener(this);
    addButton->setEnabled(false);
    addAndMakeVisible(addButton.get());

    cancelButton = std::make_unique<juce::TextButton>("Cancel");
    cancelButton->addListener(this);
    addAndMakeVisible(cancelButton.get());

    // Create scrollbar
    scrollBar = std::make_unique<juce::ScrollBar>(true);
    scrollBar->setRangeLimits(0.0, 0.0);
    addAndMakeVisible(scrollBar.get());

    // Set size last to avoid resized() being called before components are created
    setSize(DIALOG_WIDTH, DIALOG_HEIGHT);

    // Start timer for debouncing search
    startTimer(300);
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

    // Header
    auto headerBounds = bounds.removeFromTop(HEADER_HEIGHT);
    searchInput->setBounds(headerBounds.reduced(10, 10));

    // Buttons at bottom
    auto buttonBounds = bounds.removeFromBottom(BUTTON_HEIGHT + 20);
    cancelButton->setBounds(buttonBounds.removeFromRight(100).reduced(10, 10));
    addButton->setBounds(buttonBounds.removeFromRight(100).reduced(10, 10));

    // Scrollbar
    scrollBar->setBounds(bounds.removeFromRight(12));

    // Update scrollbar range
    int totalHeight = static_cast<int>(searchResults.size() * ITEM_HEIGHT);
    scrollBar->setRangeLimits(0.0, static_cast<double>(juce::jmax(0, totalHeight - bounds.getHeight())));
    scrollBar->setCurrentRangeStart(scrollPosition, juce::dontSendNotification);
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
        int index = adjustedY / ITEM_HEIGHT;
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
        juce::Rectangle<int> itemBounds(0, yPos + i * ITEM_HEIGHT, bounds.getWidth(), ITEM_HEIGHT);
        if (itemBounds.getBottom() < bounds.getY() || itemBounds.getY() > bounds.getBottom())
            continue;  // Off screen

        drawUserItem(g, searchResults[i], itemBounds.getY(), itemBounds.getWidth(), isUserSelected(searchResults[i].id));
    }
}

void UserPickerDialog::drawUserItem(juce::Graphics& g, const DiscoveredUser& user, int y, int width, bool isSelected)
{
    auto bounds = juce::Rectangle<int>(0, y, width, ITEM_HEIGHT);

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
    if (&editor == searchInput.get())
    {
        currentQuery = editor.getText().trim();
        // Timer will trigger search after debounce
    }
}

void UserPickerDialog::textEditorReturnKeyPressed(juce::TextEditor& editor)
{
    if (&editor == searchInput.get())
    {
        performSearch();
    }
}

void UserPickerDialog::buttonClicked(juce::Button* button)
{
    if (button == addButton.get())
    {
        if (onUsersSelected && selectedUserIds.size() > 0)
        {
            onUsersSelected(selectedUserIds);
        }
        // Close dialog
        juce::MessageManager::callAsync([this]() {
            setVisible(false);
            if (auto* parent = getParentComponent())
                parent->removeChildComponent(this);
        });
    }
    else if (button == cancelButton.get())
    {
        // Close dialog
        juce::MessageManager::callAsync([this]() {
            setVisible(false);
            if (auto* parent = getParentComponent())
                parent->removeChildComponent(this);
        });
    }
}

void UserPickerDialog::timerCallback()
{
    if (currentQuery.isNotEmpty() && !isSearching)
    {
        performSearch();
    }
}

//==============================================================================
void UserPickerDialog::setNetworkClient(NetworkClient* client)
{
    networkClient = client;
}

void UserPickerDialog::performSearch()
{
    if (networkClient == nullptr || currentQuery.isEmpty())
    {
        searchResults.clear();
        repaint();
        return;
    }

    isSearching = true;
    networkClient->searchUsers(currentQuery, 20, 0, [this](Outcome<juce::var> result) {
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
    int index = selectedUserIds.indexOf(userId);
    if (index >= 0)
    {
        selectedUserIds.remove(index);
    }
    else
    {
        selectedUserIds.add(userId);
    }

    addButton->setEnabled(selectedUserIds.size() > 0);
    repaint();
}

bool UserPickerDialog::isUserSelected(const juce::String& userId) const
{
    return selectedUserIds.contains(userId);
}

juce::Rectangle<int> UserPickerDialog::getUserItemBounds(int index) const
{
    auto bounds = getLocalBounds();
    bounds.removeFromTop(HEADER_HEIGHT);
    bounds.removeFromBottom(BUTTON_HEIGHT + 20);
    bounds.removeFromRight(12);
    return juce::Rectangle<int>(0, bounds.getY() - static_cast<int>(scrollPosition) + index * ITEM_HEIGHT, bounds.getWidth(), ITEM_HEIGHT);
}

int UserPickerDialog::getItemIndexAtY(int y) const
{
    auto bounds = getLocalBounds();
    bounds.removeFromTop(HEADER_HEIGHT);
    bounds.removeFromBottom(BUTTON_HEIGHT + 20);
    bounds.removeFromRight(12);

    int adjustedY = y + static_cast<int>(scrollPosition) - bounds.getY();
    int index = adjustedY / ITEM_HEIGHT;
    if (index >= 0 && index < searchResults.size())
        return index;
    return -1;
}

void UserPickerDialog::showModal(juce::Component* parentComponent)
{
    if (parentComponent == nullptr)
        return;

    // Center dialog
    auto parentBounds = parentComponent->getBounds();
    int x = parentBounds.getX() + (parentBounds.getWidth() - DIALOG_WIDTH) / 2;
    int y = parentBounds.getY() + (parentBounds.getHeight() - DIALOG_HEIGHT) / 2;
    setBounds(x, y, DIALOG_WIDTH, DIALOG_HEIGHT);

    parentComponent->addAndMakeVisible(this);
    toFront(true);
    searchInput->grabKeyboardFocus();

    // Perform initial search if there's a query
    if (currentQuery.isNotEmpty())
    {
        performSearch();
    }
}
