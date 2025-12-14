#include "UserStore.h"
#include "../util/Async.h"
#include "../util/PropertiesFileUtils.h"

namespace Sidechain {
namespace Stores {

UserStore::UserStore()
{
    Util::logInfo("UserStore", "Initialized reactive user store");

    // Load saved settings
    loadFromSettings();
}

//==============================================================================
// Authentication

void UserStore::setAuthToken(const juce::String& token)
{
    if (token.isEmpty())
    {
        clearAuthToken();
        return;
    }

    Util::logInfo("UserStore", "Setting auth token");

    updateState([token](UserState& state)
    {
        state.authToken = token;
        state.isLoggedIn = !token.isEmpty();
    });

    // Save to persistent storage
    saveToSettings();

    // Fetch user profile
    fetchUserProfile(false);
}

void UserStore::clearAuthToken()
{
    Util::logInfo("UserStore", "Clearing auth token");

    updateState([](UserState& state)
    {
        state = UserState{}; // Reset to default state
        state.isLoggedIn = false;
    });

    // Clear persistent storage
    clearAll();
}

//==============================================================================
// Profile Management

void UserStore::fetchUserProfile(bool forceRefresh)
{
    if (!networkClient)
    {
        Util::logError("UserStore", "Cannot fetch profile - network client not configured");
        return;
    }

    auto currentState = getState();
    if (!currentState.isLoggedIn)
    {
        Util::logWarning("UserStore", "Cannot fetch profile - not logged in");
        return;
    }

    // Check if we need to refresh
    if (!forceRefresh && currentState.lastProfileUpdate > 0)
    {
        auto age = juce::Time::getCurrentTime().toMilliseconds() - currentState.lastProfileUpdate;
        if (age < 60000) // Less than 1 minute old
        {
            Util::logDebug("UserStore", "Using cached profile");
            return;
        }
    }

    Util::logInfo("UserStore", "Fetching user profile", "forceRefresh=" + juce::String(forceRefresh ? "true" : "false"));

    updateState([](UserState& state)
    {
        state.isFetchingProfile = true;
        state.error = "";
    });

    networkClient->getCurrentUser([this](Outcome<juce::var> result)
    {
        if (result.isOk())
        {
            handleProfileFetchSuccess(result.getValue());
        }
        else
        {
            handleProfileFetchError(result.getError());
        }
    });
}

void UserStore::updateProfile(const juce::String& username,
                               const juce::String& displayName,
                               const juce::String& bio)
{
    if (!networkClient)
    {
        Util::logError("UserStore", "Cannot update profile - network client not configured");
        return;
    }

    Util::logInfo("UserStore", "Updating user profile");

    // Optimistic update
    optimisticUpdate(
        [username, displayName, bio](UserState& state)
        {
            if (!username.isEmpty())
                state.username = username;
            if (!displayName.isEmpty())
                state.displayName = displayName;
            if (!bio.isEmpty())
                state.bio = bio;
        },
        [this, username, displayName, bio](auto callback)
        {
            networkClient->updateUserProfile(username, displayName, bio,
                [callback](Outcome<juce::var> result)
                {
                    callback(result.isOk(), result.isOk() ? "" : result.getError());
                }
            );
        },
        [](const juce::String& error)
        {
            Util::logError("UserStore", "Failed to update profile: " + error);
        }
    );
}

void UserStore::setProfilePictureUrl(const juce::String& url)
{
    if (url.isEmpty())
        return;

    Util::logInfo("UserStore", "Setting profile picture URL", "url=" + url);

    updateState([url](UserState& state)
    {
        state.profilePictureUrl = url;
        state.isLoadingImage = true;
    });

    // Download image asynchronously
    downloadProfileImage(url);

    // Save to settings
    saveToSettings();
}

void UserStore::setLocalPreviewImage(const juce::File& imageFile)
{
    if (!imageFile.existsAsFile())
        return;

    Util::logInfo("UserStore", "Setting local preview image", "file=" + imageFile.getFullPathName());

    // Load image on background thread
    Async::run<juce::Image>(
        [imageFile]()
        {
            return juce::ImageFileFormat::loadFrom(imageFile);
        },
        [this](const juce::Image& image)
        {
            if (image.isValid())
            {
                updateState([image](UserState& state)
                {
                    state.profileImage = image;
                });
            }
        }
    );
}

void UserStore::refreshProfileImage()
{
    auto currentState = getState();
    if (currentState.profilePictureUrl.isEmpty())
        return;

    Util::logInfo("UserStore", "Refreshing profile image");

    downloadProfileImage(currentState.profilePictureUrl);
}

//==============================================================================
// User Preferences

void UserStore::setNotificationSoundEnabled(bool enabled)
{
    Util::logDebug("UserStore", "Setting notification sound", "enabled=" + juce::String(enabled ? "true" : "false"));

    updateState([enabled](UserState& state)
    {
        state.notificationSoundEnabled = enabled;
    });

    saveToSettings();
}

void UserStore::setOSNotificationsEnabled(bool enabled)
{
    Util::logDebug("UserStore", "Setting OS notifications", "enabled=" + juce::String(enabled ? "true" : "false"));

    updateState([enabled](UserState& state)
    {
        state.osNotificationsEnabled = enabled;
    });

    saveToSettings();
}

//==============================================================================
// Social Metrics

void UserStore::updateFollowerCount(int count)
{
    Util::logDebug("UserStore", "Updating follower count", "count=" + juce::String(count));

    updateState([count](UserState& state)
    {
        state.followerCount = count;
    });
}

void UserStore::updateFollowingCount(int count)
{
    Util::logDebug("UserStore", "Updating following count", "count=" + juce::String(count));

    updateState([count](UserState& state)
    {
        state.followingCount = count;
    });
}

void UserStore::updatePostCount(int count)
{
    Util::logDebug("UserStore", "Updating post count", "count=" + juce::String(count));

    updateState([count](UserState& state)
    {
        state.postCount = count;
    });
}

//==============================================================================
// Username & Profile Picture Management (Task 2.4)

void UserStore::changeUsername(const juce::String& newUsername)
{
    if (!networkClient)
    {
        Util::logError("UserStore", "Cannot change username - network client not configured");
        return;
    }

    if (newUsername.isEmpty())
    {
        Util::logError("UserStore", "Cannot change username - empty username");
        return;
    }

    Util::logInfo("UserStore", "Changing username to: " + newUsername);

    networkClient->changeUsername(newUsername, [this, newUsername](Outcome<juce::var> result)
    {
        juce::MessageManager::callAsync([this, newUsername, result]()
        {
            if (result.isOk())
            {
                updateState([newUsername](UserState& state)
                {
                    state.username = newUsername;
                });
                Util::logInfo("UserStore", "Username changed successfully");
            }
            else
            {
                Util::logError("UserStore", "Failed to change username: " + result.getError());
            }
        });
    });
}

void UserStore::updateProfileComplete(const juce::String& displayName,
                                      const juce::String& bio,
                                      const juce::String& location,
                                      const juce::String& genre,
                                      const juce::String& dawPreference,
                                      const juce::var& socialLinks,
                                      bool isPrivate,
                                      const juce::String& profilePictureUrl)
{
    if (!networkClient)
    {
        Util::logError("UserStore", "Cannot update profile - network client not configured");
        return;
    }

    Util::logInfo("UserStore", "Updating complete profile (Task 2.4)");

    // Build update payload matching EditProfile format
    auto* updateData = new juce::DynamicObject();
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

    networkClient->put("/profile", payload, [this, displayName, bio, location, genre, dawPreference, isPrivate]
        (Outcome<juce::var> result)
    {
        juce::MessageManager::callAsync([this, displayName, bio, location, genre, dawPreference, isPrivate, result]()
        {
            if (result.isOk())
            {
                updateState([displayName, bio, location, genre, dawPreference, isPrivate](UserState& state)
                {
                    state.displayName = displayName;
                    state.bio = bio;
                    state.location = location;
                    state.genre = genre;
                    state.dawPreference = dawPreference;
                    state.isPrivate = isPrivate;
                });
                Util::logInfo("UserStore", "Profile updated successfully");
            }
            else
            {
                Util::logError("UserStore", "Failed to update profile: " + result.getError());
            }
        });
    });
}

void UserStore::uploadProfilePicture(const juce::File& imageFile)
{
    if (!networkClient)
    {
        Util::logError("UserStore", "Cannot upload profile picture - network client not configured");
        return;
    }

    if (!imageFile.existsAsFile())
    {
        Util::logError("UserStore", "Cannot upload profile picture - file does not exist");
        return;
    }

    Util::logInfo("UserStore", "Uploading profile picture: " + imageFile.getFileName());

    // Show loading state
    updateState([](UserState& state)
    {
        state.isLoadingImage = true;
    });

    networkClient->uploadProfilePicture(imageFile, [this](Outcome<juce::String> result)
    {
        juce::MessageManager::callAsync([this, result]()
        {
            if (result.isOk())
            {
                juce::String s3Url = result.getValue();

                if (s3Url.isNotEmpty())
                {
                    setProfilePictureUrl(s3Url);
                    Util::logInfo("UserStore", "Profile picture uploaded successfully");
                }
                else
                {
                    Util::logError("UserStore", "Profile picture upload returned empty URL");
                    updateState([](UserState& state)
                    {
                        state.isLoadingImage = false;
                    });
                }
            }
            else
            {
                Util::logError("UserStore", "Profile picture upload failed: " + result.getError());
                updateState([](UserState& state)
                {
                    state.isLoadingImage = false;
                });
            }
        });
    });
}

//==============================================================================
// Persistence

void UserStore::saveToSettings()
{
    auto currentState = getState();

    auto options = getPropertiesOptions();
    juce::PropertiesFile props(options);

    props.setValue("userId", currentState.userId);
    props.setValue("username", currentState.username);
    props.setValue("email", currentState.email);
    props.setValue("displayName", currentState.displayName);
    props.setValue("bio", currentState.bio);
    props.setValue("profilePictureUrl", currentState.profilePictureUrl);
    props.setValue("authToken", currentState.authToken);
    props.setValue("notificationSoundEnabled", currentState.notificationSoundEnabled);
    props.setValue("osNotificationsEnabled", currentState.osNotificationsEnabled);

    props.saveIfNeeded();

    Util::logDebug("UserStore", "Saved settings to disk");
}

void UserStore::loadFromSettings()
{
    auto options = getPropertiesOptions();
    juce::PropertiesFile props(options);

    updateState([&props](UserState& state)
    {
        state.userId = props.getValue("userId", "");
        state.username = props.getValue("username", "");
        state.email = props.getValue("email", "");
        state.displayName = props.getValue("displayName", "");
        state.bio = props.getValue("bio", "");
        state.profilePictureUrl = props.getValue("profilePictureUrl", "");
        state.authToken = props.getValue("authToken", "");
        state.notificationSoundEnabled = props.getBoolValue("notificationSoundEnabled", true);
        state.osNotificationsEnabled = props.getBoolValue("osNotificationsEnabled", true);
        state.isLoggedIn = !state.authToken.isEmpty();
    });

    auto currentState = getState();
    if (currentState.isLoggedIn)
    {
        Util::logInfo("UserStore", "Loaded settings from disk", "username=" + currentState.username);

        // Download profile image if we have a URL
        if (!currentState.profilePictureUrl.isEmpty())
        {
            downloadProfileImage(currentState.profilePictureUrl);
        }
    }
}

void UserStore::clearAll()
{
    auto options = getPropertiesOptions();
    juce::PropertiesFile props(options);

    props.clear();
    props.saveIfNeeded();

    Util::logInfo("UserStore", "Cleared all settings");
}

//==============================================================================
// Internal Helpers

void UserStore::downloadProfileImage(const juce::String& url)
{
    if (url.isEmpty())
        return;

    updateState([](UserState& state)
    {
        state.isLoadingImage = true;
    });

    // Download on background thread
    Async::run<juce::Image>(
        [url]()
        {
            // Download image data
            juce::URL imageUrl(url);
            auto inputStream = imageUrl.createInputStream(juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress));

            if (inputStream != nullptr)
            {
                juce::MemoryBlock imageData;
                inputStream->readIntoMemoryBlock(imageData);

                // Decode image
                return juce::ImageFileFormat::loadFrom(imageData.getData(), imageData.getSize());
            }

            return juce::Image();
        },
        [this](const juce::Image& image)
        {
            updateState([image](UserState& state)
            {
                state.isLoadingImage = false;
                if (image.isValid())
                {
                    state.profileImage = image;
                }
            });

            if (image.isValid())
            {
                Util::logInfo("UserStore", "Profile image downloaded successfully");
            }
            else
            {
                Util::logWarning("UserStore", "Failed to download profile image");
            }
        }
    );
}

void UserStore::handleProfileFetchSuccess(const juce::var& data)
{
    Util::logInfo("UserStore", "Profile fetch successful");

    updateState([&data](UserState& state)
    {
        state.isFetchingProfile = false;
        state.error = "";

        // Parse user data
        if (data.isObject())
        {
            state.userId = data.getProperty("id", state.userId).toString();
            state.username = data.getProperty("username", state.username).toString();
            state.email = data.getProperty("email", state.email).toString();
            state.displayName = data.getProperty("display_name", state.displayName).toString();
            state.bio = data.getProperty("bio", state.bio).toString();
            state.location = data.getProperty("location", state.location).toString();

            auto newProfilePicUrl = data.getProperty("profile_picture_url", "").toString();
            if (!newProfilePicUrl.isEmpty() && newProfilePicUrl != state.profilePictureUrl)
            {
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
    if (!currentState.profilePictureUrl.isEmpty() &&
        (!currentState.profileImage.isValid() || currentState.isLoadingImage))
    {
        downloadProfileImage(currentState.profilePictureUrl);
    }

    // Save to settings
    saveToSettings();
}

void UserStore::handleProfileFetchError(const juce::String& error)
{
    Util::logError("UserStore", "Profile fetch failed: " + error);

    updateState([error](UserState& state)
    {
        state.isFetchingProfile = false;
        state.error = error;
    });
}

juce::PropertiesFile::Options UserStore::getPropertiesOptions() const
{
    return Sidechain::Util::PropertiesFileUtils::getStandardOptions();
}

}  // namespace Stores
}  // namespace Sidechain
