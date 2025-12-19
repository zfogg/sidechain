#pragma once

#include "../../util/Colors.h"
#include "../../util/HoverState.h"
#include "../../util/Time.h"
#include <JuceHeader.h>
#include <memory>

class NetworkClient;

namespace Sidechain {
namespace Stores {
class AppStore;
}
} // namespace Sidechain

// User data structure for followers/following lists
struct FollowListUser {
  juce::String id;
  juce::String username;
  juce::String displayName;
  juce::String avatarUrl;
  juce::String bio;
  bool isFollowing = false;
  bool followsYou = false;

  static FollowListUser fromJson(const juce::var &json) {
    FollowListUser user;
    if (json.isObject()) {
      user.id = json.getProperty("id", "").toString();
      user.username = json.getProperty("username", "").toString();
      user.displayName = json.getProperty("display_name", "").toString();
      user.avatarUrl = json.getProperty("profile_picture_url", "").toString();
      if (user.avatarUrl.isEmpty())
        user.avatarUrl = json.getProperty("avatar_url", "").toString();
      user.bio = json.getProperty("bio", "").toString();
      user.isFollowing = static_cast<bool>(json.getProperty("is_following", false));
      user.followsYou = static_cast<bool>(json.getProperty("follows_you", false));
    }
    return user;
  }

  bool isValid() const {
    return id.isNotEmpty();
  }
};

// ==============================================================================
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

  // Set the app store for image caching
  void setAppStore(Sidechain::Stores::AppStore *store) {
    appStore = store;
  }

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
  Sidechain::Stores::AppStore *appStore = nullptr;

  // Bounds helpers
  juce::Rectangle<int> getAvatarBounds() const;
  juce::Rectangle<int> getFollowButtonBounds() const;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FollowUserRow)
};

// ==============================================================================
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

  // ==============================================================================
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

  // ==============================================================================
  // Callbacks
  std::function<void()> onClose;
  std::function<void(const juce::String &userId)> onUserClicked;

  // ==============================================================================
  // Component overrides
  void paint(juce::Graphics &g) override;
  void resized() override;

  // ==============================================================================
  // Layout constants
  static constexpr int HEADER_HEIGHT = 50;

private:
  // ==============================================================================
  // Timer for auto-refresh
  void timerCallback() override;

  // ==============================================================================
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

  // ==============================================================================
  // UI Components
  std::unique_ptr<juce::Viewport> viewport;
  std::unique_ptr<juce::Component> contentContainer;
  juce::OwnedArray<FollowUserRow> userRows;
  std::unique_ptr<juce::TextButton> closeButton;

  // ==============================================================================
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
