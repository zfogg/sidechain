#pragma once

#include "../network/NetworkClient.h"
#include "../util/logging/Logger.h"
#include "Store.h"
#include <JuceHeader.h>

namespace Sidechain {
namespace Stores {

/**
 * FollowListUser represents a user in a followers/following list
 */
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

/**
 * FollowersState - Immutable state for followers/following lists
 */
struct FollowersState {
  enum class ListType { Followers, Following };

  ListType listType = ListType::Followers;
  juce::String targetUserId;
  juce::Array<FollowListUser> users;
  int totalCount = 0;
  bool isLoading = false;
  bool hasMore = false;
  int currentOffset = 0;
  juce::String errorMessage;
  int64_t lastUpdated = 0;
};

/**
 * FollowersStore - Reactive store for managing followers/following lists
 *
 * Features:
 * - Load followers or following for a user
 * - Pagination support
 * - Track loading state and errors
 * - Optimistic follow/unfollow updates
 *
 * Usage:
 * ```cpp
 * auto followersStore = std::make_shared<FollowersStore>(networkClient);
 * followersStore->subscribe([this](const FollowersState& state) {
 *   updateFollowersList(state.users);
 * });
 * followersStore->loadFollowers(userId);
 * followersStore->toggleFollow(user, true); // Follow user
 * ```
 */
class FollowersStore : public Store<FollowersState> {
public:
  explicit FollowersStore(NetworkClient *client);
  ~FollowersStore() override = default;

  //==============================================================================
  // Data Loading
  void loadFollowers(const juce::String &userId);
  void loadFollowing(const juce::String &userId);
  void refresh();

  //==============================================================================
  // Follow actions
  void toggleFollow(const FollowListUser &user, bool willFollow);
  void loadMoreUsers();

  //==============================================================================
  // Current State Access
  bool isLoading() const {
    return getState().isLoading;
  }

  juce::Array<FollowListUser> getUsers() const {
    return getState().users;
  }

  int getTotalCount() const {
    return getState().totalCount;
  }

  bool hasMoreUsers() const {
    return getState().hasMore;
  }

  FollowersState::ListType getListType() const {
    return getState().listType;
  }

  juce::String getTargetUserId() const {
    return getState().targetUserId;
  }

  juce::String getError() const {
    return getState().errorMessage;
  }

protected:
  //==============================================================================
  // Helper methods
  void updateUsers(const juce::Array<FollowListUser> &users, int totalCount, bool hasMore);
  void addUser(const FollowListUser &user);

private:
  NetworkClient *networkClient;

  //==============================================================================
  // Network callbacks
  void handleUsersLoaded(Outcome<juce::var> result, bool isLoadMore);
  void handleFollowToggled(Outcome<juce::var> result, const FollowListUser &user, bool willFollow);

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FollowersStore)
};

} // namespace Stores
} // namespace Sidechain
