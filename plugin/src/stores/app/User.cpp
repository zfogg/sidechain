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

  auto currentState = getState();
  if (!currentState.auth.isLoggedIn) {
    Util::logWarning("AppStore", "Cannot fetch profile - not logged in");
    return;
  }

  // Check memory cache first (if not force refreshing)
  if (!forceRefresh) {
    // Try memory cache with 10-minute TTL
    auto cachedProfile = getCached<juce::var>("user:profile");
    if (cachedProfile.has_value()) {
      Util::logDebug("AppStore", "Using cached profile from memory");
      handleProfileFetchSuccess(cachedProfile.value());
      return;
    }

    // Also check state-based cache as fallback
    if (currentState.user.lastProfileUpdate > 0) {
      auto age = juce::Time::getCurrentTime().toMilliseconds() - currentState.user.lastProfileUpdate;
      if (age < 60000) // Less than 1 minute old
      {
        Util::logDebug("AppStore", "Using cached profile from state");
        return;
      }
    }
  }

  Util::logInfo("AppStore", "Fetching user profile", "forceRefresh=" + juce::String(forceRefresh ? "true" : "false"));

  updateUserState([](UserState &state) {
    state.isFetchingProfile = true;
    state.userError = "";
  });

  networkClient->getCurrentUser([this](Outcome<juce::var> result) {
    if (result.isOk()) {
      handleProfileFetchSuccess(result.getValue());
    } else {
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

  // Optimistic update
  auto previousState = getState().user;
  updateUserState([username, displayName, bio](UserState &state) {
    if (!username.isEmpty())
      state.username = username;
    if (!displayName.isEmpty())
      state.displayName = displayName;
    if (!bio.isEmpty())
      state.bio = bio;
  });

  networkClient->updateUserProfile(username, displayName, bio, [this, previousState](Outcome<juce::var> result) {
    juce::MessageManager::callAsync([this, previousState, result]() {
      if (!result.isOk()) {
        Util::logError("AppStore", "Failed to update profile: " + result.getError());
        // Revert on error
        updateUserState([previousState](UserState &state) { state = previousState; });
      } else {
        // Invalidate cache on successful update
        invalidateCache("user:profile");
      }
    });
  });
}

void AppStore::setProfilePictureUrl(const juce::String &url) {
  if (url.isEmpty())
    return;

  Util::logInfo("AppStore", "Setting profile picture URL", "url=" + url);

  updateUserState([url](UserState &state) {
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

  // Load image on background thread
  Async::run<juce::Image>([imageFile]() { return juce::ImageFileFormat::loadFrom(imageFile); },
                          [this](const juce::Image &image) {
                            if (image.isValid()) {
                              updateUserState([image](UserState &state) { state.profileImage = image; });
                            }
                          });
}

void AppStore::refreshProfileImage() {
  auto currentState = getState();
  if (currentState.user.profilePictureUrl.isEmpty())
    return;

  Util::logInfo("AppStore", "Refreshing profile image");

  downloadProfileImage(currentState.user.profilePictureUrl);
}

//==============================================================================
// User Preferences

void AppStore::setNotificationSoundEnabled(bool enabled) {
  Util::logDebug("AppStore", "Setting notification sound", "enabled=" + juce::String(enabled ? "true" : "false"));

  updateUserState([enabled](UserState &state) { state.notificationSoundEnabled = enabled; });
}

void AppStore::setOSNotificationsEnabled(bool enabled) {
  Util::logDebug("AppStore", "Setting OS notifications", "enabled=" + juce::String(enabled ? "true" : "false"));

  updateUserState([enabled](UserState &state) { state.osNotificationsEnabled = enabled; });
}

//==============================================================================
// Social Metrics

void AppStore::updateFollowerCount(int count) {
  Util::logDebug("AppStore", "Updating follower count", "count=" + juce::String(count));

  updateUserState([count](UserState &state) { state.followerCount = count; });
}

void AppStore::updateFollowingCount(int count) {
  Util::logDebug("AppStore", "Updating following count", "count=" + juce::String(count));

  updateUserState([count](UserState &state) { state.followingCount = count; });
}

void AppStore::updatePostCount(int count) {
  Util::logDebug("AppStore", "Updating post count", "count=" + juce::String(count));

  updateUserState([count](UserState &state) { state.postCount = count; });
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

  networkClient->changeUsername(newUsername, [this, newUsername](Outcome<juce::var> result) {
    juce::MessageManager::callAsync([this, newUsername, result]() {
      if (result.isOk()) {
        updateUserState([newUsername](UserState &state) { state.username = newUsername; });
        Util::logInfo("AppStore", "Username changed successfully");
      } else {
        Util::logError("AppStore", "Failed to change username: " + result.getError());
      }
    });
  });
}

void AppStore::updateProfileComplete(const juce::String &displayName, const juce::String &bio,
                                     const juce::String &location, const juce::String &genre,
                                     const juce::String &dawPreference, const juce::var &socialLinks, bool isPrivate,
                                     const juce::String &profilePictureUrl) {
  if (!networkClient) {
    Util::logError("AppStore", "Cannot update profile - network client not configured");
    return;
  }

  Util::logInfo("AppStore", "Updating complete profile");

  // Build update payload matching EditProfile format
  auto *updateData = new juce::DynamicObject();
  updateData->setProperty("display_name", displayName);
  updateData->setProperty("bio", bio);
  updateData->setProperty("location", location);
  updateData->setProperty("genre", genre);
  updateData->setProperty("daw_preference", dawPreference);
  updateData->setProperty("social_links", socialLinks);
  updateData->setProperty("is_private", isPrivate);

  if (profilePictureUrl.isNotEmpty())
    updateData->setProperty("profile_picture_url", profilePictureUrl);

  juce::var payload(updateData);

  networkClient->put(
      "/users/me", payload,
      [this, displayName, bio, location, genre, dawPreference, isPrivate](Outcome<juce::var> result) {
        juce::MessageManager::callAsync([this, displayName, bio, location, genre, dawPreference, isPrivate, result]() {
          if (result.isOk()) {
            updateUserState([displayName, bio, location, genre, dawPreference, isPrivate](UserState &state) {
              state.displayName = displayName;
              state.bio = bio;
              state.location = location;
              state.genre = genre;
              state.dawPreference = dawPreference;
              state.isPrivate = isPrivate;
            });
            // Invalidate profile cache on successful update
            invalidateCache("user:profile");
            Util::logInfo("AppStore", "Profile updated successfully");
          } else {
            Util::logError("AppStore", "Failed to update profile: " + result.getError());
          }
        });
      });
}

void AppStore::uploadProfilePicture(const juce::File &imageFile) {
  if (!networkClient) {
    Util::logError("AppStore", "Cannot upload profile picture - network client not configured");
    return;
  }

  if (!imageFile.existsAsFile()) {
    Util::logError("AppStore", "Cannot upload profile picture - file does not exist");
    return;
  }

  Util::logInfo("AppStore", "Uploading profile picture: " + imageFile.getFileName());

  // Show loading state
  updateUserState([](UserState &state) { state.isLoadingImage = true; });

  networkClient->uploadProfilePicture(imageFile, [this](Outcome<juce::String> result) {
    juce::MessageManager::callAsync([this, result]() {
      if (result.isOk()) {
        juce::String s3Url = result.getValue();

        if (s3Url.isNotEmpty()) {
          setProfilePictureUrl(s3Url);
          Util::logInfo("AppStore", "Profile picture uploaded successfully");
        } else {
          Util::logError("AppStore", "Profile picture upload returned empty URL");
          updateUserState([](UserState &state) { state.isLoadingImage = false; });
        }
      } else {
        Util::logError("AppStore", "Profile picture upload failed: " + result.getError());
        updateUserState([](UserState &state) { state.isLoadingImage = false; });
      }
    });
  });
}

//==============================================================================
// Internal Helpers

void AppStore::downloadProfileImage(const juce::String &url) {
  if (url.isEmpty())
    return;

  updateUserState([](UserState &state) { state.isLoadingImage = true; });

  // Use getImage which handles all three cache levels automatically
  getImage(url, [this](const juce::Image &image) {
    updateUserState([image](UserState &state) {
      state.isLoadingImage = false;
      if (image.isValid()) {
        state.profileImage = image;
      }
    });

    if (image.isValid()) {
      Util::logInfo("AppStore", "Profile image downloaded successfully");
    } else {
      Util::logWarning("AppStore", "Failed to download profile image");
    }
  });
}

void AppStore::handleProfileFetchSuccess(const juce::var &data) {
  Util::logInfo("AppStore", "Profile fetch successful");

  // Cache the profile data in memory cache (10-minute TTL)
  setCached<juce::var>("user:profile", data, 600);

  updateUserState([&data](UserState &state) {
    state.isFetchingProfile = false;
    state.userError = "";

    // Parse user data
    if (data.isObject()) {
      state.userId = data.getProperty("id", state.userId).toString();
      state.username = data.getProperty("username", state.username).toString();
      state.email = data.getProperty("email", state.email).toString();
      state.displayName = data.getProperty("display_name", state.displayName).toString();
      state.bio = data.getProperty("bio", state.bio).toString();
      state.location = data.getProperty("location", state.location).toString();

      auto newProfilePicUrl = data.getProperty("profile_picture_url", "").toString();
      if (!newProfilePicUrl.isEmpty() && newProfilePicUrl != state.profilePictureUrl) {
        state.profilePictureUrl = newProfilePicUrl;
      }

      // Social metrics
      state.followerCount = static_cast<int>(data.getProperty("follower_count", 0));
      state.followingCount = static_cast<int>(data.getProperty("following_count", 0));
      state.postCount = static_cast<int>(data.getProperty("post_count", 0));

      state.lastProfileUpdate = juce::Time::getCurrentTime().toMilliseconds();
    }
  });

  auto currentState = getState();

  // Download profile image if URL changed
  if (!currentState.user.profilePictureUrl.isEmpty() &&
      (!currentState.user.profileImage.isValid() || currentState.user.isLoadingImage)) {
    downloadProfileImage(currentState.user.profilePictureUrl);
  }
}

void AppStore::handleProfileFetchError(const juce::String &error) {
  Util::logError("AppStore", "Profile fetch failed: " + error);

  updateUserState([error](UserState &state) {
    state.isFetchingProfile = false;
    state.userError = error;
  });
}

//==============================================================================
// Image Fetching with Multi-Level Caching
//
// Strategy: Memory -> File Cache -> HTTP Download
// Single unified getImage() that handles all three levels automatically

void AppStore::getImage(const juce::String &url, std::function<void(const juce::Image &)> callback) {
  if (url.isEmpty() || !callback) {
    if (callback) {
      callback(juce::Image());
    }
    return;
  }

  // Level 1: Try memory cache first (synchronous, immediate callback)
  auto memoryImage = imageCache.getImage(url);
  if (memoryImage) {
    Util::logDebug("AppStore", "Image cache hit (memory): " + url);
    callback(*memoryImage);
    return;
  }

  // Level 2 & 3: File cache + HTTP download on background thread
  // SidechainImageCache::getImage() checks memory then file automatically
  Async::run<juce::Image>(
      [this, url]() {
        try {
          // Double-check memory cache (in case another thread cached it)
          auto memoryCheck = imageCache.getImage(url);
          if (memoryCheck) {
            Util::logDebug("AppStore", "Image cache hit (memory after check): " + url);
            return *memoryCheck;
          }

          // Try file cache - SidechainImageCache::getImage() checks:
          // 1. Memory (we already checked, will miss)
          // 2. File cache (returns if found)
          // 3. Auto-promotes file cache hit to memory
          auto fileImage = imageCache.getImage(url);
          if (fileImage) {
            Util::logDebug("AppStore", "Image cache hit (file): " + url);
            return *fileImage;
          }

          // Level 3: Not in memory or file cache, download from HTTP
          Util::logDebug("AppStore", "Image cache miss, downloading: " + url);

          juce::URL imageUrl(url);
          auto inputStream =
              imageUrl.createInputStream(false, nullptr, nullptr, "User-Agent: Sidechain/1.0", 5000, nullptr);
          if (inputStream == nullptr) {
            Util::logWarning("AppStore", "Failed to open image stream for " + url);
            return juce::Image();
          }

          juce::MemoryBlock imageData;
          inputStream->readIntoMemoryBlock(imageData);

          if (imageData.isEmpty()) {
            Util::logWarning("AppStore", "Downloaded image data is empty for " + url);
            return juce::Image();
          }

          auto image = juce::ImageFileFormat::loadFrom(imageData.getData(), imageData.getSize());
          if (!image.isValid()) {
            Util::logWarning("AppStore", "Failed to decode image from " + url);
            return juce::Image();
          }

          // Cache downloaded image in both memory and file caches
          imageCache.cacheImage(url, image);

          Util::logInfo("AppStore", "Image downloaded and cached: " + url);
          return image;
        } catch (const std::exception &e) {
          Util::logError("AppStore", "Image fetch error: " + juce::String(e.what()));
          return juce::Image();
        }
      },
      [callback](const juce::Image &image) { callback(image); });
}

} // namespace Stores
} // namespace Sidechain
