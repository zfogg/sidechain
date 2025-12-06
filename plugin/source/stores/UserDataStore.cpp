#include "UserDataStore.h"
#include "../util/Log.h"
#include "../network/NetworkClient.h"
#include "../util/Constants.h"
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
        // Construct proxy URL using constant: /api/v1/users/{userId}/profile-picture
        return juce::String(Constants::Endpoints::DEV_BASE_URL) + Constants::Endpoints::API_VERSION + "/users/" + userId + "/profile-picture";
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
            Log::debug("UserDataStore: Loaded local preview image");
            sendChangeMessage();
        }
    }
}

//==============================================================================
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
    Log::debug("UserDataStore: Saved settings");
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

        Log::debug("UserDataStore: Loaded settings - username: " + username);
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
    Log::info("UserDataStore: Cleared all user data");
}
