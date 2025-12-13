#include "Header.h"
#include "../../network/NetworkClient.h"
#include "../../util/Async.h"
#include "../../util/Constants.h"
#include "../../util/Colors.h"
#include "../../stores/ImageCache.h"
#include "../../util/Validate.h"
#include "../../util/UIHelpers.h"
#include "../../util/Log.h"

//==============================================================================
Header::Header()
{
    Log::info("Header: Initializing header component");
    setSize(1000, HEADER_HEIGHT);
    Log::info("Header: Initialization complete");
}

//==============================================================================
void Header::setNetworkClient(NetworkClient* client)
{
    networkClient = client;
    Log::info("Header::setNetworkClient: NetworkClient set " + juce::String(client != nullptr ? "(valid)" : "(null)"));
}

//==============================================================================
void Header::setUserInfo(const juce::String& user, const juce::String& picUrl)
{
    Log::info("Header::setUserInfo: Setting user info - username: " + user);
    username = user;

    // Only reload image if URL changed and we don't already have a cached image
    if (profilePicUrl != picUrl)
    {
        Log::debug("Header::setUserInfo: Profile picture URL changed - old: " + profilePicUrl + ", new: " + picUrl);
        profilePicUrl = picUrl;

        // Only download if we don't have an image and URL is valid
        if (!cachedProfileImage.isValid() && Validate::isUrl(profilePicUrl))
        {
            Log::debug("Header::setUserInfo: Loading profile image from URL");
            loadProfileImage(profilePicUrl);
        }
        else if (cachedProfileImage.isValid())
        {
            Log::debug("Header::setUserInfo: Using cached profile image");
        }
        else if (!Validate::isUrl(profilePicUrl))
        {
            Log::warn("Header::setUserInfo: Invalid profile picture URL: " + picUrl);
        }
    }
    else
    {
        Log::debug("Header::setUserInfo: Profile picture URL unchanged, skipping reload");
    }

    repaint();
}

void Header::setProfileImage(const juce::Image& image)
{
    if (image.isValid())
    {
        Log::info("Header::setProfileImage: Setting profile image directly - size: " + juce::String(image.getWidth()) + "x" + juce::String(image.getHeight()));
        cachedProfileImage = image;
    }
    else
    {
        Log::warn("Header::setProfileImage: Invalid image provided");
        cachedProfileImage = juce::Image();
    }
    repaint();
}

void Header::loadProfileImage(const juce::String& url)
{
    Log::info("Header::loadProfileImage: Loading profile image from: " + url);
    // Use Async::run to download image on background thread
    Async::run<juce::Image>(
        // Background work: download image
        [this, url]() -> juce::Image {
            Log::debug("Header::loadProfileImage: Starting download on background thread");
            juce::MemoryBlock imageData;
            bool success = false;

            // Use NetworkClient if available, otherwise fall back to JUCE URL
            if (networkClient != nullptr)
            {
                Log::debug("Header::loadProfileImage: Using NetworkClient for download");
                auto result = networkClient->makeAbsoluteRequestSync(url, "GET", juce::var(), false, juce::StringPairArray(), &imageData);
                success = result.success && imageData.getSize() > 0;
                if (success)
                {
                    Log::debug("Header::loadProfileImage: Download successful via NetworkClient - size: " + juce::String(imageData.getSize()) + " bytes");
                }
                else
                {
                    Log::warn("Header::loadProfileImage: Download failed via NetworkClient - error: " + result.errorMessage);
                }
            }
            else
            {
                Log::debug("Header::loadProfileImage: NetworkClient not available, using JUCE URL fallback");
                // Fallback to JUCE URL
                auto urlObj = juce::URL(url);
                auto inputStream = urlObj.createInputStream(
                    juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                        .withConnectionTimeoutMs(Constants::Api::QUICK_TIMEOUT_MS)
                        .withNumRedirectsToFollow(Constants::Api::MAX_REDIRECTS));

                if (inputStream != nullptr)
                {
                    inputStream->readIntoMemoryBlock(imageData);
                    success = imageData.getSize() > 0;
                    if (success)
                    {
                        Log::debug("Header::loadProfileImage: Download successful via JUCE URL - size: " + juce::String(imageData.getSize()) + " bytes");
                    }
                    else
                    {
                        Log::warn("Header::loadProfileImage: Download failed via JUCE URL - empty data");
                    }
                }
                else
                {
                    Log::error("Header::loadProfileImage: Failed to create input stream from URL");
                }
            }

            if (success)
            {
                auto image = juce::ImageFileFormat::loadFrom(imageData.getData(), imageData.getSize());
                if (image.isValid())
                {
                    Log::debug("Header::loadProfileImage: Image decoded successfully - size: " + juce::String(image.getWidth()) + "x" + juce::String(image.getHeight()));
                    return image;
                }
                else
                {
                    Log::error("Header::loadProfileImage: Failed to decode image from downloaded data");
                }
            }

            return {};
        },
        // Callback on message thread: update UI
        [this, url](const juce::Image& image) {
            if (image.isValid())
            {
                Log::info("Header::loadProfileImage: Profile image loaded successfully from: " + url);
                cachedProfileImage = image;
                repaint();
            }
            else
            {
                Log::error("Header::loadProfileImage: Failed to load profile image from: " + url);
            }
        }
    );
}

//==============================================================================
void Header::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    // Background
    g.setColour(SidechainColors::backgroundLight());
    g.fillRect(bounds);

    // Draw sections
    drawLogo(g, getLogoBounds());
    drawSearchButton(g, getSearchButtonBounds());
    drawRecordButton(g, getRecordButtonBounds());
    drawMessagesButton(g, getMessagesButtonBounds());
    drawStoryButton(g, getStoryButtonBounds());
    drawProfileSection(g, getProfileBounds());

    // Bottom border - use UIHelpers::drawDivider for consistency
    UIHelpers::drawDivider(g, 0, bounds.getBottom() - 1, bounds.getWidth(),
        SidechainColors::border(), 1.0f);
}

void Header::resized()
{
    Log::debug("Header::resized: Component resized to " + juce::String(getWidth()) + "x" + juce::String(getHeight()));
    // Layout is handled in getBounds methods
}

void Header::mouseUp(const juce::MouseEvent& event)
{
    auto pos = event.getPosition();
    Log::debug("Header::mouseUp: Mouse clicked at (" + juce::String(pos.x) + ", " + juce::String(pos.y) + ")");

    if (getLogoBounds().contains(pos))
    {
        Log::info("Header::mouseUp: Logo clicked");
        if (onLogoClicked)
            onLogoClicked();
        else
            Log::warn("Header::mouseUp: Logo clicked but callback not set");
    }
    else if (getSearchButtonBounds().contains(pos))
    {
        Log::info("Header::mouseUp: Search button clicked");
        if (onSearchClicked)
            onSearchClicked();
        else
            Log::warn("Header::mouseUp: Search clicked but callback not set");
    }
    else if (getRecordButtonBounds().contains(pos))
    {
        Log::info("Header::mouseUp: Record button clicked");
        if (onRecordClicked)
            onRecordClicked();
        else
            Log::warn("Header::mouseUp: Record clicked but callback not set");
    }
    else if (getMessagesButtonBounds().contains(pos))
    {
        Log::info("Header::mouseUp: Messages button clicked");
        if (onMessagesClicked)
            onMessagesClicked();
        else
            Log::warn("Header::mouseUp: Messages clicked but callback not set");
    }
    else if (getStoryButtonBounds().contains(pos))
    {
        Log::info("Header::mouseUp: Story button clicked");
        if (onStoryClicked)
            onStoryClicked();
        else
            Log::warn("Header::mouseUp: Story clicked but callback not set");
    }
    else if (getProfileBounds().contains(pos))
    {
        // Safely log username (avoid assertion failures with invalid strings)
        juce::String safeUsername = username.isNotEmpty() ? username : "(unknown)";
        Log::info("Header::mouseUp: Profile section clicked - username: " + safeUsername);
        
        // Check if profile picture area was clicked (for story viewing)
        auto picBounds = juce::Rectangle<int>(
            getProfileBounds().getX(),
            getProfileBounds().getCentreY() - 18,
            36, 36
        );
        
        if (picBounds.contains(pos) && hasStories)
        {
            // Profile picture clicked and user has stories - view story
            Log::info("Header::mouseUp: Profile picture clicked with stories - opening story viewer");
            if (onProfileStoryClicked)
                onProfileStoryClicked();
            else
                Log::warn("Header::mouseUp: Profile story clicked but callback not set");
        }
        else
        {
            // Regular profile click
            if (onProfileClicked)
                onProfileClicked();
            else
                Log::warn("Header::mouseUp: Profile clicked but callback not set");
        }
    }
}

//==============================================================================
void Header::drawLogo(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    g.setColour(SidechainColors::textPrimary());
    g.setFont(juce::Font(juce::FontOptions().withHeight(20.0f)).boldened());
    g.drawText("Sidechain", bounds, juce::Justification::centredLeft);
}

void Header::drawSearchButton(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    // Use UIHelpers::drawOutlineButton for consistent button styling
    UIHelpers::drawOutlineButton(g, bounds, "Search users...",
        SidechainColors::border(), SidechainColors::textMuted(), false, 8.0f);
}

void Header::drawRecordButton(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    // Record button is wired up in PluginEditor.cpp (line 355-356) - navigates to Recording screen
    // Use UIHelpers::drawButton for base button, then add custom recording dot
    UIHelpers::drawButton(g, bounds, "Record",
        SidechainColors::primary(), juce::Colours::white, false, 8.0f);

    // Draw red recording dot indicator
    auto dotBounds = bounds.withWidth(bounds.getHeight()).reduced(bounds.getHeight() / 3);
    dotBounds.setX(bounds.getX() + 12);
    g.setColour(juce::Colour(0xFFFF4444));  // Red recording indicator
    g.fillEllipse(dotBounds.toFloat());
}

void Header::drawMessagesButton(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    // Draw messages icon as a chat bubble (avoid emoji for Linux font compatibility)
    g.setColour(SidechainColors::textMuted());

    // Draw envelope-style icon
    auto iconBounds = bounds.withWidth(22).withHeight(16).withCentre(bounds.getCentre());
    g.drawRoundedRectangle(iconBounds.toFloat(), 2.0f, 1.5f);

    // Draw flap lines on envelope
    juce::Path flap;
    flap.startNewSubPath(iconBounds.getX(), iconBounds.getY());
    flap.lineTo(iconBounds.getCentreX(), iconBounds.getCentreY() - 2);
    flap.lineTo(iconBounds.getRight(), iconBounds.getY());
    g.strokePath(flap, juce::PathStrokeType(1.5f));

    // Draw unread badge if there are unread messages
    if (unreadMessageCount > 0)
    {
        int badgeSize = 16;
        auto badgeBounds = juce::Rectangle<int>(
            bounds.getX() + 20,
            bounds.getY() + 6,
            badgeSize,
            badgeSize
        );

        // Draw red badge background
        g.setColour(juce::Colour(0xFFFF4444));
        g.fillEllipse(badgeBounds.toFloat());

        // Draw badge text
        g.setColour(juce::Colours::white);
        g.setFont(10.0f);
        juce::String countText = unreadMessageCount > 99 ? "99+" : juce::String(unreadMessageCount);
        g.drawText(countText, badgeBounds, juce::Justification::centred);
    }
}

void Header::drawStoryButton(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    // Draw story icon as a circle with plus (avoid emoji for Linux font compatibility)
    g.setColour(SidechainColors::textMuted());

    auto iconBounds = bounds.withWidth(24).withHeight(24).withCentre(bounds.getCentre());
    g.drawEllipse(iconBounds.toFloat().reduced(2), 1.5f);

    // Draw plus sign inside
    auto center = iconBounds.getCentre();
    g.drawLine(center.x - 5, center.y, center.x + 5, center.y, 1.5f);
    g.drawLine(center.x, center.y - 5, center.x, center.y + 5, 1.5f);
}

void Header::drawProfileSection(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    // Profile picture
    auto picBounds = juce::Rectangle<int>(bounds.getX(), bounds.getCentreY() - 18, 36, 36);
    drawCircularProfilePic(g, picBounds);

    // Username
    g.setColour(SidechainColors::textPrimary());
    g.setFont(14.0f);
    auto textBounds = bounds.withX(picBounds.getRight() + 8).withWidth(bounds.getWidth() - 44);
    g.drawText(username, textBounds, juce::Justification::centredLeft);
}

void Header::drawCircularProfilePic(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    // Draw highlighted circle if user has stories (Instagram-style)
    if (hasStories)
    {
        // Outer gradient circle (highlighted)
        juce::ColourGradient gradient(
            juce::Colour(0xFFFF6B6B),  // Red
            0.0f, 0.0f,
            juce::Colour(0xFFFFD93D),  // Yellow
            1.0f, 1.0f,
            true
        );
        gradient.addColour(0.5f, juce::Colour(0xFFFF8E53));  // Orange

        g.setGradientFill(gradient);
        g.drawEllipse(bounds.toFloat().expanded(2.0f), 2.5f);
    }

    ImageLoader::drawCircularAvatar(g, bounds, cachedProfileImage,
        ImageLoader::getInitials(username),
        SidechainColors::primary(),
        SidechainColors::textPrimary(),
        14.0f);

    // Border (only if no story highlight)
    if (!hasStories)
    {
        g.setColour(SidechainColors::border());
        g.drawEllipse(bounds.toFloat().reduced(0.5f), 1.0f);
    }
}

//==============================================================================
juce::Rectangle<int> Header::getLogoBounds() const
{
    return juce::Rectangle<int>(20, 0, 120, getHeight());
}

juce::Rectangle<int> Header::getSearchButtonBounds() const
{
    int buttonWidth = 220;
    int buttonHeight = 36;
    int x = (getWidth() - buttonWidth) / 2;
    int y = (getHeight() - buttonHeight) / 2;
    return juce::Rectangle<int>(x, y, buttonWidth, buttonHeight);
}

juce::Rectangle<int> Header::getRecordButtonBounds() const
{
    // Position the record button between search and profile
    auto searchBounds = getSearchButtonBounds();
    int buttonWidth = 100;
    int buttonHeight = 36;
    int x = searchBounds.getRight() + 16;  // 16px gap after search
    int y = (getHeight() - buttonHeight) / 2;
    return juce::Rectangle<int>(x, y, buttonWidth, buttonHeight);
}

juce::Rectangle<int> Header::getMessagesButtonBounds() const
{
    // Position the messages button between record and story
    auto recordBounds = getRecordButtonBounds();
    int buttonWidth = 36;
    int buttonHeight = 36;
    int x = recordBounds.getRight() + 12;  // 12px gap after record
    int y = (getHeight() - buttonHeight) / 2;
    return juce::Rectangle<int>(x, y, buttonWidth, buttonHeight);
}

juce::Rectangle<int> Header::getStoryButtonBounds() const
{
    // Position the story button between messages and profile
    auto messagesBounds = getMessagesButtonBounds();
    int buttonWidth = 36;
    int buttonHeight = 36;
    int x = messagesBounds.getRight() + 12;  // 12px gap after messages
    int y = (getHeight() - buttonHeight) / 2;
    return juce::Rectangle<int>(x, y, buttonWidth, buttonHeight);
}

juce::Rectangle<int> Header::getProfileBounds() const
{
    return juce::Rectangle<int>(getWidth() - 160, 0, 140, getHeight());
}

void Header::setUnreadMessageCount(int count)
{
    if (unreadMessageCount != count)
    {
        unreadMessageCount = count;
        repaint();
    }
}

void Header::setHasStories(bool hasStoriesValue)
{
    if (hasStories != hasStoriesValue)
    {
        hasStories = hasStoriesValue;
        repaint();
    }
}
