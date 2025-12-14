#pragma once

#include <JuceHeader.h>
#include "Store.h"
#include "../network/NetworkClient.h"
#include "../util/logging/Logger.h"

namespace Sidechain {
namespace Stores {

/**
 * UserState - Immutable state for current user's profile
 */
struct UserState
{
    // User identity
    juce::String userId;
    juce::String username;
    juce::String email;
    juce::String displayName;
    juce::String bio;
    juce::String location;
    juce::String profilePictureUrl;
    juce::String authToken;

    // Profile image
    juce::Image profileImage;
    bool isLoadingImage = false;

    // User preferences
    bool notificationSoundEnabled = true;
    bool osNotificationsEnabled = true;

    // State flags
    bool isLoggedIn = false;
    bool isFetchingProfile = false;
    juce::String error;
    int64_t lastProfileUpdate = 0;

    // Social metrics
    int followerCount = 0;
    int followingCount = 0;
    int postCount = 0;

    bool operator==(const UserState& other) const
    {
        return userId == other.userId &&
               username == other.username &&
               email == other.email &&
               displayName == other.displayName &&
               bio == other.bio &&
               profilePictureUrl == other.profilePictureUrl &&
               authToken == other.authToken &&
               notificationSoundEnabled == other.notificationSoundEnabled &&
               osNotificationsEnabled == other.osNotificationsEnabled &&
               isLoggedIn == other.isLoggedIn &&
               isFetchingProfile == other.isFetchingProfile &&
               followerCount == other.followerCount &&
               followingCount == other.followingCount &&
               postCount == other.postCount;
    }
};

/**
 * UserStore - Reactive store for current user's profile and settings
 *
 * Replaces callback-based UserDataStore with reactive subscriptions.
 *
 * Features:
 * - Reactive state management: subscribe to profile changes
 * - Automatic profile image loading
 * - Persistent storage of credentials
 * - Profile update with optimistic UI
 *
 * Usage:
 *   // Get singleton instance
 *   auto& userStore = UserStore::getInstance();
 *   userStore.setNetworkClient(networkClient);
 *
 *   // Subscribe to state changes
 *   auto unsubscribe = userStore.subscribe([](const UserState& state) {
 *       if (state.isLoggedIn) {
 *           displayUserInfo(state.username, state.email);
 *           if (state.profileImage.isValid()) {
 *               displayAvatar(state.profileImage);
 *           }
 *       }
 *   });
 *
 *   // Set auth token (triggers profile fetch)
 *   userStore.setAuthToken(token);
 *
 *   // Update profile optimistically
 *   userStore.updateProfile(newUsername, newBio);
 */
class UserStore : public Store<UserState>
{
public:
    /**
     * Get singleton instance
     */
    static UserStore& getInstance()
    {
        static UserStore instance;
        return instance;
    }

    /**
     * Set the network client for API requests
     */
    void setNetworkClient(NetworkClient* client)
    {
        networkClient = client;
    }

    /**
     * Get the network client
     */
    NetworkClient* getNetworkClient() const { return networkClient; }

    //==========================================================================
    // Authentication

    /**
     * Set authentication token and load user profile
     * @param token The authentication token
     */
    void setAuthToken(const juce::String& token);

    /**
     * Clear authentication and user data (logout)
     */
    void clearAuthToken();

    /**
     * Check if user is logged in
     */
    bool isLoggedIn() const
    {
        return getState().isLoggedIn;
    }

    /**
     * Get current auth token
     */
    juce::String getAuthToken() const
    {
        return getState().authToken;
    }

    //==========================================================================
    // Profile Management

    /**
     * Fetch full user profile from server
     * @param forceRefresh If true, bypass cache
     */
    void fetchUserProfile(bool forceRefresh = false);

    /**
     * Update user profile (optimistic)
     * @param username New username (empty to keep current)
     * @param displayName New display name (empty to keep current)
     * @param bio New bio (empty to keep current)
     */
    void updateProfile(const juce::String& username = "",
                       const juce::String& displayName = "",
                       const juce::String& bio = "");

    /**
     * Set profile picture URL and download image
     * @param url Profile picture URL
     */
    void setProfilePictureUrl(const juce::String& url);

    /**
     * Set local preview image (while uploading)
     * @param imageFile Local image file
     */
    void setLocalPreviewImage(const juce::File& imageFile);

    /**
     * Refresh profile image from current URL
     */
    void refreshProfileImage();

    //==========================================================================
    // User Preferences

    /**
     * Set notification sound preference
     */
    void setNotificationSoundEnabled(bool enabled);

    /**
     * Set OS notifications preference
     */
    void setOSNotificationsEnabled(bool enabled);

    //==========================================================================
    // Social Metrics Updates

    /**
     * Update follower count (from real-time event)
     */
    void updateFollowerCount(int count);

    /**
     * Update following count
     */
    void updateFollowingCount(int count);

    /**
     * Update post count
     */
    void updatePostCount(int count);

    //==========================================================================
    // Persistence

    /**
     * Save user data to persistent storage
     */
    void saveToSettings();

    /**
     * Load user data from persistent storage
     */
    void loadFromSettings();

    /**
     * Clear all user data and cached images
     */
    void clearAll();

private:
    UserStore();
    ~UserStore() override = default;

    // Network client (not owned)
    NetworkClient* networkClient = nullptr;

    // Internal helpers
    void downloadProfileImage(const juce::String& url);
    void handleProfileFetchSuccess(const juce::var& data);
    void handleProfileFetchError(const juce::String& error);
    juce::PropertiesFile::Options getPropertiesOptions() const;

    // Singleton enforcement
    UserStore(const UserStore&) = delete;
    UserStore& operator=(const UserStore&) = delete;
};

}  // namespace Stores
}  // namespace Sidechain
