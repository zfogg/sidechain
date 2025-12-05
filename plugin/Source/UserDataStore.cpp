#include "UserDataStore.h"
#include "network/NetworkClient.h"
#include <thread>

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
    DBG("UserDataStore::setProfilePictureUrl called with: " + url);
    DBG("  - current profilePictureUrl: " + profilePictureUrl);
    DBG("  - cachedProfileImage.isValid: " + juce::String(cachedProfileImage.isValid() ? "true" : "false"));

    if (url == profilePictureUrl && cachedProfileImage.isValid())
    {
        DBG("  - EARLY RETURN: url matches and image is valid");
        return; // No change needed
    }

    profilePictureUrl = url;

    if (url.isNotEmpty() && url.startsWith("http"))
    {
        // Use proxy endpoint instead of direct S3 URL to work around JUCE SSL issues on Linux
        juce::String downloadUrl = getProxyUrl();
        DBG("  - getProxyUrl returned: " + downloadUrl);
        DBG("  - userId: " + userId);
        DBG("  - networkClient: " + juce::String(networkClient != nullptr ? "SET" : "NULL"));

        if (downloadUrl.isNotEmpty())
        {
            DBG("  - calling downloadProfileImage with proxy URL");
            downloadProfileImage(downloadUrl);
        }
        else
        {
            DBG("  - calling downloadProfileImage with direct URL (fallback)");
            // Fallback to direct URL
            downloadProfileImage(url);
        }
    }
    else
    {
        DBG("  - NOT downloading: url empty or doesn't start with http");
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

    std::thread([this, url, token, originalUrl]() {
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

            juce::MessageManager::callAsync([this, imageData, originalUrl]() {
                DBG("UserDataStore: Processing image on main thread");
                // Only update if the original URL hasn't changed while downloading
                // (we compare against originalUrl, not the download URL which may be a proxy)
                if (profilePictureUrl == originalUrl)
                {
                    cachedProfileImage = juce::ImageFileFormat::loadFrom(
                        imageData.getData(), imageData.getSize());

                    if (cachedProfileImage.isValid())
                    {
                        DBG("UserDataStore: *** Image loaded OK (" +
                            juce::String(cachedProfileImage.getWidth()) + "x" +
                            juce::String(cachedProfileImage.getHeight()) + ") - sending change message");
                    }
                    else
                    {
                        DBG("UserDataStore: *** FAILED to decode image (bytes=" +
                            juce::String((int)imageData.getSize()) + ")");
                    }

                    sendChangeMessage();
                }
                else
                {
                    DBG("UserDataStore: URL changed during download, ignoring (orig=" + originalUrl + ", current=" + profilePictureUrl + ")");
                }
                isDownloadingImage = false;
            });
        }
        else
        {
            DBG("UserDataStore: *** FAILED to create input stream for " + url);
            juce::MessageManager::callAsync([this]() {
                isDownloadingImage = false;
            });
        }
    }).detach();
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
            if (success && response.isObject())
            {
                // Update all user data from response
                userId = response.getProperty("id", "").toString();
                username = response.getProperty("username", "").toString();
                email = response.getProperty("email", "").toString();
                displayName = response.getProperty("display_name", "").toString();
                bio = response.getProperty("bio", "").toString();
                location = response.getProperty("location", "").toString();

                juce::String newPicUrl = response.getProperty("profile_picture_url", "").toString();

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
    DBG("UserDataStore::loadFromSettings CALLED");
    DBG("  - networkClient at entry: " + juce::String(networkClient != nullptr ? "SET" : "NULL"));

    auto appProperties = std::make_unique<juce::PropertiesFile>(getPropertiesOptions());

    bool isLoggedIn = appProperties->getBoolValue("isLoggedIn", false);
    DBG("  - isLoggedIn: " + juce::String(isLoggedIn ? "true" : "false"));

    if (isLoggedIn)
    {
        userId = appProperties->getValue("userId", "");
        username = appProperties->getValue("username", "");
        email = appProperties->getValue("email", "");
        displayName = appProperties->getValue("displayName", "");
        authToken = appProperties->getValue("authToken", "");

        DBG("  - userId loaded: " + userId);
        DBG("  - username loaded: " + username);

        juce::String savedPicUrl = appProperties->getValue("profilePicUrl", "");
        DBG("  - savedPicUrl: " + savedPicUrl);

        if (savedPicUrl.isNotEmpty())
        {
            DBG("  - about to call setProfilePictureUrl");
            setProfilePictureUrl(savedPicUrl);
        }
        else
        {
            DBG("  - savedPicUrl is empty, not calling setProfilePictureUrl");
        }

        DBG("UserDataStore: Loaded settings - username: " + username +
            ", profilePicUrl: " + savedPicUrl);
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
