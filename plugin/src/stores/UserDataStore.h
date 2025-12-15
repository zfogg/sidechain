#pragma once

#include <JuceHeader.h>
#include <thread>

class NetworkClient;

/**
 * UserDataStore - Centralized store for current user's profile data
 *
 * This class manages the logged-in user's data and provides:
 * - Cached profile image (downloaded from S3)
 * - User info (username, email, profile URL, etc.)
 * - Listener interface for components to react to data changes
 * - Async fetching from /api/v1/users/me endpoint
 */
class UserDataStore : public juce::ChangeBroadcaster {
public:
  /** Constructor */
  UserDataStore();

  /** Destructor */
  ~UserDataStore() override;

  //==========================================================================
  // Data access

  /** Get the current user's ID
   *  @return User ID string
   */
  const juce::String &getUserId() const {
    return userId;
  }

  /** Get the current user's username
   *  @return Username string
   */
  const juce::String &getUsername() const {
    return username;
  }

  /** Get the current user's email
   *  @return Email address string
   */
  const juce::String &getEmail() const {
    return email;
  }

  /** Get the current user's display name
   *  @return Display name string
   */
  const juce::String &getDisplayName() const {
    return displayName;
  }

  /** Get the current user's bio
   *  @return Bio text string
   */
  const juce::String &getBio() const {
    return bio;
  }

  /** Get the profile picture URL
   *  @return Profile picture URL string
   */
  const juce::String &getProfilePictureUrl() const {
    return profilePictureUrl;
  }

  /** Get the cached profile image
   *  @return Cached profile image (may be invalid if not loaded)
   */
  const juce::Image &getProfileImage() const {
    return cachedProfileImage;
  }

  /** Check if profile image is available
   *  @return true if profile image is valid and loaded
   */
  bool hasProfileImage() const {
    return cachedProfileImage.isValid();
  }

  /** Check if user is logged in
   *  @return true if authentication token is set
   */
  bool isLoggedIn() const {
    return !authToken.isEmpty();
  }

  /** Get the authentication token
   *  @return Authentication token string
   */
  const juce::String &getAuthToken() const {
    return authToken;
  }

  /** Check if notification sounds are enabled
   *  @return true if notification sounds should play
   */
  bool isNotificationSoundEnabled() const {
    return notificationSoundEnabled;
  }

  /** Set notification sound preference
   *  @param enabled Whether to play sounds for notifications
   */
  void setNotificationSoundEnabled(bool enabled);

  /** Check if OS notifications are enabled
   *  @return true if OS notifications should be shown
   */
  bool isOSNotificationsEnabled() const {
    return osNotificationsEnabled;
  }

  /** Set OS notifications preference
   *  @param enabled Whether to show OS notifications
   */
  void setOSNotificationsEnabled(bool enabled);

  //==========================================================================
  // Data modification

  /** Set the network client for API requests
   *  @param client Pointer to NetworkClient instance
   */
  void setNetworkClient(NetworkClient *client) {
    networkClient = client;
  }

  /** Set the authentication token
   *  @param token Authentication token string
   */
  void setAuthToken(const juce::String &token);

  /** Clear the authentication token and user data */
  void clearAuthToken();

  /** Set basic user info (from login response)
   *  @param user Username
   *  @param mail Email address
   */
  void setBasicUserInfo(const juce::String &user, const juce::String &mail);

  /** Set profile picture URL and start async download
   *  @param url Profile picture URL
   */
  void setProfilePictureUrl(const juce::String &url);

  /** Set a local image for immediate preview (while uploading)
   *  @param imageFile Local image file to use as preview
   */
  void setLocalPreviewImage(const juce::File &imageFile);

  //==========================================================================
  // Network operations

  /** Fetch full profile from /api/v1/users/me
   *  @param callback Optional callback called when fetch completes
   */
  void fetchUserProfile(std::function<void(bool success)> callback = nullptr);

  /** Refresh profile picture from URL
   *  Downloads the profile image again from the current URL
   */
  void refreshProfileImage();

  //==========================================================================
  // Persistence

  /** Save user data to persistent storage */
  void saveToSettings();

  /** Load user data from persistent storage */
  void loadFromSettings();

  /** Clear all user data and cached images */
  void clearAll();

private:
  //==========================================================================
  // User data
  juce::String userId;
  juce::String username;
  juce::String email;
  juce::String displayName;
  juce::String bio;
  juce::String location;
  juce::String profilePictureUrl;
  juce::String authToken;
  bool notificationSoundEnabled = true; // Default: enabled
  bool osNotificationsEnabled = true;   // Default: enabled

  // Cached profile image
  juce::Image cachedProfileImage;
  bool isDownloadingImage = false;

  // Network client (not owned)
  NetworkClient *networkClient = nullptr;

  //==========================================================================
  // Helpers
  void downloadProfileImage(const juce::String &url);
  juce::String getProxyUrl() const;
  juce::PropertiesFile::Options getPropertiesOptions() const;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UserDataStore)
};
