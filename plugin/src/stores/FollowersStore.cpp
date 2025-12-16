#include "FollowersStore.h"
#include "../util/Json.h"
#include "../util/Log.h"

namespace Sidechain {
namespace Stores {

//==============================================================================
FollowersStore::FollowersStore(NetworkClient *client) : networkClient(client) {
  Log::info("FollowersStore: Initializing");
  setState(FollowersState{});
}

//==============================================================================
void FollowersStore::loadFollowers(const juce::String &userId) {
  if (networkClient == nullptr || userId.isEmpty())
    return;

  auto state = getState();
  state.listType = FollowersState::ListType::Followers;
  state.targetUserId = userId;
  state.isLoading = true;
  state.currentOffset = 0;
  state.users.clear();
  setState(state);

  Log::info("FollowersStore: Loading followers for user: " + userId);

  networkClient->getFollowers(userId, 20, 0, [this](Outcome<juce::var> result) { handleUsersLoaded(result, false); });
}

//==============================================================================
void FollowersStore::loadFollowing(const juce::String &userId) {
  if (networkClient == nullptr || userId.isEmpty())
    return;

  auto state = getState();
  state.listType = FollowersState::ListType::Following;
  state.targetUserId = userId;
  state.isLoading = true;
  state.currentOffset = 0;
  state.users.clear();
  setState(state);

  Log::info("FollowersStore: Loading following for user: " + userId);

  networkClient->getFollowing(userId, 20, 0, [this](Outcome<juce::var> result) { handleUsersLoaded(result, false); });
}

//==============================================================================
void FollowersStore::refresh() {
  auto state = getState();
  if (state.targetUserId.isEmpty())
    return;

  if (state.listType == FollowersState::ListType::Followers)
    loadFollowers(state.targetUserId);
  else
    loadFollowing(state.targetUserId);
}

//==============================================================================
void FollowersStore::loadMoreUsers() {
  auto state = getState();
  if (!state.hasMore || state.targetUserId.isEmpty())
    return;

  Log::info("FollowersStore: Loading more users");

  if (state.listType == FollowersState::ListType::Followers)
    networkClient->getFollowers(state.targetUserId, 20, state.currentOffset,
                                [this](Outcome<juce::var> result) { handleUsersLoaded(result, true); });
  else
    networkClient->getFollowing(state.targetUserId, 20, state.currentOffset,
                                [this](Outcome<juce::var> result) { handleUsersLoaded(result, true); });
}

//==============================================================================
void FollowersStore::handleUsersLoaded(Outcome<juce::var> result, bool isLoadMore) {
  auto state = getState();
  state.isLoading = false;

  if (result.isError()) {
    Log::error("FollowersStore: Failed to load users - " + result.getError());
    state.errorMessage = "Failed to load users";
    setState(state);
    return;
  }

  auto response = result.getValue();
  juce::Array<FollowListUser> users;

  // Parse users array
  juce::String usersKey = (state.listType == FollowersState::ListType::Followers) ? "followers" : "following";
  if (response.hasProperty(usersKey.toRawUTF8())) {
    auto usersArray = response[usersKey.toRawUTF8()];
    if (usersArray.isArray()) {
      for (int i = 0; i < usersArray.size(); ++i) {
        users.add(FollowListUser::fromJson(usersArray[i]));
      }
    }
  }

  // Update pagination info
  if (response.hasProperty("total_count"))
    state.totalCount = Json::getInt(response, "total_count");

  if (response.hasProperty("has_more"))
    state.hasMore = Json::getBool(response, "has_more", false);

  // Append to existing users if loading more
  if (isLoadMore) {
    for (const auto &user : users) {
      state.users.add(user);
    }
    state.currentOffset += users.size();
  } else {
    state.users = users;
    state.currentOffset = users.size();
  }

  state.errorMessage = "";
  state.lastUpdated = juce::Time::getCurrentTime().toMilliseconds();
  setState(state);

  Log::info("FollowersStore: Loaded " + juce::String(users.size()) + " users");
}

//==============================================================================
void FollowersStore::toggleFollow(const FollowListUser &user, bool willFollow) {
  // Optimistic update - update UI immediately
  auto state = getState();
  juce::Array<FollowListUser> updatedUsers = state.users;
  for (int i = 0; i < updatedUsers.size(); ++i) {
    if (updatedUsers[i].id == user.id) {
      FollowListUser updatedUser = updatedUsers[i];
      updatedUser.isFollowing = willFollow;
      updatedUsers.set(i, updatedUser);
      break;
    }
  }
  state.users = updatedUsers;
  setState(state);

  // Make network request
  if (willFollow) {
    networkClient->followUser(user.id,
                              [this, user](Outcome<juce::var> result) { handleFollowToggled(result, user, true); });
  } else {
    networkClient->unfollowUser(user.id,
                                [this, user](Outcome<juce::var> result) { handleFollowToggled(result, user, false); });
  }
}

//==============================================================================
void FollowersStore::handleFollowToggled(Outcome<juce::var> result, const FollowListUser &user, bool willFollow) {
  if (result.isError()) {
    Log::error("FollowersStore: Failed to toggle follow - " + result.getError());

    // Revert optimistic update
    auto state = getState();
    juce::Array<FollowListUser> updatedUsers = state.users;
    for (int i = 0; i < updatedUsers.size(); ++i) {
      if (updatedUsers[i].id == user.id) {
        FollowListUser updatedUser = updatedUsers[i];
        updatedUser.isFollowing = !willFollow;
        updatedUsers.set(i, updatedUser);
        state.errorMessage = "Failed to update follow status";
        break;
      }
    }
    state.users = updatedUsers;
    setState(state);
  }
}

//==============================================================================
void FollowersStore::updateUsers(const juce::Array<FollowListUser> &users, int totalCount, bool hasMore) {
  auto state = getState();
  state.users = users;
  state.totalCount = totalCount;
  state.hasMore = hasMore;
  state.errorMessage = "";
  state.lastUpdated = juce::Time::getCurrentTime().toMilliseconds();
  setState(state);
}

//==============================================================================
void FollowersStore::addUser(const FollowListUser &user) {
  auto state = getState();
  state.users.add(user);
  setState(state);
}

} // namespace Stores
} // namespace Sidechain
