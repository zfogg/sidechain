#pragma once

#include "../../util/Colors.h"
#include "../../util/HoverState.h"
#include "../../util/Time.h"
#include "../../models/User.h"
#include "../../stores/app/AppState.h"
#include "../../stores/slices/AppSlices.h"
#include "../common/AppStoreComponent.h"
#include <JuceHeader.h>
#include <memory>

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
 * Works with immutable shared_ptr<const User> from EntityStore
 */
class FollowUserRow : public juce::Component {
public:
  FollowUserRow();
  ~FollowUserRow() override = default;

  // Set user from immutable shared_ptr<const User>
  void setUser(const std::shared_ptr<const Sidechain::User> &user);
  const std::shared_ptr<const Sidechain::User> &getUser() const {
    return userPtr;
  }

  // Set the app store for image caching
  void setAppStore(Sidechain::Stores::AppStore *store) {
    appStore = store;
  }

  // Callbacks - dispatch actions through AppStore
  std::function<void(const juce::String &userId)> onUserClicked;
  std::function<void(const juce::String &userId, bool willFollow)> onFollowToggled;

  // Component overrides
  void paint(juce::Graphics &g) override;
  void resized() override;
  void mouseUp(const juce::MouseEvent &event) override;
  void mouseEnter(const juce::MouseEvent &event) override;
  void mouseExit(const juce::MouseEvent &event) override;

  static constexpr int ROW_HEIGHT = 70;

private:
  std::shared_ptr<const Sidechain::User> userPtr;
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
 *
 * Architecture:
 * - Extends AppStoreComponent<FollowersState>
 * - Subscribes to FollowersState for reactive updates
 * - Renders immutable user list from state
 */
class FollowersList : public Sidechain::UI::AppStoreComponent<Sidechain::Stores::FollowersState>, private juce::Timer {
public:
  enum class ListType { Followers, Following };

  explicit FollowersList(Sidechain::Stores::AppStore *store = nullptr);
  ~FollowersList() override;

  // ==============================================================================
  // Setup
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
  // AppStoreComponent<FollowersState> implementation
protected:
  void subscribeToAppStore() override;
  void onAppStateChanged(const Sidechain::Stores::FollowersState &state) override;

  // ==============================================================================
  // Layout constants
  static constexpr int HEADER_HEIGHT = 50;

private:
  // ==============================================================================
  // Timer for auto-refresh
  void timerCallback() override;

  // ==============================================================================
  // State Cache (updated in onAppStateChanged)
  // Stored for access in paint(), updateUsersList(), refresh(), etc.
  Sidechain::Stores::FollowersState currentState;

  // Context
  juce::String currentUserId; // Currently logged in user

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
  void onSliceChanged(const Sidechain::Stores::FollowersState &state);

  // Row callbacks
  void setupRowCallbacks(FollowUserRow *row);

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FollowersList)
};
