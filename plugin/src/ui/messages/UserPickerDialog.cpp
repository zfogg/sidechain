#include "UserPickerDialog.h"
#include "../../network/NetworkClient.h"
#include "../../stores/AppStore.h"
#include "../../util/Colors.h"
#include "../../util/Log.h"

using namespace Sidechain::Stores;

UserPickerDialog::UserPickerDialog()
    : scrollBar(true) // Initialize scrollbar as vertical
{
  Log::info("UserPickerDialog: Initializing");

  // Setup search input
  searchInput.setMultiLine(false);
  searchInput.setReturnKeyStartsNewLine(false);
  searchInput.setPopupMenuEnabled(true);
  searchInput.setTextToShowWhenEmpty("Search for people...", SidechainColors::textMuted());
  searchInput.setColour(juce::TextEditor::backgroundColourId, SidechainColors::surface());
  searchInput.setColour(juce::TextEditor::outlineColourId, SidechainColors::border());
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
  groupNameInput.setColour(juce::TextEditor::backgroundColourId, SidechainColors::surface());
  groupNameInput.setColour(juce::TextEditor::outlineColourId, SidechainColors::border());
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

UserPickerDialog::~UserPickerDialog() {
  Log::debug("UserPickerDialog: Destroying");
}

void UserPickerDialog::paint(juce::Graphics &g) {
  // Background
  g.fillAll(SidechainColors::background());

  if (dialogState == DialogState::Error) {
    drawErrorState(g);
    return;
  }

  if (dialogState == DialogState::Loading) {
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
  if (!recentUsers.empty() && currentSearchQuery.isEmpty()) {
    drawSectionHeader(g, "Recent", y);
    y += SECTION_HEADER_HEIGHT;

    for (size_t i = 0; i < recentUsers.size(); ++i) {
      bool isSelected = selectedUserIds.find(recentUsers[i].userId) != selectedUserIds.end();
      drawUserItem(g, recentUsers[i], y, isSelected);
      y += USER_ITEM_HEIGHT;
    }

    y += 10; // Section spacing
  }

  // Suggested users section
  if (!suggestedUsers.empty() && currentSearchQuery.isEmpty()) {
    drawSectionHeader(g, "Suggested", y);
    y += SECTION_HEADER_HEIGHT;

    for (size_t i = 0; i < suggestedUsers.size(); ++i) {
      bool isSelected = selectedUserIds.find(suggestedUsers[i].userId) != selectedUserIds.end();
      drawUserItem(g, suggestedUsers[i], y, isSelected);
      y += USER_ITEM_HEIGHT;
    }
  }

  // Empty state - show helpful text if no data loaded yet
  if (recentUsers.empty() && suggestedUsers.empty() && currentSearchQuery.isEmpty()) {
    g.setColour(SidechainColors::textMuted());
    g.setFont(juce::FontOptions(14.0f));
    g.drawText("Search for people to start a conversation", 10, y + 40, getWidth() - 20, 60,
               juce::Justification::centredLeft);
  }

  // Search results section
  if (!currentSearchQuery.isEmpty()) {
    if (isSearching) {
      g.setColour(SidechainColors::textSecondary());
      g.setFont(juce::FontOptions(14.0f));
      g.drawText("Searching...", 0, y, getWidth(), 40, juce::Justification::centred);
    } else if (searchResults.empty()) {
      g.setColour(SidechainColors::textMuted());
      g.setFont(juce::FontOptions(14.0f));
      g.drawText("No users found", 0, y, getWidth(), 40, juce::Justification::centred);
    } else {
      drawSectionHeader(g, "Results", y);
      y += SECTION_HEADER_HEIGHT;

      for (size_t i = 0; i < searchResults.size(); ++i) {
        bool isSelected = selectedUserIds.find(searchResults[i].userId) != selectedUserIds.end();
        drawUserItem(g, searchResults[i], y, isSelected);
        y += USER_ITEM_HEIGHT;
      }
    }
  }

  // Draw action buttons at bottom
  drawActionButtons(g);
}

void UserPickerDialog::resized() {
  auto bounds = getLocalBounds();

  // Position scroll bar on right
  scrollBar.setBounds(bounds.removeFromRight(16));

  // Position search input
  int searchY = HEADER_HEIGHT;
  searchInput.setBounds(15, searchY + 10, bounds.getWidth() - 30, SEARCH_INPUT_HEIGHT - 20);

  // Position group name input (if visible)
  if (showGroupNameInput) {
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

void UserPickerDialog::mouseUp(const juce::MouseEvent &event) {
  auto pos = event.getPosition();

  // Close button
  if (getCloseButtonBounds().contains(pos)) {
    cancel();
    return;
  }

  // Cancel button
  if (getCancelButtonBounds().contains(pos)) {
    cancel();
    return;
  }

  // Create/Send button
  if (getCreateButtonBounds().contains(pos)) {
    createConversation();
    return;
  }

  // Check for clicks on user items
  int contentY = HEADER_HEIGHT + SEARCH_INPUT_HEIGHT;
  if (showGroupNameInput)
    contentY += GROUP_NAME_INPUT_HEIGHT;

  int y = contentY - static_cast<int>(scrollPosition);

  // Recent users
  if (!recentUsers.empty() && currentSearchQuery.isEmpty()) {
    y += SECTION_HEADER_HEIGHT;
    for (size_t i = 0; i < recentUsers.size(); ++i) {
      auto itemBounds = getUserItemBounds(0, y);
      if (itemBounds.contains(pos)) {
        toggleUserSelection(recentUsers[i].userId);
        return;
      }
      y += USER_ITEM_HEIGHT;
    }
    y += 10;
  }

  // Suggested users
  if (!suggestedUsers.empty() && currentSearchQuery.isEmpty()) {
    y += SECTION_HEADER_HEIGHT;
    for (size_t i = 0; i < suggestedUsers.size(); ++i) {
      auto itemBounds = getUserItemBounds(0, y);
      if (itemBounds.contains(pos)) {
        toggleUserSelection(suggestedUsers[i].userId);
        return;
      }
      y += USER_ITEM_HEIGHT;
    }
  }

  // Search results
  if (!currentSearchQuery.isEmpty() && !searchResults.empty()) {
    y += SECTION_HEADER_HEIGHT;
    for (size_t i = 0; i < searchResults.size(); ++i) {
      auto itemBounds = getUserItemBounds(0, y);
      if (itemBounds.contains(pos)) {
        toggleUserSelection(searchResults[i].userId);
        return;
      }
      y += USER_ITEM_HEIGHT;
    }
  }
}

void UserPickerDialog::mouseWheelMove(const juce::MouseEvent &event, const juce::MouseWheelDetails &wheel) {
  // Only scroll if wheel is within content area (not over scroll bar)
  int contentY = HEADER_HEIGHT + SEARCH_INPUT_HEIGHT;
  if (showGroupNameInput)
    contentY += GROUP_NAME_INPUT_HEIGHT;

  if (event.x < getWidth() - scrollBar.getWidth() && event.y >= contentY) {
    scrollPosition -= wheel.deltaY * 30.0;
    scrollPosition = juce::jlimit(0.0, scrollBar.getMaximumRangeLimit(), scrollPosition);
    scrollBar.setCurrentRangeStart(scrollPosition, juce::dontSendNotification);
    repaint();
  }
}

void UserPickerDialog::textEditorTextChanged(juce::TextEditor &editor) {
  if (&editor == &searchInput) {
    // Start debounce timer for search
    stopTimer();
    startTimer(SEARCH_DEBOUNCE_MS);
  }
}

void UserPickerDialog::textEditorReturnKeyPressed(juce::TextEditor &editor) {
  if (&editor == &searchInput) {
    // Trigger search immediately
    stopTimer();
    performSearch(searchInput.getText());
  } else if (&editor == &groupNameInput) {
    // Enter key in group name = create conversation
    createConversation();
  }
}

void UserPickerDialog::timerCallback() {
  // Debounced search triggered
  stopTimer();
  performSearch(searchInput.getText());
}

void UserPickerDialog::scrollBarMoved(juce::ScrollBar *scrollBarPtr, double newRangeStart) {
  // Verify scroll bar callback is from our scroll bar instance
  if (scrollBarPtr == &scrollBar) {
    scrollPosition = newRangeStart;
    repaint();
  }
}

void UserPickerDialog::setStreamChatClient(StreamChatClient *client) {
  streamChatClient = client;
}

void UserPickerDialog::setNetworkClient(NetworkClient *client) {
  networkClient = client;
}

void UserPickerDialog::loadRecentConversations() {
  if (!streamChatClient || !streamChatClient->isAuthenticated()) {
    Log::error("UserPickerDialog: Cannot load recent conversations - not "
               "authenticated");
    dialogState = DialogState::Showing;
    repaint();
    return;
  }

  Log::info("UserPickerDialog: Loading recent conversations");
  dialogState = DialogState::Loading;
  repaint();

  juce::Component::SafePointer<UserPickerDialog> safeThis(this);

  // Query recent channels to get recent conversation partners
  streamChatClient->queryChannels(
      [safeThis](Outcome<std::vector<StreamChatClient::Channel>> result) {
        if (safeThis == nullptr)
          return;

        safeThis->dialogState = DialogState::Showing;

        if (result.isError()) {
          Log::error("UserPickerDialog: Failed to load channels - " + result.getError());
          safeThis->repaint();
          return;
        }

        auto channels = result.getValue();
        safeThis->recentUsers.clear();

        // Extract unique users from channels
        std::set<juce::String> seenUserIds;

        for (const auto &channel : channels) {
          // Skip group channels for "recent" section (use members array size)
          if (channel.members.isArray() && channel.members.size() > 2)
            continue;

          // Get the other user from the channel
          if (channel.members.isArray()) {
            for (int i = 0; i < channel.members.size(); ++i) {
              auto member = channel.members[i];
              juce::String userId = member.getProperty("user_id", "").toString();

              // Skip self and duplicates
              if (userId == safeThis->currentUserId || userId.isEmpty())
                continue;
              if (seenUserIds.find(userId) != seenUserIds.end())
                continue;

              seenUserIds.insert(userId);

              // Create UserItem from member data
              UserItem user;
              user.userId = userId;
              user.username = member.getProperty("username", "").toString();
              user.displayName = member.getProperty("display_name", "").toString();
              user.profilePictureUrl = member.getProperty("avatar_url", "").toString();

              safeThis->recentUsers.push_back(user);

              // Limit to 5 recent users
              if (safeThis->recentUsers.size() >= 5)
                break;
            }

            if (safeThis->recentUsers.size() >= 5)
              break;
          }
        }

        Log::info("UserPickerDialog: Loaded " + juce::String(safeThis->recentUsers.size()) + " recent users");
        safeThis->resized();
        safeThis->repaint();
      },
      20, 0);
}

void UserPickerDialog::loadSuggestedUsers() {
  if (!networkClient) {
    Log::error("UserPickerDialog: Cannot load suggested users - no NetworkClient");
    return;
  }

  Log::info("UserPickerDialog: Loading suggested users");

  juce::Component::SafePointer<UserPickerDialog> safeThis(this);

  // Get suggested users based on shared interests
  networkClient->getSuggestedUsers(10, [safeThis](Outcome<juce::var> result) {
    if (safeThis == nullptr)
      return;

    if (result.isError()) {
      Log::error("UserPickerDialog: Failed to load suggested users - " + result.getError());
      return;
    }

    auto data = result.getValue();
    safeThis->suggestedUsers.clear();

    if (data.isArray()) {
      for (int i = 0; i < data.size(); ++i) {
        auto userObj = data[i];

        // Skip excluded users
        juce::String userId = userObj.getProperty("id", "").toString();
        if (userId.isEmpty() || safeThis->excludedUserIds.contains(userId))
          continue;

        UserItem user;
        user.userId = userId;
        user.username = userObj.getProperty("username", "").toString();
        user.displayName = userObj.getProperty("display_name", "").toString();
        user.profilePictureUrl = userObj.getProperty("profile_picture_url", "").toString();
        user.isFollowing = userObj.getProperty("is_following", false);
        user.followsMe = userObj.getProperty("follows_me", false);
        user.isOnline = userObj.getProperty("is_online", false);

        safeThis->suggestedUsers.push_back(user);
      }
    }

    Log::info("UserPickerDialog: Loaded " + juce::String(safeThis->suggestedUsers.size()) + " suggested users");
    safeThis->resized();
    safeThis->repaint();
  });
}

//==============================================================================
// Drawing helpers

void UserPickerDialog::drawHeader(juce::Graphics &g) {
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
  g.drawText(juce::String(juce::CharPointer_UTF8("\xE2\x9C\x96")), closeBounds, juce::Justification::centred);

  // Bottom border
  g.setColour(SidechainColors::border());
  g.drawLine(0.0f, static_cast<float>(HEADER_HEIGHT), static_cast<float>(getWidth()), static_cast<float>(HEADER_HEIGHT),
             1.0f);
}

void UserPickerDialog::drawSearchInput(juce::Graphics &g) {
  // Draw search input background and styling
  auto bounds = juce::Rectangle<int>(15, HEADER_HEIGHT + 10, getWidth() - 30, SEARCH_INPUT_HEIGHT - 20);

  // Set up TextEditor bounds
  searchInput.setBounds(bounds);

  // Draw subtle background highlight when focused
  if (searchInput.hasKeyboardFocus(true)) {
    g.setColour(SidechainColors::accent().withAlpha(0.05f));
    g.fillRoundedRectangle(bounds.toFloat(), 8.0f);
  }
}

void UserPickerDialog::drawGroupNameInput(juce::Graphics &g) {
  // Only draw if visible
  if (!showGroupNameInput)
    return;

  // Draw group name input background and styling
  auto bounds =
      juce::Rectangle<int>(15, HEADER_HEIGHT + SEARCH_INPUT_HEIGHT + 10, getWidth() - 30, GROUP_NAME_INPUT_HEIGHT - 20);

  // Set up TextEditor bounds
  groupNameInput.setBounds(bounds);

  // Draw subtle background highlight when focused
  if (groupNameInput.hasKeyboardFocus(true)) {
    g.setColour(SidechainColors::accent().withAlpha(0.05f));
    g.fillRoundedRectangle(bounds.toFloat(), 8.0f);
  }
}

void UserPickerDialog::drawSectionHeader(juce::Graphics &g, const juce::String &title, int y) {
  g.setColour(SidechainColors::textSecondary());
  g.setFont(juce::FontOptions(12.0f).withStyle("Bold"));
  g.drawText(title.toUpperCase(), 15, y + 10, getWidth() - 30, 20, juce::Justification::centredLeft);
}

void UserPickerDialog::drawUserItem(juce::Graphics &g, const UserItem &user, int y, bool isSelected) {
  auto itemBounds = getUserItemBounds(0, y);

  // Selection background
  if (isSelected) {
    g.setColour(SidechainColors::accent().withAlpha(0.1f));
    g.fillRoundedRectangle(itemBounds.reduced(5, 2).toFloat(), 8.0f);
  }

  int x = 15;

  // Avatar (placeholder circle)
  int avatarSize = 50;
  int avatarY = y + (USER_ITEM_HEIGHT - avatarSize) / 2;

  g.setColour(SidechainColors::surface());
  g.fillEllipse(static_cast<float>(x), static_cast<float>(avatarY), static_cast<float>(avatarSize),
                static_cast<float>(avatarSize));

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
  if (user.isOnline) {
    int dotSize = 14;
    int dotX = x + avatarSize - dotSize + 2;
    int dotY = avatarY + avatarSize - dotSize + 2;

    // White border
    g.setColour(SidechainColors::background());
    g.fillEllipse(static_cast<float>(dotX - 1), static_cast<float>(dotY - 1), static_cast<float>(dotSize + 2),
                  static_cast<float>(dotSize + 2));

    // Green dot
    g.setColour(SidechainColors::onlineIndicator());
    g.fillEllipse(static_cast<float>(dotX), static_cast<float>(dotY), static_cast<float>(dotSize),
                  static_cast<float>(dotSize));
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
  if (user.isFollowing && user.followsMe) {
    g.setColour(SidechainColors::accent().withAlpha(0.2f));
    g.fillRoundedRectangle(static_cast<float>(badgeX), static_cast<float>(y + 20), 75.0f, 24.0f, 12.0f);
    g.setColour(SidechainColors::accent());
    g.setFont(juce::FontOptions(11.0f).withStyle("Bold"));
    g.drawText("MUTUAL", badgeX, y + 20, 75, 24, juce::Justification::centred);
  } else if (user.followsMe) {
    g.setColour(SidechainColors::textMuted());
    g.setFont(juce::FontOptions(11.0f));
    g.drawText("Follows you", badgeX, y + 25, 75, 20, juce::Justification::centredRight);
  }

  // Selection checkmark
  if (isSelected) {
    int checkX = getWidth() - scrollBar.getWidth() - 40;
    g.setColour(SidechainColors::accent());
    g.setFont(juce::FontOptions(24.0f));
    g.drawText(juce::String::fromUTF8("✓"), checkX, y, 30, USER_ITEM_HEIGHT, juce::Justification::centred);
  }
}

void UserPickerDialog::drawActionButtons(juce::Graphics &g) {
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

void UserPickerDialog::drawLoadingState(juce::Graphics &g) {
  g.setColour(SidechainColors::textSecondary());
  g.setFont(juce::FontOptions(16.0f));
  g.drawText("Loading...", 0, 0, getWidth(), getHeight(), juce::Justification::centred);
}

void UserPickerDialog::drawErrorState(juce::Graphics &g) {
  // Draw error background
  g.fillAll(SidechainColors::background());

  // Draw error icon and message
  int centerY = getHeight() / 2;

  // Error icon (simple circle with X)
  int iconSize = 80;
  int iconX = (getWidth() - iconSize) / 2;
  int iconY = centerY - 60;

  g.setColour(SidechainColors::error().withAlpha(0.2f));
  g.fillEllipse(static_cast<float>(iconX), static_cast<float>(iconY), static_cast<float>(iconSize),
                static_cast<float>(iconSize));

  g.setColour(SidechainColors::error());
  g.setFont(juce::FontOptions(48.0f).withStyle("Bold"));
  g.drawText("!", iconX, iconY, iconSize, iconSize, juce::Justification::centred);

  // Error message
  g.setColour(SidechainColors::textPrimary());
  g.setFont(juce::FontOptions(16.0f).withStyle("Bold"));
  g.drawText("Failed to Load Users", 20, centerY + 20, getWidth() - 40, 30, juce::Justification::centred);

  // Error detail
  g.setColour(SidechainColors::textSecondary());
  g.setFont(juce::FontOptions(13.0f));
  g.drawText(errorMessage, 20, centerY + 60, getWidth() - 40, 60, juce::Justification::centred, true);
}

void UserPickerDialog::drawEmptyState(juce::Graphics &g) {
  // Draw empty state when no users are found
  int contentY = HEADER_HEIGHT + SEARCH_INPUT_HEIGHT;
  if (showGroupNameInput)
    contentY += GROUP_NAME_INPUT_HEIGHT;

  int centerY = contentY + (getHeight() - contentY) / 2;

  // Empty state icon
  int iconSize = 80;
  int iconX = (getWidth() - iconSize) / 2;
  int iconY = centerY - 60;

  g.setColour(SidechainColors::textMuted().withAlpha(0.2f));
  g.fillEllipse(static_cast<float>(iconX), static_cast<float>(iconY), static_cast<float>(iconSize),
                static_cast<float>(iconSize));

  g.setColour(SidechainColors::textMuted());
  g.setFont(juce::FontOptions(40.0f));
  g.drawText("👥", iconX, iconY, iconSize, iconSize, juce::Justification::centred);

  // Empty state message
  g.setColour(SidechainColors::textSecondary());
  g.setFont(juce::FontOptions(15.0f).withStyle("Bold"));
  g.drawText("No users found", 20, centerY + 20, getWidth() - 40, 30, juce::Justification::centred);

  // Helper text
  g.setColour(SidechainColors::textMuted());
  g.setFont(juce::FontOptions(13.0f));
  g.drawText("Try searching for a friend to start a conversation", 20, centerY + 60, getWidth() - 40, 40,
             juce::Justification::centred, true);
}

//==============================================================================
// Helper methods

int UserPickerDialog::calculateContentHeight() {
  int height = 0;

  // Recent users
  if (!recentUsers.empty() && currentSearchQuery.isEmpty()) {
    height += SECTION_HEADER_HEIGHT;
    height += static_cast<int>(recentUsers.size()) * USER_ITEM_HEIGHT;
    height += 10; // Section spacing
  }

  // Suggested users
  if (!suggestedUsers.empty() && currentSearchQuery.isEmpty()) {
    height += SECTION_HEADER_HEIGHT;
    height += static_cast<int>(suggestedUsers.size()) * USER_ITEM_HEIGHT;
  }

  // Search results
  if (!currentSearchQuery.isEmpty() && !searchResults.empty()) {
    height += SECTION_HEADER_HEIGHT;
    height += static_cast<int>(searchResults.size()) * USER_ITEM_HEIGHT;
  }

  return height;
}

void UserPickerDialog::performSearch(const juce::String &query) {
  currentSearchQuery = query.trim();

  if (currentSearchQuery.isEmpty()) {
    searchResults.clear();
    isSearching = false;
    repaint();
    return;
  }

  if (!networkClient) {
    Log::error("UserPickerDialog: Cannot search - no NetworkClient");
    return;
  }

  Log::info("UserPickerDialog: Searching for: " + currentSearchQuery);
  isSearching = true;
  repaint();

  juce::Component::SafePointer<UserPickerDialog> safeThis(this);
  auto &appStore = AppStore::getInstance();

  // Search users by username or display name via AppStore (with caching)
  appStore.searchUsersObservable(currentSearchQuery)
      .subscribe(
          [safeThis](const juce::Array<juce::var> &users) {
            if (safeThis == nullptr)
              return;

            safeThis->isSearching = false;
            safeThis->searchResults.clear();

            for (const auto &userObj : users) {
              // Skip excluded users
              juce::String userId = userObj.getProperty("id", "").toString();
              if (userId.isEmpty() || safeThis->excludedUserIds.contains(userId))
                continue;

              UserItem user;
              user.userId = userId;
              user.username = userObj.getProperty("username", "").toString();
              user.displayName = userObj.getProperty("display_name", "").toString();
              user.profilePictureUrl = userObj.getProperty("profile_picture_url", "").toString();
              user.isFollowing = userObj.getProperty("is_following", false);
              user.followsMe = userObj.getProperty("follows_me", false);
              user.isOnline = userObj.getProperty("is_online", false);

              safeThis->searchResults.push_back(user);
            }

            Log::info("UserPickerDialog: Found " + juce::String(safeThis->searchResults.size()) + " users");
            safeThis->resized();
            safeThis->repaint();
          },
          [safeThis](std::exception_ptr) {
            if (safeThis == nullptr)
              return;

            safeThis->isSearching = false;
            Log::error("UserPickerDialog: Search failed");
            safeThis->searchResults.clear();
            safeThis->repaint();
          });
}

void UserPickerDialog::toggleUserSelection(const juce::String &userId) {
  if (selectedUserIds.find(userId) != selectedUserIds.end()) {
    selectedUserIds.erase(userId);
  } else {
    selectedUserIds.insert(userId);
  }

  updateGroupNameInputVisibility();
  repaint();
}

bool UserPickerDialog::isUserSelected(const juce::String &userId) const {
  return selectedUserIds.find(userId) != selectedUserIds.end();
}

void UserPickerDialog::updateGroupNameInputVisibility() {
  bool shouldShow = selectedUserIds.size() >= 2;
  if (shouldShow != showGroupNameInput) {
    showGroupNameInput = shouldShow;
    groupNameInput.setVisible(shouldShow);
    resized();
  }
}

void UserPickerDialog::createConversation() {
  if (selectedUserIds.empty()) {
    Log::info("UserPickerDialog: No users selected");
    return;
  }

  if (selectedUserIds.size() == 1) {
    // Single user - create DM
    juce::String userId = *selectedUserIds.begin();
    Log::info("UserPickerDialog: Creating DM with user: " + userId);

    if (onUserSelected)
      onUserSelected(userId);
  } else {
    // Multiple users - create group
    std::vector<juce::String> userIds(selectedUserIds.begin(), selectedUserIds.end());
    juce::String groupName = groupNameInput.getText().trim();

    Log::info("UserPickerDialog: Creating group with " + juce::String(userIds.size()) + " users");

    if (onGroupCreated)
      onGroupCreated(userIds, groupName);
  }
}

void UserPickerDialog::cancel() {
  Log::info("UserPickerDialog: Cancelled");

  if (onCancelled)
    onCancelled();
}

//==============================================================================
// Hit test bounds

juce::Rectangle<int> UserPickerDialog::getUserItemBounds(int index, int yOffset) const {
  return juce::Rectangle<int>(0, yOffset + index * USER_ITEM_HEIGHT, getWidth() - scrollBar.getWidth(),
                              USER_ITEM_HEIGHT);
}

juce::Rectangle<int> UserPickerDialog::getCreateButtonBounds() const {
  int y = getHeight() - BOTTOM_PADDING + 15;
  int width = (getWidth() - scrollBar.getWidth() - 45) / 2;
  return juce::Rectangle<int>(getWidth() - scrollBar.getWidth() - 15 - width, y, width, BUTTON_HEIGHT);
}

juce::Rectangle<int> UserPickerDialog::getCancelButtonBounds() const {
  int y = getHeight() - BOTTOM_PADDING + 15;
  int width = (getWidth() - scrollBar.getWidth() - 45) / 2;
  return juce::Rectangle<int>(15, y, width, BUTTON_HEIGHT);
}

juce::Rectangle<int> UserPickerDialog::getCloseButtonBounds() const {
  return juce::Rectangle<int>(getWidth() - 50, 0, 50, HEADER_HEIGHT);
}

//==============================================================================
// API helpers

void UserPickerDialog::handleSearchResults(const std::vector<UserItem> &results) {
  searchResults = results;
  isSearching = false;
  repaint();
}

void UserPickerDialog::handleError(const juce::String &error) {
  errorMessage = error;
  dialogState = DialogState::Error;
  repaint();
}

void UserPickerDialog::setExcludedUserIds(const juce::Array<juce::String> &userIds) {
  excludedUserIds = userIds;
}

void UserPickerDialog::showModal(juce::Component *parent) {
  if (parent == nullptr)
    return;

  // Set dialog size and position
  auto parentBounds = parent->getLocalBounds();
  int width = 500;
  int height = 600;
  int x = (parentBounds.getWidth() - width) / 2;
  int y = (parentBounds.getHeight() - height) / 2;
  setBounds(x, y, width, height);

  parent->addAndMakeVisible(this);
  toFront(true);
  searchInput.grabKeyboardFocus();

  // Load initial data - both recent conversations and suggested users
  loadRecentConversations();
  loadSuggestedUsers();
}
