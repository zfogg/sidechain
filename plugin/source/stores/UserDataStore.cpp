#include "UserDataStore.h"
#include "../network/NetworkClient.h"
#include "../util/Json.h"
#include "../util/Async.h"
#include "../util/Validate.h"

//==============================================================================
UserDataStore::UserDataStore()
{
}

UserDataStore::~UserDataStore()
{
}

//==============================================================================
void UserDataStore::setAuthToken(const juce::String& token)
{
    authToken = token;
    sendChangeMessage();
}

void UserDataStore::clearAuthToken()
{
    authToken = "";
    sendChangeMessage();
}

void UserDataStore::setBasicUserInfo(const juce::String& user, const juce::String& mail)
{
    username = user;
    email = mail;
    sendChangeMessage();
}

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

juce::String UserDataStore::getProxyUrl() const
{
    // If we have a user ID, use the backend proxy endpoint
    if (userId.isNotEmpty() && networkClient != nullptr)
    {
        // Get base URL from network client config (e.g., http://localhost:8787)
        // Construct proxy URL: /api/v1/users/{userId}/profile-picture
        return "http://localhost:8787/api/v1/users/" + userId + "/profile-picture";
    }
    return "";
}

void UserDataStore::setLocalPreviewImage(const juce::File& imageFile)
{
    if (imageFile.existsAsFile())
    {
        cachedProfileImage = juce::ImageFileFormat::loadFrom(imageFile);
        if (cachedProfileImage.isValid())
        {
            DBG("UserDataStore: Loaded local preview image");
            sendChangeMessage();
        }
    }
}

//==============================================================================
void UserDataStore::downloadProfileImage(const juce::String& url)
{
    if (isDownloadingImage)
    {
        DBG("UserDataStore: Already downloading, skipping request for " + url);
        return;
    }

    isDownloadingImage = true;
    DBG("UserDataStore: *** Starting profile image download from " + url);

    // Capture auth token and current profile picture URL for the background thread
    // Note: We capture profilePictureUrl (not the download url) because we may be using
    // a proxy URL for download, but want to verify the original URL hasn't changed
    juce::String token = authToken;
    juce::String originalUrl = profilePictureUrl;

    // Use Async::run to download image on background thread
    Async::run<juce::Image>(
        // Background work: download and decode image
        [url, token]() -> juce::Image {
            DBG("UserDataStore: Download thread started");
            auto juceUrl = juce::URL(url);

            // Build headers - include auth token if we have one and this is a localhost URL (proxy)
            juce::String headers;
            if (token.isNotEmpty() && url.contains("localhost"))
            {
                headers = "Authorization: Bearer " + token;
                DBG("UserDataStore: Adding auth header for proxy request");
            }

            auto inputStream = juceUrl.createInputStream(
                juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                    .withExtraHeaders(headers)
                    .withConnectionTimeoutMs(10000)
                    .withNumRedirectsToFollow(5));

            if (inputStream != nullptr)
            {
                juce::MemoryBlock imageData;
                inputStream->readIntoMemoryBlock(imageData);
                DBG("UserDataStore: Downloaded " + juce::String((int)imageData.getSize()) + " bytes");

                return juce::ImageFileFormat::loadFrom(imageData.getData(), imageData.getSize());
            }

            DBG("UserDataStore: *** FAILED to create input stream for " + url);
            return {};
        },
        // Callback on message thread: update state
        [this, originalUrl](const juce::Image& image) {
            DBG("UserDataStore: Processing image on main thread");

            // Only update if the original URL hasn't changed while downloading
            // (we compare against originalUrl, not the download URL which may be a proxy)
            if (profilePictureUrl == originalUrl)
            {
                cachedProfileImage = image;

                if (cachedProfileImage.isValid())
                {
                    DBG("UserDataStore: *** Image loaded OK (" +
                        juce::String(cachedProfileImage.getWidth()) + "x" +
                        juce::String(cachedProfileImage.getHeight()) + ") - sending change message");
                }
                else
                {
                    DBG("UserDataStore: *** FAILED to decode image");
                }

                sendChangeMessage();
            }
            else
            {
                DBG("UserDataStore: URL changed during download, ignoring (orig=" + originalUrl + ", current=" + profilePictureUrl + ")");
            }

            isDownloadingImage = false;
        }
    );
}

void UserDataStore::refreshProfileImage()
{
    if (profilePictureUrl.isNotEmpty())
    {
        cachedProfileImage = juce::Image(); // Clear cached image
        downloadProfileImage(profilePictureUrl);
    }
}

//==============================================================================
void UserDataStore::fetchUserProfile(std::function<void(bool success)> callback)
{
    if (!networkClient || authToken.isEmpty())
    {
        DBG("UserDataStore: Cannot fetch profile - no network client or auth token");
        if (callback)
            callback(false);
        return;
    }

    DBG("UserDataStore: Fetching user profile from /api/v1/users/me");

    networkClient->get("/api/v1/users/me", [this, callback](bool success, const juce::var& response) {
        juce::MessageManager::callAsync([this, success, response, callback]() {
            if (success && Json::isObject(response))
            {
                // Update all user data from response
                userId = Json::getString(response, "id");
                username = Json::getString(response, "username");
                email = Json::getString(response, "email");
                displayName = Json::getString(response, "display_name");
                bio = Json::getString(response, "bio");
                location = Json::getString(response, "location");

                juce::String newPicUrl = Json::getString(response, "profile_picture_url");

                DBG("UserDataStore: Profile fetched - username: " + username +
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
                DBG("UserDataStore: Failed to fetch profile");
                if (callback)
                    callback(false);
            }
        });
    });
}

//==============================================================================
juce::PropertiesFile::Options UserDataStore::getPropertiesOptions() const
{
    juce::PropertiesFile::Options options;
    options.applicationName = "Sidechain";
    options.filenameSuffix = ".settings";
    options.folderName = "SidechainPlugin";
    return options;
}

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
    }
    else
    {
        appProperties->setValue("isLoggedIn", false);
    }

    appProperties->save();
    DBG("UserDataStore: Saved settings");
}

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

        DBG("UserDataStore: Loaded settings - username: " + username);
    }

    sendChangeMessage();
}

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
    DBG("UserDataStore: Cleared all user data");
}
