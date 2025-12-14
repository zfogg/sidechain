#include "UserDataStore.h"
#include "../util/Log.h"
#include "../network/NetworkClient.h"
#include "../util/Constants.h"
#include "../util/Json.h"
#include "../util/Async.h"
#include "../util/Validate.h"

//==============================================================================
/** Construct UserDataStore
 * Initializes the user data store with empty values
 */
UserDataStore::UserDataStore()
{
}

/** Destructor
 */
UserDataStore::~UserDataStore()
{
}

//==============================================================================
/** Set the authentication token
 * Updates the stored auth token and notifies listeners
 * @param token Authentication token string
 */
void UserDataStore::setAuthToken(const juce::String& token)
{
    authToken = token;
    sendChangeMessage();
}

/** Clear the authentication token and user data
 * Removes auth token and notifies listeners
 */
void UserDataStore::clearAuthToken()
{
    authToken = "";
    sendChangeMessage();
}

/** Set basic user info (from login response)
 * Sets username and email from authentication response
 * @param user Username
 * @param mail Email address
 */
void UserDataStore::setBasicUserInfo(const juce::String& user, const juce::String& mail)
{
    username = user;
    email = mail;
    sendChangeMessage();
}

/** Set notification sound preference
 * Updates the notification sound setting and saves to preferences
 * @param enabled Whether to play sounds for notifications
 */
void UserDataStore::setNotificationSoundEnabled(bool enabled)
{
    notificationSoundEnabled = enabled;
    saveToSettings();
    sendChangeMessage();
}

/** Set OS notifications preference
 * Updates the OS notifications setting and saves to preferences
 * @param enabled Whether to show OS notifications
 */
void UserDataStore::setOSNotificationsEnabled(bool enabled)
{
    osNotificationsEnabled = enabled;
    saveToSettings();
    sendChangeMessage();
}

/** Set profile picture URL and start async download
 * Downloads the profile image from the URL in the background
 * @param url Profile picture URL
 */
void UserDataStore::setProfilePictureUrl(const juce::String& url)
{
    if (url == profilePictureUrl && cachedProfileImage.isValid())
        return; // No change needed

    profilePictureUrl = url;

    if (Validate::isUrl(url))
    {
        // Use proxy endpoint instead of direct S3 URL to work around JUCE SSL issues on Linux
        juce::String downloadUrl = getProxyUrl();
        if (downloadUrl.isNotEmpty())
        {
            downloadProfileImage(downloadUrl);
        }
        else
        {
            // Fallback to direct URL
            downloadProfileImage(url);
        }
    }

    sendChangeMessage();
}

/** Get proxy URL for profile picture download
 * Returns backend proxy endpoint URL to work around SSL issues on Linux
 * @return Proxy URL if user ID is available, empty string otherwise
 */
juce::String UserDataStore::getProxyUrl() const
{
    // If we have a user ID, use the backend proxy endpoint
    if (userId.isNotEmpty() && networkClient != nullptr)
    {
        // Construct proxy URL using constant: /api/v1/users/{userId}/profile-picture?proxy=true
        // The ?proxy=true tells the backend to return actual image bytes instead of JSON
        return juce::String(Constants::Endpoints::DEV_BASE_URL) + Constants::Endpoints::API_VERSION + "/users/" + userId + "/profile-picture?proxy=true";
    }
    return "";
}

/** Set a local image for immediate preview (while uploading)
 * Loads an image file directly from disk for immediate display
 * @param imageFile Local image file to use as preview
 */
void UserDataStore::setLocalPreviewImage(const juce::File& imageFile)
{
    if (imageFile.existsAsFile())
    {
        cachedProfileImage = juce::ImageFileFormat::loadFrom(imageFile);
        if (cachedProfileImage.isValid())
        {
            Log::debug("UserDataStore: Loaded local preview image");
            sendChangeMessage();
        }
    }
}

//==============================================================================
/** Download profile image from URL in background
 * Downloads the image asynchronously and updates cachedProfileImage when complete
 * @param url URL to download image from
 */
void UserDataStore::downloadProfileImage(const juce::String& url)
{
    if (isDownloadingImage)
    {
        Log::debug("UserDataStore: Already downloading, skipping request for " + url);
        return;
    }

    isDownloadingImage = true;
    Log::info("UserDataStore: Starting profile image download from " + url);

    // Capture auth token and current profile picture URL for the background thread
    // Note: We capture profilePictureUrl (not the download url) because we may be using
    // a proxy URL for download, but want to verify the original URL hasn't changed
    juce::String token = authToken;
    juce::String originalUrl = profilePictureUrl;
    NetworkClient* client = networkClient;  // Capture pointer locally to avoid 'this' capture issues

    // Create function objects for Async::run
    std::function<juce::Image()> workFunc = [client, url, token]() -> juce::Image {
            Log::debug("UserDataStore: Download thread started");
            juce::MemoryBlock imageData;
            bool success = false;

            // Use NetworkClient if available, otherwise fall back to JUCE URL
            if (client != nullptr)
            {
                juce::StringPairArray headers;
                if (token.isNotEmpty() && url.contains("localhost"))
                {
                    headers.set("Authorization", "Bearer " + token);
                    Log::debug("UserDataStore: Adding auth header for proxy request");
                }

                auto result = client->makeAbsoluteRequestSync(url, "GET", juce::var(), false, headers, &imageData);
                success = result.success && imageData.getSize() > 0;
            }
            else
            {
                // Fallback to JUCE URL
                auto juceUrl = juce::URL(url);
                juce::String headersStr;
                if (token.isNotEmpty() && url.contains("localhost"))
                {
                    headersStr = "Authorization: Bearer " + token;
                    Log::debug("UserDataStore: Adding auth header for proxy request");
                }

                auto inputStream = juceUrl.createInputStream(
                    juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                        .withExtraHeaders(headersStr)
                        .withConnectionTimeoutMs(Constants::Api::IMAGE_TIMEOUT_MS)
                        .withNumRedirectsToFollow(Constants::Api::MAX_REDIRECTS));

                if (inputStream != nullptr)
                {
                    inputStream->readIntoMemoryBlock(imageData);
                    success = imageData.getSize() > 0;
                }
            }

            if (success)
            {
                Log::debug("UserDataStore: Downloaded " + juce::String((int)imageData.getSize()) + " bytes");
                return juce::ImageFileFormat::loadFrom(imageData.getData(), imageData.getSize());
            }

            Log::error("UserDataStore: Failed to download image from " + url);
            return juce::Image();
        };

    std::function<void(juce::Image)> completeFunc = [this, originalUrl](const juce::Image& image) {
            Log::debug("UserDataStore: Processing image on main thread");

            // Only update if the original URL hasn't changed while downloading
            // (we compare against originalUrl, not the download URL which may be a proxy)
            if (profilePictureUrl == originalUrl)
            {
                cachedProfileImage = image;

                if (cachedProfileImage.isValid())
                {
                    Log::info("UserDataStore: Image loaded OK (" +
                        juce::String(cachedProfileImage.getWidth()) + "x" +
                        juce::String(cachedProfileImage.getHeight()) + ") - sending change message");
                }
                else
                {
                    Log::error("UserDataStore: Failed to decode image");
                }

                sendChangeMessage();
            }
            else
            {
                Log::warn("UserDataStore: URL changed during download, ignoring (orig=" + originalUrl + ", current=" + profilePictureUrl + ")");
            }

            isDownloadingImage = false;
        };

    // Use Async::run to download image on background thread
    Async::run<juce::Image>(std::move(workFunc), std::move(completeFunc));
}

/** Refresh profile picture from URL
 * Clears cached image and downloads again from the current URL
 */
void UserDataStore::refreshProfileImage()
{
    if (profilePictureUrl.isNotEmpty())
    {
        cachedProfileImage = juce::Image(); // Clear cached image
        downloadProfileImage(profilePictureUrl);
    }
}

//==============================================================================
/** Fetch full profile from /api/v1/users/me
 * Downloads complete user profile data from the backend API
 * @param callback Optional callback called when fetch completes (true if successful)
 */
void UserDataStore::fetchUserProfile(std::function<void(bool success)> callback)
{
    if (!networkClient || authToken.isEmpty())
    {
        Log::warn("UserDataStore: Cannot fetch profile - no network client or auth token");
        if (callback)
            callback(false);
        return;
    }

    Log::info("UserDataStore: Fetching user profile from /api/v1/users/me");

    networkClient->get("/api/v1/users/me", [this, callback](Outcome<juce::var> result) {
        juce::MessageManager::callAsync([this, result, callback]() {
            if (result.isOk() && Json::isObject(result.getValue()))
            {
                auto response = result.getValue();
                // Update all user data from response
                userId = Json::getString(response, "id");
                username = Json::getString(response, "username");
                email = Json::getString(response, "email");
                displayName = Json::getString(response, "display_name");
                bio = Json::getString(response, "bio");
                location = Json::getString(response, "location");

                juce::String newPicUrl = Json::getString(response, "profile_picture_url");

                Log::info("UserDataStore: Profile fetched - username: " + username +
                    ", profilePicUrl: " + newPicUrl);

                // Update profile picture if URL changed
                if (newPicUrl != profilePictureUrl || !cachedProfileImage.isValid())
                {
                    setProfilePictureUrl(newPicUrl);
                }

                saveToSettings();
                sendChangeMessage();

                if (callback)
                    callback(true);
            }
            else
            {
                Log::error("UserDataStore: Failed to fetch profile");
                if (callback)
                    callback(false);
            }
        });
    });
}

//==============================================================================
/** Get properties file options for persistence
 * @return PropertiesFile options configured for Sidechain plugin
 */
juce::PropertiesFile::Options UserDataStore::getPropertiesOptions() const
{
    juce::PropertiesFile::Options options;
    options.applicationName = "Sidechain";
    options.filenameSuffix = ".settings";
    options.folderName = "SidechainPlugin";
    return options;
}

/** Save user data to persistent storage
 * Writes user data to platform-specific settings file
 */
void UserDataStore::saveToSettings()
{
    auto appProperties = std::make_unique<juce::PropertiesFile>(getPropertiesOptions());

    if (!username.isEmpty())
    {
        appProperties->setValue("isLoggedIn", true);
        appProperties->setValue("userId", userId);
        appProperties->setValue("username", username);
        appProperties->setValue("email", email);
        appProperties->setValue("displayName", displayName);
        appProperties->setValue("profilePicUrl", profilePictureUrl);
        appProperties->setValue("authToken", authToken);
        appProperties->setValue("notificationSoundEnabled", notificationSoundEnabled);
        appProperties->setValue("osNotificationsEnabled", osNotificationsEnabled);
    }
    else
    {
        appProperties->setValue("isLoggedIn", false);
    }

    appProperties->save();
    Log::debug("UserDataStore: Saved settings");
}

/** Load user data from persistent storage
 * Reads user data from platform-specific settings file
 */
void UserDataStore::loadFromSettings()
{
    auto appProperties = std::make_unique<juce::PropertiesFile>(getPropertiesOptions());

    bool isLoggedIn = appProperties->getBoolValue("isLoggedIn", false);

    if (isLoggedIn)
    {
        userId = appProperties->getValue("userId", "");
        username = appProperties->getValue("username", "");
        email = appProperties->getValue("email", "");
        displayName = appProperties->getValue("displayName", "");
        authToken = appProperties->getValue("authToken", "");

        juce::String savedPicUrl = appProperties->getValue("profilePicUrl", "");
        if (savedPicUrl.isNotEmpty())
        {
            setProfilePictureUrl(savedPicUrl);
        }

        // Load notification sound preference (default: enabled)
        notificationSoundEnabled = appProperties->getBoolValue("notificationSoundEnabled", true);
        
        // Load OS notifications preference (default: enabled)
        osNotificationsEnabled = appProperties->getBoolValue("osNotificationsEnabled", true);

        Log::debug("UserDataStore: Loaded settings - username: " + username);
    }

    sendChangeMessage();
}

/** Clear all user data and cached images
 * Removes all stored user information and cached profile image
 */
void UserDataStore::clearAll()
{
    userId = "";
    username = "";
    email = "";
    displayName = "";
    bio = "";
    location = "";
    profilePictureUrl = "";
    authToken = "";
    cachedProfileImage = juce::Image();

    auto appProperties = std::make_unique<juce::PropertiesFile>(getPropertiesOptions());
    appProperties->setValue("isLoggedIn", false);
    appProperties->removeValue("userId");
    appProperties->removeValue("username");
    appProperties->removeValue("email");
    appProperties->removeValue("displayName");
    appProperties->removeValue("profilePicUrl");
    appProperties->removeValue("authToken");
    appProperties->save();

    sendChangeMessage();
    Log::info("UserDataStore: Cleared all user data");
}
