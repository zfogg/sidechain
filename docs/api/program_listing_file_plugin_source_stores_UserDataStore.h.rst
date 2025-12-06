
.. _program_listing_file_plugin_source_stores_UserDataStore.h:

Program Listing for File UserDataStore.h
========================================

|exhale_lsh| :ref:`Return to documentation for file <file_plugin_source_stores_UserDataStore.h>` (``plugin/source/stores/UserDataStore.h``)

.. |exhale_lsh| unicode:: U+021B0 .. UPWARDS ARROW WITH TIP LEFTWARDS

.. code-block:: cpp

   #pragma once
   
   #include <JuceHeader.h>
   #include <thread>
   
   class NetworkClient;
   
   class UserDataStore : public juce::ChangeBroadcaster
   {
   public:
       UserDataStore();
       ~UserDataStore() override;
   
       //==========================================================================
       // Data access
       const juce::String& getUserId() const { return userId; }
       const juce::String& getUsername() const { return username; }
       const juce::String& getEmail() const { return email; }
       const juce::String& getDisplayName() const { return displayName; }
       const juce::String& getBio() const { return bio; }
       const juce::String& getProfilePictureUrl() const { return profilePictureUrl; }
       const juce::Image& getProfileImage() const { return cachedProfileImage; }
       bool hasProfileImage() const { return cachedProfileImage.isValid(); }
       bool isLoggedIn() const { return !authToken.isEmpty(); }
       const juce::String& getAuthToken() const { return authToken; }
   
       //==========================================================================
       // Data modification
       void setNetworkClient(NetworkClient* client) { networkClient = client; }
       void setAuthToken(const juce::String& token);
       void clearAuthToken();
   
       // Set basic user info (from login response)
       void setBasicUserInfo(const juce::String& user, const juce::String& mail);
   
       // Set profile picture URL and start async download
       void setProfilePictureUrl(const juce::String& url);
   
       // Set a local image for immediate preview (while uploading)
       void setLocalPreviewImage(const juce::File& imageFile);
   
       //==========================================================================
       // Network operations
   
       // Fetch full profile from /api/v1/users/me
       void fetchUserProfile(std::function<void(bool success)> callback = nullptr);
   
       // Refresh profile picture from URL
       void refreshProfileImage();
   
       //==========================================================================
       // Persistence
       void saveToSettings();
       void loadFromSettings();
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
   
       // Cached profile image
       juce::Image cachedProfileImage;
       bool isDownloadingImage = false;
   
       // Network client (not owned)
       NetworkClient* networkClient = nullptr;
   
       //==========================================================================
       // Helpers
       void downloadProfileImage(const juce::String& url);
       juce::String getProxyUrl() const;
       juce::PropertiesFile::Options getPropertiesOptions() const;
   
       JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UserDataStore)
   };
