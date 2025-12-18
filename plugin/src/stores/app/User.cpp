#include "../AppStore.h"
#include "../../util/logging/Logger.h"
#include "../../util/Async.h"
#include "../../util/PropertiesFileUtils.h"

namespace Sidechain {
namespace Stores {

//==============================================================================
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

  networkClient->updateUserProfile(username, displayName, bio, [this, userSlice, previousState](Outcome<juce::var> result) {
    juce::MessageManager::callAsync([this, userSlice, previousState, result]() {
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

//==============================================================================
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

//==============================================================================
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

//==============================================================================
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

  networkClient->changeUsername(newUsername, [this, userSlice, newUsername](Outcome<juce::var> result) {
    juce::MessageManager::callAsync([this, userSlice, newUsername, result]() {
      if (result.isOk()) {
        userSlice->dispatch([newUsername](UserState &state) { state.username = newUsername; });
        Util::logInfo("AppStore", "Username changed successfully");
      } else {
        Util::logError("AppStore", "Failed to change username: " + result.getError());
      }
    });
  });
}

//==============================================================================
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
          auto inputStream = imageUrl.createInputStream(false, nullptr, nullptr, "User-Agent: Sidechain/1.0", 5000, nullptr);
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
        userSlice->dispatch([](UserState &state) {
          state.followingCount++;
        });
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
                                     const juce::String &bio, const juce::String &location,
                                     const juce::String &genre, const juce::var &socialLinks,
                                     bool isPrivate, const juce::String &dawPreference) {
  auto userSlice = sliceManager.getUserSlice();

  userSlice->dispatch([username, displayName, bio, location, genre, socialLinks, isPrivate, dawPreference](UserState &state) {
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

} // namespace Stores
} // namespace Sidechain
