#include "../AppStore.h"
#include "../../util/logging/Logger.h"
#include "../../util/Async.h"
#include "../../util/PropertiesFileUtils.h"

namespace Sidechain {
namespace Stores {

// ==============================================================================
// Profile Management

void AppStore::fetchUserProfile(bool forceRefresh) {
  if (!networkClient) {
    Util::logError("AppStore", "Cannot fetch profile - network client not configured");
    return;
  }

  auto authSlice = sliceManager.getAuthSlice();
  if (!authSlice->getState().isLoggedIn) {
    Util::logWarning("AppStore", "Cannot fetch profile - not logged in");
    return;
  }

  auto userSlice = sliceManager.getUserSlice();

  // Check profile age (if not force refreshing)
  if (!forceRefresh) {
    const auto currentState = userSlice->getState();
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

  userSlice->dispatch([](UserState &state) {
    state.isFetchingProfile = true;
    state.userError = "";
  });

  networkClient->getCurrentUser([this](Outcome<juce::var> result) {
    Log::debug("fetchUserProfile callback: result.isOk=" + juce::String(result.isOk() ? "true" : "false"));
    if (result.isOk()) {
      Log::debug("fetchUserProfile callback: Success");
      handleProfileFetchSuccess(result.getValue());
    } else {
      Log::debug("fetchUserProfile callback: Error - " + result.getError());
      handleProfileFetchError(result.getError());
    }
  });
}

void AppStore::updateProfile(const juce::String &username, const juce::String &displayName, const juce::String &bio) {
  if (!networkClient) {
    Util::logError("AppStore", "Cannot update profile - network client not configured");
    return;
  }

  Util::logInfo("AppStore", "Updating user profile");

  auto userSlice = sliceManager.getUserSlice();
  auto previousState = userSlice->getState();

  // Optimistic update
  userSlice->dispatch([username, displayName, bio](UserState &state) {
    if (!username.isEmpty())
      state.username = username;
    if (!displayName.isEmpty())
      state.displayName = displayName;
    if (!bio.isEmpty())
      state.bio = bio;
  });

  networkClient->updateUserProfile(username, displayName, bio, [userSlice, previousState](Outcome<juce::var> result) {
    juce::MessageManager::callAsync([userSlice, previousState, result]() {
      if (!result.isOk()) {
        Util::logError("AppStore", "Failed to update profile: " + result.getError());
        // Revert on error
        userSlice->dispatch([previousState](UserState &state) { state = previousState; });
      }
    });
  });
}

void AppStore::setProfilePictureUrl(const juce::String &url) {
  if (url.isEmpty())
    return;

  Util::logInfo("AppStore", "Setting profile picture URL", "url=" + url);

  auto userSlice = sliceManager.getUserSlice();
  userSlice->dispatch([url](UserState &state) {
    state.profilePictureUrl = url;
    state.isLoadingImage = true;
  });

  // Download image asynchronously
  downloadProfileImage(url);
}

void AppStore::setLocalPreviewImage(const juce::File &imageFile) {
  if (!imageFile.existsAsFile())
    return;

  Util::logInfo("AppStore", "Setting local preview image", "file=" + imageFile.getFullPathName());

  auto userSlice = sliceManager.getUserSlice();

  // Load image on background thread
  Async::run<juce::Image>([imageFile]() { return juce::ImageFileFormat::loadFrom(imageFile); },
                          [userSlice](const juce::Image &image) {
                            if (image.isValid()) {
                              userSlice->dispatch([image](UserState &state) { state.profileImage = image; });
                            }
                          });
}

void AppStore::refreshProfileImage() {
  auto userSlice = sliceManager.getUserSlice();
  const auto currentState = userSlice->getState();

  if (currentState.profilePictureUrl.isEmpty())
    return;

  Util::logInfo("AppStore", "Refreshing profile image");

  downloadProfileImage(currentState.profilePictureUrl);
}

// ==============================================================================
// User Preferences

void AppStore::setNotificationSoundEnabled(bool enabled) {
  Util::logDebug("AppStore", "Setting notification sound", "enabled=" + juce::String(enabled ? "true" : "false"));

  auto userSlice = sliceManager.getUserSlice();
  userSlice->dispatch([enabled](UserState &state) { state.notificationSoundEnabled = enabled; });
}

void AppStore::setOSNotificationsEnabled(bool enabled) {
  Util::logDebug("AppStore", "Setting OS notifications", "enabled=" + juce::String(enabled ? "true" : "false"));

  auto userSlice = sliceManager.getUserSlice();
  userSlice->dispatch([enabled](UserState &state) { state.osNotificationsEnabled = enabled; });
}

// ==============================================================================
// Social Metrics

void AppStore::updateFollowerCount(int count) {
  Util::logDebug("AppStore", "Updating follower count", "count=" + juce::String(count));

  auto userSlice = sliceManager.getUserSlice();
  userSlice->dispatch([count](UserState &state) { state.followerCount = count; });
}

void AppStore::updateFollowingCount(int count) {
  Util::logDebug("AppStore", "Updating following count", "count=" + juce::String(count));

  auto userSlice = sliceManager.getUserSlice();
  userSlice->dispatch([count](UserState &state) { state.followingCount = count; });
}

void AppStore::updatePostCount(int count) {
  Util::logDebug("AppStore", "Updating post count", "count=" + juce::String(count));

  auto userSlice = sliceManager.getUserSlice();
  userSlice->dispatch([count](UserState &state) { state.postCount = count; });
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

  auto userSlice = sliceManager.getUserSlice();

  networkClient->changeUsername(newUsername, [userSlice, newUsername](Outcome<juce::var> result) {
    juce::MessageManager::callAsync([userSlice, newUsername, result]() {
      if (result.isOk()) {
        userSlice->dispatch([newUsername](UserState &state) { state.username = newUsername; });
        Util::logInfo("AppStore", "Username changed successfully");
      } else {
        Util::logError("AppStore", "Failed to change username: " + result.getError());
      }
    });
  });
}

// ==============================================================================
// Helper Methods

void AppStore::downloadProfileImage(const juce::String &url) {
  if (url.isEmpty())
    return;

  auto userSlice = sliceManager.getUserSlice();

  // Try cache first
  if (auto cached = imageCache.getImage(url)) {
    userSlice->dispatch([cached](UserState &state) { state.profileImage = *cached; });
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
      [userSlice](const juce::Image &img) {
        userSlice->dispatch([img](UserState &state) { state.profileImage = img; });
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

  auto userSlice = sliceManager.getUserSlice();

  userSlice->dispatch([data](UserState &state) {
    state.userId = data.getProperty("id", "").toString();
    state.username = data.getProperty("name", "").toString();
    state.email = data.getProperty("email", "").toString();
    state.bio = data.getProperty("bio", "").toString();
    state.profilePictureUrl = data.getProperty("image", "").toString();
    state.userError = "";
    state.isFetchingProfile = false;
    state.lastProfileUpdate = juce::Time::getCurrentTime().toMilliseconds();
  });

  // Download the profile image if URL is available
  auto profileUrl = data.getProperty("image", "").toString();
  if (profileUrl.isNotEmpty()) {
    downloadProfileImage(profileUrl);
  }
}

void AppStore::handleProfileFetchError(const juce::String &error) {
  auto userSlice = sliceManager.getUserSlice();

  userSlice->dispatch([error](UserState &state) {
    state.userError = error;
    state.isFetchingProfile = false;
  });
}

void AppStore::followUser(const juce::String &userId) {
  if (!networkClient || userId.isEmpty())
    return;

  auto userSlice = sliceManager.getUserSlice();

  networkClient->followUser(userId, [userSlice](Outcome<juce::var> result) {
    juce::MessageManager::callAsync([userSlice, result]() {
      if (result.isOk()) {
        // Update following count
        userSlice->dispatch([](UserState &state) { state.followingCount++; });
      }
    });
  });
}

void AppStore::unfollowUser(const juce::String &userId) {
  if (!networkClient || userId.isEmpty())
    return;

  auto userSlice = sliceManager.getUserSlice();

  networkClient->unfollowUser(userId, [userSlice](Outcome<juce::var> result) {
    juce::MessageManager::callAsync([userSlice, result]() {
      if (result.isOk()) {
        // Update following count
        userSlice->dispatch([](UserState &state) {
          if (state.followingCount > 0)
            state.followingCount--;
        });
      }
    });
  });
}

void AppStore::updateProfileComplete(const juce::String &username, const juce::String &displayName,
                                     const juce::String &bio, const juce::String &location, const juce::String &genre,
                                     const juce::var &socialLinks, bool isPrivate, const juce::String &dawPreference) {
  auto userSlice = sliceManager.getUserSlice();

  userSlice->dispatch(
      [username, displayName, bio, location, genre, socialLinks, isPrivate, dawPreference](UserState &state) {
        state.username = username;
        state.displayName = displayName;
        state.bio = bio;
        state.location = location;
        state.genre = genre;
        state.socialLinks = socialLinks;
        state.isPrivate = isPrivate;
        state.dawPreference = dawPreference;
      });
}

void AppStore::uploadProfilePicture(const juce::File &file) {
  if (!networkClient)
    return;

  networkClient->uploadProfilePicture(file, [this](Outcome<juce::String> result) {
    juce::MessageManager::callAsync([this, result]() {
      if (result.isOk()) {
        setProfilePictureUrl(result.getValue());
      }
    });
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

  auto searchSlice = sliceManager.getSearchSlice();
  searchSlice->dispatch([](SearchState &state) {
    state.results.isSearching = true;
    state.results.searchError = "";
  });

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

  auto searchSlice = sliceManager.getSearchSlice();
  searchSlice->dispatch([](SearchState &state) {
    state.results.isSearching = true;
    state.results.searchError = "";
  });

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

  auto searchSlice = sliceManager.getSearchSlice();
  searchSlice->dispatch([](SearchState &state) {
    state.results.isSearching = true;
    state.results.searchError = "";
  });

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
  auto searchSlice = sliceManager.getSearchSlice();
  searchSlice->dispatch([users](SearchState &state) {
    state.results.users = users;
    state.results.isSearching = false;
    state.results.searchError = "";
  });

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
  auto searchSlice = sliceManager.getSearchSlice();
  searchSlice->dispatch([users](SearchState &state) {
    state.results.users = users;
    state.results.isSearching = false;
    state.results.searchError = "";
  });

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
  auto searchSlice = sliceManager.getSearchSlice();
  searchSlice->dispatch([users](SearchState &state) {
    state.results.users = users;
    state.results.isSearching = false;
    state.results.searchError = "";
  });

  Util::logInfo("AppStore", "Loaded " + juce::String(users.size()) + " suggested users");
}

// ==============================================================================
// Discovery Error Handlers

void AppStore::handleTrendingUsersError(const juce::String &error) {
  Util::logError("AppStore", "Failed to load trending users: " + error);

  auto searchSlice = sliceManager.getSearchSlice();
  searchSlice->dispatch([error](SearchState &state) {
    state.results.isSearching = false;
    state.results.searchError = error;
  });
}

void AppStore::handleFeaturedProducersError(const juce::String &error) {
  Util::logError("AppStore", "Failed to load featured producers: " + error);

  auto searchSlice = sliceManager.getSearchSlice();
  searchSlice->dispatch([error](SearchState &state) {
    state.results.isSearching = false;
    state.results.searchError = error;
  });
}

void AppStore::handleSuggestedUsersError(const juce::String &error) {
  Util::logError("AppStore", "Failed to load suggested users: " + error);

  auto searchSlice = sliceManager.getSearchSlice();
  searchSlice->dispatch([error](SearchState &state) {
    state.results.isSearching = false;
    state.results.searchError = error;
  });
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

  // Make network request
  networkClient->getUser(userId, [this, userId](Outcome<juce::var> result) {
    auto &entityStore = EntityStore::getInstance();

    if (result.isOk()) {
      try {
        // Convert juce::var to nlohmann::json
        auto jsonStr = result.getValue().toString().toStdString();
        auto json = nlohmann::json::parse(jsonStr);

        // Normalize user (creates/updates shared_ptr in EntityStore)
        auto normalized = entityStore.normalizeUser(json);
        if (normalized) {
          Util::logInfo("AppStore", "Loaded user: " + userId);
        } else {
          Util::logError("AppStore", "Failed to normalize user data for: " + userId);
        }
      } catch (const std::exception &e) {
        Util::logError("AppStore", "Failed to parse user JSON: " + juce::String(e.what()));
      }
    } else {
      Util::logError("AppStore", "Failed to load user " + userId + ": " + result.getError());
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

  // Make network request - getUserPosts takes ResponseCallback which returns Outcome<juce::var>
  networkClient->getUserPosts(userId, limit, offset, [userId](Outcome<juce::var> result) {
    auto &entityStore = EntityStore::getInstance();

    if (result.isOk()) {
      auto postsData = result.getValue();

      // Normalize posts
      if (postsData.isArray()) {
        std::vector<std::shared_ptr<FeedPost>> normalizedPosts;
        for (int i = 0; i < postsData.size(); ++i) {
          // normalizePost expects juce::var
          if (auto normalized = entityStore.normalizePost(postsData[i])) {
            normalizedPosts.push_back(normalized);
          }
        }
        Util::logInfo("AppStore", "Loaded " + juce::String(normalizedPosts.size()) + " posts for user: " + userId);
      }
    } else {
      Util::logError("AppStore", "Failed to load posts for user " + userId + ": " + result.getError());
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

  // Redux Action: Set loading state
  sliceManager.getFollowersSlice()->dispatch([userId](FollowersState &state) {
    state.targetUserId = userId.toStdString();
    state.listType = FollowersState::Followers;
    state.isLoading = true;
    state.errorMessage = "";
    state.users.clear();
  });

  // Network request to fetch followers
  networkClient->getFollowers(userId, limit, offset, [this, userId](Outcome<juce::var> result) {
    if (result.isError()) {
      // Reducer: Create error state
      auto error = result.getError().toStdString();
      sliceManager.getFollowersSlice()->dispatch([error](FollowersState &state) {
        state.isLoading = false;
        state.errorMessage = error;
      });
      return;
    }

    try {
      auto jsonArray = result.getValue().getArray();
      if (!jsonArray) {
        throw std::runtime_error("Response is not an array");
      }

      // Step 1: Normalize JSON to mutable User objects and store in EntityCache
      std::vector<std::shared_ptr<Sidechain::User>> users;
      for (int i = 0; i < jsonArray->size(); ++i) {
        try {
          auto jsonStr = (*jsonArray)[i].toString().toStdString();
          auto json = nlohmann::json::parse(jsonStr);

          // Normalize and cache in EntityStore
          auto user = EntityStore::getInstance().normalizeUser(json);
          if (user) {
            users.push_back(user);
          }
        } catch (const std::exception &e) {
          Util::logError("AppStore", "Failed to parse follower JSON: " + juce::String(e.what()));
        }
      }

      // Step 2: Reducer - Create new immutable FollowersState
      sliceManager.getFollowersSlice()->dispatch([users](FollowersState &state) {
        state.isLoading = false;
        state.errorMessage = "";
        state.totalCount = static_cast<int>(users.size());

        // Convert mutable users to immutable for state
        state.users.clear();
        for (const auto &user : users) {
          state.users.push_back(std::const_pointer_cast<const Sidechain::User>(user));
        }
      });

      Util::logInfo("AppStore", "Loaded " + juce::String(users.size()) + " followers");

    } catch (const std::exception &e) {
      // Reducer: Create error state
      auto error = std::string(e.what());
      sliceManager.getFollowersSlice()->dispatch([error](FollowersState &state) {
        state.isLoading = false;
        state.errorMessage = error;
      });
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

  // Redux Action: Set loading state
  sliceManager.getFollowersSlice()->dispatch([userId](FollowersState &state) {
    state.targetUserId = userId.toStdString();
    state.listType = FollowersState::Following;
    state.isLoading = true;
    state.errorMessage = "";
    state.users.clear();
  });

  // Network request to fetch following
  networkClient->getFollowing(userId, limit, offset, [this, userId](Outcome<juce::var> result) {
    if (result.isError()) {
      // Reducer: Create error state
      auto error = result.getError().toStdString();
      sliceManager.getFollowersSlice()->dispatch([error](FollowersState &state) {
        state.isLoading = false;
        state.errorMessage = error;
      });
      return;
    }

    try {
      auto jsonArray = result.getValue().getArray();
      if (!jsonArray) {
        throw std::runtime_error("Response is not an array");
      }

      // Step 1: Normalize JSON to mutable User objects and store in EntityCache
      std::vector<std::shared_ptr<Sidechain::User>> users;
      for (int i = 0; i < jsonArray->size(); ++i) {
        try {
          auto jsonStr = (*jsonArray)[i].toString().toStdString();
          auto json = nlohmann::json::parse(jsonStr);

          // Normalize and cache in EntityStore
          auto user = EntityStore::getInstance().normalizeUser(json);
          if (user) {
            users.push_back(user);
          }
        } catch (const std::exception &e) {
          Util::logError("AppStore", "Failed to parse following JSON: " + juce::String(e.what()));
        }
      }

      // Step 2: Reducer - Create new immutable FollowersState
      sliceManager.getFollowersSlice()->dispatch([users](FollowersState &state) {
        state.isLoading = false;
        state.errorMessage = "";
        state.totalCount = static_cast<int>(users.size());

        // Convert mutable users to immutable for state
        state.users.clear();
        for (const auto &user : users) {
          state.users.push_back(std::const_pointer_cast<const Sidechain::User>(user));
        }
      });

      Util::logInfo("AppStore", "Loaded " + juce::String(users.size()) + " following");

    } catch (const std::exception &e) {
      // Reducer: Create error state
      auto error = std::string(e.what());
      sliceManager.getFollowersSlice()->dispatch([error](FollowersState &state) {
        state.isLoading = false;
        state.errorMessage = error;
      });
      Util::logError("AppStore", "Failed to load following: " + juce::String(e.what()));
    }
  });
}

// Old handler methods removed - now using Redux pattern in loadFollowers/loadFollowing

} // namespace Stores
} // namespace Sidechain
