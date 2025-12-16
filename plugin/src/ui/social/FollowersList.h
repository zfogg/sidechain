#pragma once

#include "../../stores/FollowersStore.h"
#include "../../util/Colors.h"
#include "../../util/HoverState.h"
#include "../../util/Time.h"
#include <JuceHeader.h>
#include <memory>

class NetworkClient;

// Use FollowListUser from FollowersStore instead of defining our own
using FollowListUser = Sidechain::Stores::FollowListUser;

//==============================================================================
/**
 * FollowUserRow displays a single user in the followers/following list
 */
class FollowUserRow : public juce::Component {
public:
  FollowUserRow();
  ~FollowUserRow() override = default;

  void setUser(const FollowListUser &user);
  const FollowListUser &getUser() const {
    return user;
  }

  // Update follow state
  void setFollowing(bool following);

  // Callbacks
  std::function<void(const FollowListUser &)> onUserClicked;
  std::function<void(const FollowListUser &, bool willFollow)> onFollowToggled;

  // Component overrides
  void paint(juce::Graphics &g) override;
  void resized() override;
  void mouseUp(const juce::MouseEvent &event) override;
  void mouseEnter(const juce::MouseEvent &event) override;
  void mouseExit(const juce::MouseEvent &event) override;

  static constexpr int ROW_HEIGHT = 70;

private:
  FollowListUser user;
  HoverState hoverState;

  // Cached avatar
  juce::Image avatarImage;

  // Bounds helpers
  juce::Rectangle<int> getAvatarBounds() const;
  juce::Rectangle<int> getFollowButtonBounds() const;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FollowUserRow)
};

//==============================================================================
/**
 * FollowersList displays a list of followers or following users
 *
 * Features:
 * - Scrollable list of users
 * - Follow/unfollow buttons
 * - Click to view profile
 * - Pagination support
 */
class FollowersList : public juce::Component, private juce::Timer {
public:
  enum class ListType { Followers, Following };

  FollowersList();
  ~FollowersList() override;

  //==============================================================================
  // Setup
  void setNetworkClient(NetworkClient *client) {
    networkClient = client;
  }
  void setCurrentUserId(const juce::String &userId) {
    currentUserId = userId;
  }

  // Load followers or following for a user
  void loadList(const juce::String &userId, ListType type);
  void refresh();

  //==============================================================================
  // Store integration (reactive pattern)
  void bindToStore(std::shared_ptr<Sidechain::Stores::FollowersStore> store);
  void unbindFromStore();

  //==============================================================================
  // Callbacks
  std::function<void()> onClose;
  std::function<void(const juce::String &userId)> onUserClicked;

  //==============================================================================
  // Component overrides
  void paint(juce::Graphics &g) override;
  void resized() override;

  //==============================================================================
  // Layout constants
  static constexpr int HEADER_HEIGHT = 50;

private:
  //==============================================================================
  // Timer for auto-refresh
  void timerCallback() override;

  //==============================================================================
  // Data
  NetworkClient *networkClient = nullptr;
  juce::String targetUserId;  // User whose followers/following we're viewing
  juce::String currentUserId; // Currently logged in user
  ListType listType = ListType::Followers;
  juce::Array<FollowListUser> users;
  int totalCount = 0;
  bool isLoading = false;
  bool hasMore = false;
  int currentOffset = 0;
  juce::String errorMessage;

  // Store integration
  std::shared_ptr<Sidechain::Stores::FollowersStore> followersStore;
  std::function<void()> storeUnsubscriber;

  //==============================================================================
  // Store callback
  void handleStoreStateChanged(const Sidechain::Stores::FollowersState &state);

  //==============================================================================
  // UI Components
  std::unique_ptr<juce::Viewport> viewport;
  std::unique_ptr<juce::Component> contentContainer;
  juce::OwnedArray<FollowUserRow> userRows;
  std::unique_ptr<juce::TextButton> closeButton;

  //==============================================================================
  // Methods
  void setupUI();
  void updateUsersList();
  void loadMoreUsers();

  // API callbacks
  void handleUsersLoaded(bool success, const juce::var &usersData);
  void handleFollowToggled(const FollowListUser &user, bool willFollow);

  // Row callbacks
  void setupRowCallbacks(FollowUserRow *row);

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FollowersList)
};
