#include "../AppStore.h"
#include "../../util/logging/Logger.h"
#include "../../util/Async.h"
#include "../../util/PropertiesFileUtils.h"
#include "../../util/rx/JuceScheduler.h"
#include "../../models/User.h"
#include "../../network/NetworkClient.h"
#include <rxcpp/rx.hpp>
#include <nlohmann/json.hpp>

namespace Sidechain {
namespace Stores {

// ==============================================================================
// Profile Management

void AppStore::fetchUserProfile(bool forceRefresh) {
  if (!networkClient) {
    Util::logError("AppStore", "Cannot fetch profile - network client not configured");
    return;
  }

  auto authState = stateManager.auth;
  if (!authState->getState().isLoggedIn) {
    Util::logWarning("AppStore", "Cannot fetch profile - not logged in");
    return;
  }

  auto userState = stateManager.user;

  // Check profile age (if not force refreshing)
  if (!forceRefresh) {
    const auto currentState = userState->getState();
    if (currentState.lastProfileUpdate > 0) {
      auto age = juce::Time::getCurrentTime().toMilliseconds() - currentState.lastProfileUpdate;
      if (age < 60000) // Less than 1 minute old
      {
        Util::logDebug("AppStore", "Using cached profile from state");
        return;
      }
    }
  }

  Util::logInfo("AppStore", "Fetching user profile", "forceRefresh=" + juce::String(forceRefresh ? "true" : "false"));

  UserState newState = userState->getState();
  newState.isFetchingProfile = true;
  newState.userError = "";
  userState->setState(newState);

  // Use observable API - now returns typed User
  networkClient->getCurrentUserObservable().subscribe(
      [this](const User &user) {
        Log::debug("fetchUserProfile callback: Success");
        // Update state directly from typed User
        auto userSlice = stateManager.user;
        UserState updatedState = userSlice->getState();
        updatedState.userId = user.id;
        updatedState.username = user.username;
        updatedState.displayName = user.displayName;
        updatedState.bio = user.bio;
        updatedState.profilePictureUrl = user.avatarUrl;
        updatedState.userError = "";
        updatedState.isFetchingProfile = false;
        updatedState.lastProfileUpdate = juce::Time::getCurrentTime().toMilliseconds();
        userSlice->setState(updatedState);

        // Download the profile image if URL is available
        if (user.avatarUrl.isNotEmpty()) {
          downloadProfileImage(user.avatarUrl);
        }
      },
      [this](std::exception_ptr ep) {
        try {
          std::rethrow_exception(ep);
        } catch (const std::exception &e) {
          Log::debug("fetchUserProfile callback: Error - " + juce::String(e.what()));
          handleProfileFetchError(juce::String(e.what()));
        }
      });
}

void AppStore::updateProfile(const juce::String &username, const juce::String &displayName, const juce::String &bio) {
  if (!networkClient) {
    Util::logError("AppStore", "Cannot update profile - network client not configured");
    return;
  }

  Util::logInfo("AppStore", "Updating user profile");

  auto userState = stateManager.user;
  auto previousState = userState->getState();

  // Optimistic update
  UserState optimisticState = userState->getState();
  if (!username.isEmpty())
    optimisticState.username = username;
  if (!displayName.isEmpty())
    optimisticState.displayName = displayName;
  if (!bio.isEmpty())
    optimisticState.bio = bio;
  userState->setState(optimisticState);

  // Use observable API - now returns typed User
  networkClient->updateUserProfileObservable(username, displayName, bio)
      .subscribe([](const User &) { Util::logInfo("AppStore", "Profile updated successfully"); },
                 [userState, previousState](std::exception_ptr ep) {
                   try {
                     std::rethrow_exception(ep);
                   } catch (const std::exception &e) {
                     Util::logError("AppStore", "Failed to update profile: " + juce::String(e.what()));
                     // Revert on error
                     userState->setState(previousState);
                   }
                 });
}

void AppStore::setProfilePictureUrl(const juce::String &url) {
  if (url.isEmpty())
    return;

  Util::logInfo("AppStore", "Setting profile picture URL", "url=" + url);

  auto userState = stateManager.user;
  UserState newState = userState->getState();
  newState.profilePictureUrl = url;
  newState.isLoadingImage = true;
  userState->setState(newState);

  // Download image asynchronously
  downloadProfileImage(url);
}

void AppStore::setLocalPreviewImage(const juce::File &imageFile) {
  if (!imageFile.existsAsFile())
    return;

  Util::logInfo("AppStore", "Setting local preview image", "file=" + imageFile.getFullPathName());

  auto userState = stateManager.user;

  // Load image on background thread
  Async::run<juce::Image>([imageFile]() { return juce::ImageFileFormat::loadFrom(imageFile); },
                          [userState](const juce::Image &image) {
                            if (image.isValid()) {
                              UserState newState = userState->getState();
                              newState.profileImage = image;
                              userState->setState(newState);
                            }
                          });
}

void AppStore::refreshProfileImage() {
  auto userState = stateManager.user;
  const auto currentState = userState->getState();

  if (currentState.profilePictureUrl.isEmpty())
    return;

  Util::logInfo("AppStore", "Refreshing profile image");

  downloadProfileImage(currentState.profilePictureUrl);
}

// ==============================================================================
// User Preferences

void AppStore::setNotificationSoundEnabled(bool enabled) {
  Util::logDebug("AppStore", "Setting notification sound", "enabled=" + juce::String(enabled ? "true" : "false"));

  auto userState = stateManager.user;
  UserState newState = userState->getState();
  newState.notificationSoundEnabled = enabled;
  userState->setState(newState);
}

void AppStore::setOSNotificationsEnabled(bool enabled) {
  Util::logDebug("AppStore", "Setting OS notifications", "enabled=" + juce::String(enabled ? "true" : "false"));

  auto userState = stateManager.user;
  UserState newState = userState->getState();
  newState.osNotificationsEnabled = enabled;
  userState->setState(newState);
}

// ==============================================================================
// Social Metrics

void AppStore::updateFollowerCount(int count) {
  Util::logDebug("AppStore", "Updating follower count", "count=" + juce::String(count));

  auto userState = stateManager.user;
  UserState newState = userState->getState();
  newState.followerCount = count;
  userState->setState(newState);
}

void AppStore::updateFollowingCount(int count) {
  Util::logDebug("AppStore", "Updating following count", "count=" + juce::String(count));

  auto userState = stateManager.user;
  UserState newState = userState->getState();
  newState.followingCount = count;
  userState->setState(newState);
}

void AppStore::updatePostCount(int count) {
  Util::logDebug("AppStore", "Updating post count", "count=" + juce::String(count));

  auto userState = stateManager.user;
  UserState newState = userState->getState();
  newState.postCount = count;
  userState->setState(newState);
}

// ==============================================================================
// Username & Profile Picture Management

void AppStore::changeUsername(const juce::String &newUsername) {
  if (!networkClient) {
    Util::logError("AppStore", "Cannot change username - network client not configured");
    return;
  }

  if (newUsername.isEmpty()) {
    Util::logError("AppStore", "Cannot change username - empty username");
    return;
  }

  Util::logInfo("AppStore", "Changing username to: " + newUsername);

  auto userState = stateManager.user;

  // Use observable API - now returns typed User
  networkClient->changeUsernameObservable(newUsername)
      .subscribe(
          [userState, newUsername](const User &) {
            UserState newState = userState->getState();
            newState.username = newUsername;
            userState->setState(newState);
            Util::logInfo("AppStore", "Username changed successfully");
          },
          [](std::exception_ptr ep) {
            try {
              std::rethrow_exception(ep);
            } catch (const std::exception &e) {
              Util::logError("AppStore", "Failed to change username: " + juce::String(e.what()));
            }
          });
}

// ==============================================================================
// Helper Methods

void AppStore::downloadProfileImage(const juce::String &url) {
  if (url.isEmpty())
    return;

  auto userState = stateManager.user;

  // Try cache first
  if (auto cached = imageCache.getImage(url)) {
    UserState newState = userState->getState();
    newState.profileImage = *cached;
    userState->setState(newState);
    return;
  }

  // Download on background thread
  Async::run<juce::Image>(
      [this, url]() {
        try {
          juce::URL imageUrl(url);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
          auto inputStream =
              imageUrl.createInputStream(false, nullptr, nullptr, "User-Agent: Sidechain/1.0", 5000, nullptr);
#pragma clang diagnostic pop
          if (!inputStream)
            return juce::Image();

          auto image = juce::ImageFileFormat::loadFrom(*inputStream);
          if (image.isValid()) {
            imageCache.cacheImage(url, image);
          }
          return image;
        } catch (...) {
          return juce::Image();
        }
      },
      [userState](const juce::Image &img) {
        UserState newState = userState->getState();
        newState.profileImage = img;
        userState->setState(newState);
      });
}

void AppStore::downloadProfileImage(const juce::String &userId, const juce::String &url) {
  // Single parameter version is used for current user
  if (userId.isEmpty() || url.isEmpty())
    return;
  downloadProfileImage(url);
}

void AppStore::handleProfileFetchSuccess(const juce::var &data) {
  if (!data.isObject())
    return;

  auto userState = stateManager.user;

  UserState newState = userState->getState();
  newState.userId = data.getProperty("id", "").toString();
  newState.username = data.getProperty("username", "").toString();
  newState.displayName = data.getProperty("display_name", "").toString();
  newState.email = data.getProperty("email", "").toString();
  newState.bio = data.getProperty("bio", "").toString();

  // Use avatar_url (backend's effective avatar: S3 if available, else OAuth)
  // This respects the backend's prioritization logic
  juce::String profileUrl = data.getProperty("avatar_url", "").toString();
  if (profileUrl.isEmpty()) {
    // Fallback to explicit S3 URL if avatar_url is somehow missing
    profileUrl = data.getProperty("profile_picture_url", "").toString();
  }
  if (profileUrl.isEmpty()) {
    // Final fallback to OAuth URL
    profileUrl = data.getProperty("oauth_profile_picture_url", "").toString();
  }
  newState.profilePictureUrl = profileUrl;
  newState.userError = "";
  newState.isFetchingProfile = false;
  newState.lastProfileUpdate = juce::Time::getCurrentTime().toMilliseconds();
  userState->setState(newState);

  // Download the profile image if URL is available
  if (profileUrl.isNotEmpty()) {
    downloadProfileImage(profileUrl);
  }
}

void AppStore::handleProfileFetchError(const juce::String &error) {
  auto userState = stateManager.user;

  UserState newState = userState->getState();
  newState.userError = error;
  newState.isFetchingProfile = false;
  userState->setState(newState);
}

void AppStore::followUser(const juce::String &userId) {
  if (!networkClient || userId.isEmpty())
    return;

  auto userState = stateManager.user;

  // Optimistic update
  UserState optimisticState = userState->getState();
  optimisticState.followingCount++;
  userState->setState(optimisticState);

  // Use observable API
  networkClient->followUserObservable(userId).subscribe(
      [userId](const NetworkClient::FollowResult &) {
        Util::logInfo("AppStore", "Successfully followed user: " + userId);
      },
      [userState](std::exception_ptr ep) {
        // Rollback optimistic update
        UserState rollbackState = userState->getState();
        if (rollbackState.followingCount > 0)
          rollbackState.followingCount--;
        userState->setState(rollbackState);

        try {
          std::rethrow_exception(ep);
        } catch (const std::exception &e) {
          Util::logError("AppStore", "Failed to follow user: " + juce::String(e.what()));
        }
      });
}

void AppStore::unfollowUser(const juce::String &userId) {
  if (!networkClient || userId.isEmpty())
    return;

  auto userState = stateManager.user;

  // Optimistic update
  UserState optimisticState = userState->getState();
  if (optimisticState.followingCount > 0)
    optimisticState.followingCount--;
  userState->setState(optimisticState);

  // Use observable API
  networkClient->unfollowUserObservable(userId).subscribe(
      [userId](int) { Util::logInfo("AppStore", "Successfully unfollowed user: " + userId); },
      [userState](std::exception_ptr ep) {
        // Rollback optimistic update
        UserState rollbackState = userState->getState();
        rollbackState.followingCount++;
        userState->setState(rollbackState);

        try {
          std::rethrow_exception(ep);
        } catch (const std::exception &e) {
          Util::logError("AppStore", "Failed to unfollow user: " + juce::String(e.what()));
        }
      });
}

void AppStore::updateProfileComplete(const juce::String &username, const juce::String &displayName,
                                     const juce::String &bio, const juce::String &location, const juce::String &genre,
                                     const juce::var &socialLinks, bool isPrivate, const juce::String &dawPreference) {
  auto userState = stateManager.user;

  UserState newState = userState->getState();
  newState.username = username;
  newState.displayName = displayName;
  newState.bio = bio;
  newState.location = location;
  newState.genre = genre;
  newState.socialLinks = socialLinks;
  newState.isPrivate = isPrivate;
  newState.dawPreference = dawPreference;
  userState->setState(newState);
}

void AppStore::uploadProfilePicture(const juce::File &file) {
  if (!networkClient)
    return;

  // Use observable API
  networkClient->uploadProfilePictureObservable(file).subscribe(
      [this](const juce::String &url) { setProfilePictureUrl(url); },
      [](std::exception_ptr ep) {
        try {
          std::rethrow_exception(ep);
        } catch (const std::exception &e) {
          Util::logError("AppStore", "Failed to upload profile picture: " + juce::String(e.what()));
        }
      });
}

// ==============================================================================
// Discovery Methods

void AppStore::loadTrendingUsers() {
  if (!networkClient) {
    Util::logError("AppStore", "Cannot load trending users - network client not configured");
    return;
  }

  Util::logInfo("AppStore", "Loading trending users");

  auto searchState = stateManager.search;
  SearchState newState = searchState->getState();
  newState.results.isSearching = true;
  newState.results.searchError = "";
  searchState->setState(newState);

  networkClient->getTrendingUsers(20, [this](Outcome<juce::var> result) {
    juce::MessageManager::callAsync([this, result]() {
      if (result.isOk()) {
        handleTrendingUsersSuccess(result.getValue());
      } else {
        handleTrendingUsersError(result.getError());
      }
    });
  });
}

void AppStore::loadFeaturedProducers() {
  if (!networkClient) {
    Util::logError("AppStore", "Cannot load featured producers - network client not configured");
    return;
  }

  Util::logInfo("AppStore", "Loading featured producers");

  auto searchState = stateManager.search;
  SearchState newState = searchState->getState();
  newState.results.isSearching = true;
  newState.results.searchError = "";
  searchState->setState(newState);

  networkClient->getFeaturedProducers(20, [this](Outcome<juce::var> result) {
    juce::MessageManager::callAsync([this, result]() {
      if (result.isOk()) {
        handleFeaturedProducersSuccess(result.getValue());
      } else {
        handleFeaturedProducersError(result.getError());
      }
    });
  });
}

void AppStore::loadSuggestedUsers() {
  if (!networkClient) {
    Util::logError("AppStore", "Cannot load suggested users - network client not configured");
    return;
  }

  Util::logInfo("AppStore", "Loading suggested users");

  auto searchState = stateManager.search;
  SearchState newState = searchState->getState();
  newState.results.isSearching = true;
  newState.results.searchError = "";
  searchState->setState(newState);

  networkClient->getSuggestedUsers(20, [this](Outcome<juce::var> result) {
    juce::MessageManager::callAsync([this, result]() {
      if (result.isOk()) {
        handleSuggestedUsersSuccess(result.getValue());
      } else {
        handleSuggestedUsersError(result.getError());
      }
    });
  });
}

// ==============================================================================
// Discovery Success Handlers

void AppStore::handleTrendingUsersSuccess(const juce::var &data) {
  if (!data.isArray())
    return;

  auto &entityStore = EntityStore::getInstance();
  std::vector<std::shared_ptr<Sidechain::User>> users;

  for (int i = 0; i < data.size(); ++i) {
    try {
      // Convert juce::var to nlohmann::json
      auto jsonStr = data[i].toString().toStdString();
      auto json = nlohmann::json::parse(jsonStr);

      // Normalize user (creates/updates shared_ptr in EntityStore)
      auto normalized = entityStore.normalizeUser(json);
      if (normalized) {
        users.push_back(normalized);
      }
    } catch (const std::exception &e) {
      Util::logError("AppStore", "Failed to parse trending user JSON: " + juce::String(e.what()));
    }
  }

  // Update search state with users
  auto searchState = stateManager.search;
  SearchState newState = searchState->getState();
  newState.results.users = users;
  newState.results.isSearching = false;
  newState.results.searchError = "";
  searchState->setState(newState);

  Util::logInfo("AppStore", "Loaded " + juce::String(users.size()) + " trending users");
}

void AppStore::handleFeaturedProducersSuccess(const juce::var &data) {
  if (!data.isArray())
    return;

  auto &entityStore = EntityStore::getInstance();
  std::vector<std::shared_ptr<Sidechain::User>> users;

  for (int i = 0; i < data.size(); ++i) {
    try {
      // Convert juce::var to nlohmann::json
      auto jsonStr = data[i].toString().toStdString();
      auto json = nlohmann::json::parse(jsonStr);

      // Normalize user (creates/updates shared_ptr in EntityStore)
      auto normalized = entityStore.normalizeUser(json);
      if (normalized) {
        users.push_back(normalized);
      }
    } catch (const std::exception &e) {
      Util::logError("AppStore", "Failed to parse featured producer JSON: " + juce::String(e.what()));
    }
  }

  // Update search state with users
  auto searchState = stateManager.search;
  SearchState newState = searchState->getState();
  newState.results.users = users;
  newState.results.isSearching = false;
  newState.results.searchError = "";
  searchState->setState(newState);

  Util::logInfo("AppStore", "Loaded " + juce::String(users.size()) + " featured producers");
}

void AppStore::handleSuggestedUsersSuccess(const juce::var &data) {
  if (!data.isArray())
    return;

  auto &entityStore = EntityStore::getInstance();
  std::vector<std::shared_ptr<Sidechain::User>> users;

  for (int i = 0; i < data.size(); ++i) {
    try {
      // Convert juce::var to nlohmann::json
      auto jsonStr = data[i].toString().toStdString();
      auto json = nlohmann::json::parse(jsonStr);

      // Normalize user (creates/updates shared_ptr in EntityStore)
      auto normalized = entityStore.normalizeUser(json);
      if (normalized) {
        users.push_back(normalized);
      }
    } catch (const std::exception &e) {
      Util::logError("AppStore", "Failed to parse suggested user JSON: " + juce::String(e.what()));
    }
  }

  // Update search state with users
  auto searchState = stateManager.search;
  SearchState newState = searchState->getState();
  newState.results.users = users;
  newState.results.isSearching = false;
  newState.results.searchError = "";
  searchState->setState(newState);

  Util::logInfo("AppStore", "Loaded " + juce::String(users.size()) + " suggested users");
}

// ==============================================================================
// Discovery Error Handlers

void AppStore::handleTrendingUsersError(const juce::String &error) {
  Util::logError("AppStore", "Failed to load trending users: " + error);

  auto searchState = stateManager.search;
  SearchState newState = searchState->getState();
  newState.results.isSearching = false;
  newState.results.searchError = error;
  searchState->setState(newState);
}

void AppStore::handleFeaturedProducersError(const juce::String &error) {
  Util::logError("AppStore", "Failed to load featured producers: " + error);

  auto searchState = stateManager.search;
  SearchState newState = searchState->getState();
  newState.results.isSearching = false;
  newState.results.searchError = error;
  searchState->setState(newState);
}

void AppStore::handleSuggestedUsersError(const juce::String &error) {
  Util::logError("AppStore", "Failed to load suggested users: " + error);

  auto searchState = stateManager.search;
  SearchState newState = searchState->getState();
  newState.results.isSearching = false;
  newState.results.searchError = error;
  searchState->setState(newState);
}

// ==============================================================================
// User Model Subscriptions (Redux Pattern)

std::function<void()> AppStore::subscribeToUser(const juce::String &userId,
                                                std::function<void(const std::shared_ptr<User> &)> callback) {
  if (!callback) {
    Util::logError("AppStore", "Cannot subscribe to user - callback is null");
    return []() {};
  }

  auto &entityStore = EntityStore::getInstance();
  return entityStore.users().subscribe(userId, callback);
}

void AppStore::loadUser(const juce::String &userId, bool forceRefresh) {
  if (userId.isEmpty()) {
    Util::logError("AppStore", "Cannot load user - userId is empty");
    return;
  }

  if (!networkClient) {
    Util::logError("AppStore", "Cannot load user - network client not configured");
    return;
  }

  auto &entityStore = EntityStore::getInstance();

  // Check cache first (unless force refresh)
  if (!forceRefresh) {
    auto cached = entityStore.users().get(userId);
    if (cached) {
      Util::logInfo("AppStore", "User " + userId + " already cached");
      return;
    }
  }

  Util::logInfo("AppStore", "Loading user: " + userId);

  // Use observable API - now returns typed User
  networkClient->getUserObservable(userId).subscribe(
      [userId](const User &user) {
        // Store typed user in EntityStore
        auto &entityStore = EntityStore::getInstance();
        auto sharedUser = std::make_shared<User>(user);
        entityStore.users().set(userId, sharedUser);
        Util::logInfo("AppStore", "Loaded user: " + userId);
      },
      [userId](std::exception_ptr ep) {
        try {
          std::rethrow_exception(ep);
        } catch (const std::exception &e) {
          Util::logError("AppStore", "Failed to load user " + userId + ": " + juce::String(e.what()));
        }
      });
}

void AppStore::loadUserPosts(const juce::String &userId, int limit, int offset) {
  if (userId.isEmpty()) {
    Util::logError("AppStore", "Cannot load user posts - userId is empty");
    return;
  }

  if (!networkClient) {
    Util::logError("AppStore", "Cannot load user posts - network client not configured");
    return;
  }

  Util::logInfo("AppStore", "Loading posts for user: " + userId + " (limit=" + juce::String(limit) +
                                ", offset=" + juce::String(offset) + ")");

  // Use observable API - now returns typed FeedResult
  networkClient->getUserPostsObservable(userId, limit, offset)
      .subscribe(
          [userId](const NetworkClient::FeedResult &feedResult) {
            // Posts are already typed, store them in EntityStore
            auto &entityStore = EntityStore::getInstance();
            for (const auto &post : feedResult.posts) {
              auto sharedPost = std::make_shared<FeedPost>(post);
              entityStore.posts().set(post.id, sharedPost);
            }
            Util::logInfo("AppStore", "Loaded " + juce::String(static_cast<int>(feedResult.posts.size())) +
                                          " posts for user: " + userId);
          },
          [userId](std::exception_ptr ep) {
            try {
              std::rethrow_exception(ep);
            } catch (const std::exception &e) {
              Util::logError("AppStore", "Failed to load posts for user " + userId + ": " + juce::String(e.what()));
            }
          });
}

void AppStore::loadFollowers(const juce::String &userId, int limit, int offset) {
  if (userId.isEmpty()) {
    Util::logError("AppStore", "Cannot load followers - userId is empty");
    return;
  }

  if (!networkClient) {
    Util::logError("AppStore", "Cannot load followers - network client not configured");
    return;
  }

  Util::logInfo("AppStore", "Loading followers for user: " + userId);

  // Redux Action: Set loading state (immutable state instance)
  stateManager.followers->setState(FollowersState({},                   // empty users vector
                                                  true,                 // isLoading
                                                  "",                   // no error yet
                                                  0,                    // totalCount
                                                  userId.toStdString(), // targetUserId
                                                  FollowersState::Followers));

  // Use observable API - now returns typed UserResult
  networkClient->getFollowersObservable(userId, limit, offset)
      .subscribe(
          [this](const NetworkClient::UserResult &userResult) {
            // Convert typed users to shared_ptr and store in EntityStore
            std::vector<std::shared_ptr<Sidechain::User>> users;
            auto &entityStore = EntityStore::getInstance();
            for (const auto &user : userResult.users) {
              auto sharedUser = std::make_shared<User>(user);
              entityStore.users().set(user.id, sharedUser);
              users.push_back(sharedUser);
            }

            // Reducer - Create new immutable FollowersState with loaded users
            std::vector<std::shared_ptr<const Sidechain::User>> immutableUsers;
            for (const auto &user : users) {
              immutableUsers.push_back(std::const_pointer_cast<const Sidechain::User>(user));
            }
            auto currentState = stateManager.followers->getState();
            stateManager.followers->setState(FollowersState(immutableUsers,
                                                            false,            // isLoading
                                                            "",               // no error
                                                            userResult.total, // totalCount from response
                                                            currentState.targetUserId, currentState.listType));

            Util::logInfo("AppStore", "Loaded " + juce::String(static_cast<int>(users.size())) + " followers");
          },
          [this](std::exception_ptr ep) {
            try {
              std::rethrow_exception(ep);
            } catch (const std::exception &e) {
              // Reducer: Create immutable error state
              auto error = std::string(e.what());
              auto currentState = stateManager.followers->getState();
              stateManager.followers->setState(FollowersState(currentState.users,
                                                              false, // isLoading
                                                              error, // errorMessage
                                                              currentState.totalCount, currentState.targetUserId,
                                                              currentState.listType));
              Util::logError("AppStore", "Failed to load followers: " + juce::String(e.what()));
            }
          });
}

void AppStore::loadFollowing(const juce::String &userId, int limit, int offset) {
  if (userId.isEmpty()) {
    Util::logError("AppStore", "Cannot load following - userId is empty");
    return;
  }

  if (!networkClient) {
    Util::logError("AppStore", "Cannot load following - network client not configured");
    return;
  }

  Util::logInfo("AppStore", "Loading following for user: " + userId);

  // Redux Action: Set loading state (immutable state instance)
  stateManager.followers->setState(FollowersState({},                   // empty users vector
                                                  true,                 // isLoading
                                                  "",                   // no error yet
                                                  0,                    // totalCount
                                                  userId.toStdString(), // targetUserId
                                                  FollowersState::Following));

  // Use observable API - now returns typed UserResult
  networkClient->getFollowingObservable(userId, limit, offset)
      .subscribe(
          [this](const NetworkClient::UserResult &userResult) {
            // Convert typed users to shared_ptr and store in EntityStore
            std::vector<std::shared_ptr<Sidechain::User>> users;
            auto &entityStore = EntityStore::getInstance();
            for (const auto &user : userResult.users) {
              auto sharedUser = std::make_shared<User>(user);
              entityStore.users().set(user.id, sharedUser);
              users.push_back(sharedUser);
            }

            // Reducer - Create new immutable FollowersState with loaded users
            std::vector<std::shared_ptr<const Sidechain::User>> immutableUsers;
            for (const auto &user : users) {
              immutableUsers.push_back(std::const_pointer_cast<const Sidechain::User>(user));
            }
            auto currentState = stateManager.followers->getState();
            stateManager.followers->setState(FollowersState(immutableUsers,
                                                            false,            // isLoading
                                                            "",               // no error
                                                            userResult.total, // totalCount from response
                                                            currentState.targetUserId, currentState.listType));

            Util::logInfo("AppStore", "Loaded " + juce::String(static_cast<int>(users.size())) + " following");
          },
          [this](std::exception_ptr ep) {
            try {
              std::rethrow_exception(ep);
            } catch (const std::exception &e) {
              // Reducer: Create error state
              auto error = std::string(e.what());
              auto currentState = stateManager.followers->getState();
              stateManager.followers->setState(FollowersState(currentState.users,
                                                              false, // isLoading
                                                              error, // errorMessage
                                                              currentState.totalCount, currentState.targetUserId,
                                                              currentState.listType));
              Util::logError("AppStore", "Failed to load following: " + juce::String(e.what()));
            }
          });
}

// Old handler methods removed - now using Redux pattern in loadFollowers/loadFollowing

// ==============================================================================
// Reactive User/Profile Observables (Phase 7)
// ==============================================================================

rxcpp::observable<User> AppStore::fetchUserProfileObservable(bool forceRefresh) {
  return rxcpp::sources::create<User>([this, forceRefresh](auto observer) {
           if (!networkClient) {
             Util::logError("AppStore", "Network client not configured");
             observer.on_error(std::make_exception_ptr(std::runtime_error("Network client not configured")));
             return;
           }

           auto authState = stateManager.auth;
           if (!authState->getState().isLoggedIn) {
             observer.on_error(std::make_exception_ptr(std::runtime_error("Not logged in")));
             return;
           }

           // Check cache if not force refreshing
           if (!forceRefresh) {
             const auto currentState = stateManager.user->getState();
             if (currentState.lastProfileUpdate > 0) {
               auto age = juce::Time::getCurrentTime().toMilliseconds() - currentState.lastProfileUpdate;
               if (age < 60000) {
                 // Return cached data as User
                 User user;
                 user.id = currentState.userId;
                 user.username = currentState.username;
                 user.displayName = currentState.displayName;
                 user.bio = currentState.bio;
                 user.avatarUrl = currentState.profilePictureUrl;
                 user.followerCount = currentState.followerCount;
                 user.followingCount = currentState.followingCount;
                 observer.on_next(std::move(user));
                 observer.on_completed();
                 return;
               }
             }
           }

           Util::logDebug("AppStore", "Fetching user profile via observable");

           networkClient->getCurrentUser([this, observer](Outcome<juce::var> result) {
             if (result.isOk()) {
               const auto &data = result.getValue();
               if (!data.isObject()) {
                 observer.on_error(std::make_exception_ptr(std::runtime_error("Invalid profile data")));
                 return;
               }

               User user;
               user.id = data.getProperty("id", "").toString();
               user.username = data.getProperty("username", "").toString();
               user.displayName = data.getProperty("display_name", "").toString();
               user.bio = data.getProperty("bio", "").toString();

               // Get profile picture URL
               juce::String profileUrl = data.getProperty("avatar_url", "").toString();
               if (profileUrl.isEmpty()) {
                 profileUrl = data.getProperty("profile_picture_url", "").toString();
               }
               if (profileUrl.isEmpty()) {
                 profileUrl = data.getProperty("oauth_profile_picture_url", "").toString();
               }
               user.avatarUrl = profileUrl;

               // Update slice state
               UserState newState = stateManager.user->getState();
               newState.userId = user.id;
               newState.username = user.username;
               newState.displayName = user.displayName;
               newState.email = data.getProperty("email", "").toString();
               newState.bio = user.bio;
               newState.profilePictureUrl = profileUrl;
               newState.userError = "";
               newState.isFetchingProfile = false;
               newState.lastProfileUpdate = juce::Time::getCurrentTime().toMilliseconds();
               stateManager.user->setState(newState);

               Util::logInfo("AppStore", "Profile fetched for: " + user.username);
               observer.on_next(std::move(user));
               observer.on_completed();
             } else {
               Util::logError("AppStore", "Failed to fetch profile: " + result.getError());
               observer.on_error(std::make_exception_ptr(std::runtime_error(result.getError().toStdString())));
             }
           });
         })
      .observe_on(Rx::observe_on_juce_thread());
}

rxcpp::observable<int> AppStore::updateProfileObservable(const juce::String &username, const juce::String &displayName,
                                                         const juce::String &bio) {
  return rxcpp::sources::create<int>([this, username, displayName, bio](auto observer) {
           if (!networkClient) {
             Util::logError("AppStore", "Network client not configured");
             observer.on_error(std::make_exception_ptr(std::runtime_error("Network client not configured")));
             return;
           }

           Util::logDebug("AppStore", "Updating profile via observable");

           auto userState = stateManager.user;
           auto previousState = userState->getState();

           // Optimistic update
           UserState optimisticState = userState->getState();
           if (!username.isEmpty())
             optimisticState.username = username;
           if (!displayName.isEmpty())
             optimisticState.displayName = displayName;
           if (!bio.isEmpty())
             optimisticState.bio = bio;
           userState->setState(optimisticState);

           networkClient->updateUserProfile(
               username, displayName, bio, [this, observer, previousState](Outcome<juce::var> result) {
                 if (result.isOk()) {
                   Util::logInfo("AppStore", "Profile updated successfully");
                   observer.on_next(0);
                   observer.on_completed();
                 } else {
                   // Revert optimistic update
                   stateManager.user->setState(previousState);
                   Util::logError("AppStore", "Failed to update profile: " + result.getError());
                   observer.on_error(std::make_exception_ptr(std::runtime_error(result.getError().toStdString())));
                 }
               });
         })
      .observe_on(Rx::observe_on_juce_thread());
}

rxcpp::observable<int> AppStore::changeUsernameObservable(const juce::String &newUsername) {
  return rxcpp::sources::create<int>([this, newUsername](auto observer) {
           if (!networkClient) {
             Util::logError("AppStore", "Network client not configured");
             observer.on_error(std::make_exception_ptr(std::runtime_error("Network client not configured")));
             return;
           }

           if (newUsername.isEmpty()) {
             observer.on_error(std::make_exception_ptr(std::runtime_error("Username cannot be empty")));
             return;
           }

           Util::logDebug("AppStore", "Changing username via observable to: " + newUsername);

           networkClient->changeUsername(newUsername, [this, observer, newUsername](Outcome<juce::var> result) {
             if (result.isOk()) {
               UserState newState = stateManager.user->getState();
               newState.username = newUsername;
               stateManager.user->setState(newState);

               Util::logInfo("AppStore", "Username changed successfully");
               observer.on_next(0);
               observer.on_completed();
             } else {
               Util::logError("AppStore", "Failed to change username: " + result.getError());
               observer.on_error(std::make_exception_ptr(std::runtime_error(result.getError().toStdString())));
             }
           });
         })
      .observe_on(Rx::observe_on_juce_thread());
}

rxcpp::observable<juce::String> AppStore::uploadProfilePictureObservable(const juce::File &imageFile) {
  return rxcpp::sources::create<juce::String>([this, imageFile](auto observer) {
           if (!networkClient) {
             Util::logError("AppStore", "Network client not configured");
             observer.on_error(std::make_exception_ptr(std::runtime_error("Network client not configured")));
             return;
           }

           if (!imageFile.existsAsFile()) {
             observer.on_error(std::make_exception_ptr(std::runtime_error("Image file does not exist")));
             return;
           }

           Util::logDebug("AppStore", "Uploading profile picture via observable");

           networkClient->uploadProfilePicture(imageFile, [this, observer](Outcome<juce::String> result) {
             if (result.isOk()) {
               const auto &url = result.getValue();

               UserState newState = stateManager.user->getState();
               newState.profilePictureUrl = url;
               newState.isLoadingImage = true;
               stateManager.user->setState(newState);

               // Download and cache the new image
               downloadProfileImage(url);

               Util::logInfo("AppStore", "Profile picture uploaded: " + url);
               observer.on_next(url);
               observer.on_completed();
             } else {
               Util::logError("AppStore", "Failed to upload profile picture: " + result.getError());
               observer.on_error(std::make_exception_ptr(std::runtime_error(result.getError().toStdString())));
             }
           });
         })
      .observe_on(Rx::observe_on_juce_thread());
}

} // namespace Stores
} // namespace Sidechain
