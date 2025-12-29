#pragma once

#include "../../network/StreamChatClient.h"
#include "../../stores/AppStore.h"
#include "../common/ErrorState.h"
#include <JuceHeader.h>
#include <rxcpp/rx.hpp>

class NetworkClient;

// ==============================================================================
/**
 * UserPickerDialog - Modal dialog for selecting users to start a conversation
 *
 * Features:
 * - Search input with real-time debounced search
 * - Recent conversations section
 * - Suggested users (mutual follows, frequent interactions)
 * - User search results with avatar, name, follow status, online status
 * - Multi-select support for creating group chats
 * - Group name input when 2+ users selected
 */
class UserPickerDialog : public juce::Component, public juce::TextEditor::Listener, public juce::ScrollBar::Listener {
public:
  UserPickerDialog();
  ~UserPickerDialog() override;

  void paint(juce::Graphics &) override;
  void resized() override;
  void mouseUp(const juce::MouseEvent &event) override;
  void mouseWheelMove(const juce::MouseEvent &event, const juce::MouseWheelDetails &wheel) override;

  // TextEditor::Listener
  void textEditorTextChanged(juce::TextEditor &editor) override;
  void textEditorReturnKeyPressed(juce::TextEditor &editor) override;

  // Callbacks
  std::function<void(const juce::String &userId)> onUserSelected;                // Single user selected
  std::function<void(const juce::Array<juce::String> &userIds)> onUsersSelected; // Multiple users selected (for adding
                                                                                 // to existing channel)
  std::function<void(const std::vector<juce::String> &userIds,
                     const juce::String &groupName)>
      onGroupCreated; // Multiple users selected (for creating new group)
  std::function<void()> onCancelled;

  // Set clients
  void setStreamChatClient(StreamChatClient *client);
  void setNetworkClient(NetworkClient *client);
  void setAppStore(Sidechain::Stores::AppStore *store);
  void setCurrentUserId(const juce::String &userId) {
    currentUserId = userId;
  }

  // Set excluded user IDs (e.g., already in the channel)
  void setExcludedUserIds(const juce::Array<juce::String> &userIds);

  // Show dialog modally
  void showModal(juce::Component *parent);

  // Load data
  void loadRecentConversations();
  void loadSuggestedUsers();

private:
  // ==============================================================================
  enum class DialogState { Loading, Showing, Error };

  DialogState dialogState = DialogState::Loading;
  juce::String errorMessage;

  StreamChatClient *streamChatClient = nullptr;
  NetworkClient *networkClient = nullptr;
  Sidechain::Stores::AppStore *appStore = nullptr;
  juce::String currentUserId;

  // RxCpp subscription management
  rxcpp::composite_subscription dialogSubscriptions_;
  rxcpp::subjects::subject<juce::String> searchQuerySubject_;
  rxcpp::composite_subscription searchSubscription_;

  void setupDebouncedSearch();

  // UI elements
  juce::TextEditor searchInput;
  juce::TextEditor groupNameInput; // Only visible when 2+ users selected
  juce::ScrollBar scrollBar;
  double scrollPosition = 0.0;

  static constexpr int HEADER_HEIGHT = 60;
  static constexpr int SEARCH_INPUT_HEIGHT = 50;
  static constexpr int GROUP_NAME_INPUT_HEIGHT = 50;
  static constexpr int USER_ITEM_HEIGHT = 70;
  static constexpr int SECTION_HEADER_HEIGHT = 40;
  static constexpr int BUTTON_HEIGHT = 50;
  static constexpr int BOTTOM_PADDING = 80; // Space for action buttons

  // Data
  struct UserItem {
    juce::String userId;
    juce::String username;
    juce::String displayName;
    juce::String profilePictureUrl;
    bool isFollowing = false;
    bool followsMe = false;
    bool isOnline = false;
    juce::String lastActive;
  };

  std::vector<UserItem> recentUsers;
  std::vector<UserItem> suggestedUsers;
  std::vector<UserItem> searchResults;
  std::set<juce::String> selectedUserIds;    // Multi-select support
  juce::Array<juce::String> excludedUserIds; // Users to exclude from results

  // Search state
  juce::String currentSearchQuery;
  bool isSearching = false;
  int64_t lastSearchTime = 0;
  static constexpr int SEARCH_DEBOUNCE_MS = 300;

  // UI state
  bool showGroupNameInput = false;
  std::unique_ptr<ErrorState> errorStateComponent;

  // Drawing helpers
  void drawHeader(juce::Graphics &g);
  void drawSearchInput(juce::Graphics &g);
  void drawGroupNameInput(juce::Graphics &g);
  void drawSectionHeader(juce::Graphics &g, const juce::String &title, int y);
  void drawUserItem(juce::Graphics &g, const UserItem &user, int y, bool isSelected);
  void drawActionButtons(juce::Graphics &g);
  void drawEmptyState(juce::Graphics &g);
  void drawErrorState(juce::Graphics &g);
  void drawLoadingState(juce::Graphics &g);

  // ScrollBar::Listener
  void scrollBarMoved(juce::ScrollBar *scrollBar, double newRangeStart) override;

  // Helper methods
  int calculateContentHeight();
  void performSearch(const juce::String &query);
  void toggleUserSelection(const juce::String &userId);
  bool isUserSelected(const juce::String &userId) const;
  void updateGroupNameInputVisibility();
  void createConversation();
  void cancel();

  // Hit test bounds
  juce::Rectangle<int> getUserItemBounds(int index, int yOffset) const;
  juce::Rectangle<int> getCreateButtonBounds() const;
  juce::Rectangle<int> getCancelButtonBounds() const;
  juce::Rectangle<int> getCloseButtonBounds() const;

  // API helpers
  void handleSearchResults(const std::vector<UserItem> &results);
  void handleError(const juce::String &error);

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UserPickerDialog)
};
