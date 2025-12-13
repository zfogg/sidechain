#include "Profile.h"
#include "../../network/NetworkClient.h"
#include "../../network/StreamChatClient.h"
#include "../../util/Json.h"
#include "../../util/Result.h"
#include "../../stores/ImageCache.h"
#include "../../util/Log.h"
#include "../../util/StringFormatter.h"
#include "../../util/Colors.h"
#include "../feed/PostCard.h"
#include <vector>

//==============================================================================
// UserProfile implementation
//==============================================================================
UserProfile UserProfile::fromJson(const juce::var& json)
{
    UserProfile profile;

    if (!Json::isObject(json))
    {
        Log::warn("UserProfile::fromJson: Invalid JSON - not an object");
        return profile;
    }

    profile.id = Json::getString(json, "id");
    profile.username = Json::getString(json, "username");
    profile.displayName = Json::getString(json, "display_name");
    profile.bio = Json::getString(json, "bio");
    profile.location = Json::getString(json, "location");
    profile.avatarUrl = Json::getString(json, "avatar_url");
    profile.profilePictureUrl = Json::getString(json, "profile_picture_url");
    profile.dawPreference = Json::getString(json, "daw_preference");
    profile.genre = Json::getString(json, "genre");
    profile.socialLinks = Json::getObject(json, "social_links");
    profile.followerCount = Json::getInt(json, "follower_count");
    profile.followingCount = Json::getInt(json, "following_count");
    profile.postCount = Json::getInt(json, "post_count");
    profile.isFollowing = Json::getBool(json, "is_following");
    profile.isFollowedBy = Json::getBool(json, "is_followed_by");
    profile.isPrivate = Json::getBool(json, "is_private");
    profile.followRequestStatus = Json::getString(json, "follow_request_status");
    profile.followRequestId = Json::getString(json, "follow_request_id");

    // Parse created_at timestamp
    juce::String createdAtStr = Json::getString(json, "created_at");
    if (createdAtStr.isNotEmpty())
    {
        // Try to parse ISO 8601 format
        profile.createdAt = juce::Time::fromISO8601(createdAtStr);
        if (profile.createdAt.toMilliseconds() == 0)
            Log::warn("UserProfile::fromJson: Failed to parse created_at timestamp: " + createdAtStr);
    }

    Log::debug("UserProfile::fromJson: Parsed profile - id: " + profile.id + ", username: " + profile.username);
    return profile;
}

juce::String UserProfile::getAvatarUrl() const
{
    // Backend returns avatar_url as the effective URL (prefers profile_picture_url over oauth)
    // Keep this logic for backwards compatibility
    if (profilePictureUrl.isNotEmpty())
        return profilePictureUrl;
    return avatarUrl;
}

juce::String UserProfile::getMemberSince() const
{
    if (createdAt.toMilliseconds() == 0)
        return "";

    // Format as "Member since Month Year"
    return "Member since " + createdAt.toString(false, false, false, true).substring(0, 3) + " " +
           juce::String(createdAt.getYear());
}

bool UserProfile::isOwnProfile(const juce::String& currentUserId) const
{
    return id == currentUserId;
}

//==============================================================================
// Profile implementation
//==============================================================================
Profile::Profile()
{
    Log::info("Profile: Initializing profile component");

    scrollBar = std::make_unique<juce::ScrollBar>(true);
    scrollBar->addListener(this);
    scrollBar->setAutoHide(true);
    addAndMakeVisible(scrollBar.get());
    Log::debug("Profile: Scroll bar created and added");

    // Create story highlights component
    storyHighlights = std::make_unique<StoryHighlights>();
    storyHighlights->onHighlightClicked = [this](const StoryHighlight& highlight) {
        Log::info("Profile: Highlight clicked - id: " + highlight.id + ", name: " + highlight.name);
        if (onHighlightClicked)
            onHighlightClicked(highlight);
    };
    storyHighlights->onCreateHighlightClicked = [this]() {
        Log::info("Profile: Create highlight clicked");
        if (onCreateHighlightClicked)
            onCreateHighlightClicked();
    };
    addAndMakeVisible(storyHighlights.get());
    Log::debug("Profile: Story highlights component created");

    // Create followers list panel (initially hidden)
    followersListPanel = std::make_unique<FollowersList>();
    followersListPanel->onClose = [this]() {
        Log::debug("Profile: Followers list close requested");
        hideFollowersList();
    };
    followersListPanel->onUserClicked = [this](const juce::String& userId) {
        Log::info("Profile: User clicked in followers list - userId: " + userId);
        hideFollowersList();
        // Navigate to the clicked user's profile
        loadProfile(userId);
    };
    addChildComponent(followersListPanel.get());
    Log::debug("Profile: Followers list panel created");

    // Create error state component (initially hidden)
    errorStateComponent = std::make_unique<ErrorState>();
    errorStateComponent->setErrorType(ErrorState::ErrorType::Network);
    errorStateComponent->setPrimaryAction("Try Again", [this]() {
        Log::info("Profile: Retry requested from error state");
        if (profile.id.isNotEmpty())
            loadProfile(profile.id);
        else if (currentUserId.isNotEmpty())
            loadOwnProfile();
    });
    errorStateComponent->setSecondaryAction("Go Back", [this]() {
        Log::info("Profile: Go back requested from error state");
        if (onBackPressed)
            onBackPressed();
    });
    addChildComponent(errorStateComponent.get());
    Log::debug("Profile: Error state component created");

    // IMPORTANT: setSize must be called LAST because it triggers resized()
    // which uses scrollBar and other child components
    setSize(600, 800);
    Log::info("Profile: Initialization complete");
}

Profile::~Profile()
{
    Log::debug("Profile: Destroying profile component");
    scrollBar->removeListener(this);
}

//==============================================================================
void Profile::setNetworkClient(NetworkClient* client)
{
    networkClient = client;
    if (storyHighlights)
        storyHighlights->setNetworkClient(client);
    Log::info("Profile: NetworkClient set " + juce::String(client != nullptr ? "(valid)" : "(null)"));
}

void Profile::setStreamChatClient(StreamChatClient* client)
{
    streamChatClient = client;
    Log::info("Profile::setStreamChatClient: StreamChatClient set " + juce::String(client != nullptr ? "(valid)" : "(null)"));
}

void Profile::setCurrentUserId(const juce::String& userId)
{
    currentUserId = userId;
    Log::info("Profile: Current user ID set to: " + userId);
}

//==============================================================================
void Profile::loadProfile(const juce::String& userId)
{
    if (userId.isEmpty())
    {
        Log::warn("Profile::loadProfile: Empty userId provided");
        return;
    }

    Log::info("Profile::loadProfile: Loading profile for userId: " + userId);
    isLoading = true;
    hasError = false;
    errorMessage = "";
    profile = UserProfile();
    userPosts.clear();
    postCards.clear();
    repaint();

    fetchProfile(userId);
}

void Profile::loadOwnProfile()
{
    if (currentUserId.isEmpty())
    {
        Log::warn("Profile::loadOwnProfile: currentUserId is empty");
        return;
    }

    Log::info("Profile::loadOwnProfile: Loading own profile - userId: " + currentUserId);
    loadProfile(currentUserId);
}

void Profile::setProfile(const UserProfile& newProfile)
{
    Log::info("Profile::setProfile: Setting profile - id: " + newProfile.id + ", username: " + newProfile.username);
    profile = newProfile;
    isLoading = false;
    hasError = false;
    avatarImage = juce::Image();

    // Hide error state component on successful load
    if (errorStateComponent != nullptr)
        errorStateComponent->setVisible(false);

    // Load avatar via backend proxy to work around JUCE SSL/redirect issues on Linux
    // The backend downloads the image from S3/OAuth and relays the raw bytes
    if (profile.id.isNotEmpty())
    {
        Log::debug("Profile::setProfile: Loading avatar via proxy for user: " + profile.id);
        ImageLoader::loadAvatarForUser(profile.id, [this](const juce::Image& img) {
            if (img.isValid())
            {
                Log::debug("Profile::setProfile: Avatar loaded successfully via proxy");
                avatarImage = img;
            }
            else
            {
                Log::warn("Profile::setProfile: Failed to load avatar image via proxy");
            }
            repaint();
        });
    }
    else
    {
        Log::debug("Profile::setProfile: No profile ID available for avatar");
    }

    repaint();

    // Fetch user's posts
    if (profile.id.isNotEmpty())
    {
        Log::debug("Profile::setProfile: Fetching user posts for: " + profile.id);
        fetchUserPosts(profile.id);

        // Check if user has active stories
        checkForActiveStories(profile.id);

        // Load story highlights
        if (storyHighlights)
        {
            storyHighlights->setUserId(profile.id);
            storyHighlights->setIsOwnProfile(profile.isOwnProfile(currentUserId));
            storyHighlights->loadHighlights();
            Log::debug("Profile::setProfile: Loading story highlights for user: " + profile.id);
        }

        // Query presence for this user (if not own profile)
        if (!profile.isOwnProfile(currentUserId))
        {
            queryPresenceForProfile();
        }
    }
    else
    {
        Log::warn("Profile::setProfile: Profile ID is empty, skipping post fetch");
    }
}

void Profile::refresh()
{
    if (profile.id.isNotEmpty())
    {
        Log::info("Profile::refresh: Refreshing profile - id: " + profile.id);
        loadProfile(profile.id);
    }
    else
    {
        Log::warn("Profile::refresh: Cannot refresh - profile ID is empty");
    }
}

//==============================================================================
void Profile::paint(juce::Graphics& g)
{
    drawBackground(g);

    if (isLoading)
    {
        drawLoadingState(g);
        return;
    }

    if (hasError)
    {
        // ErrorState component handles the error UI as a child component
        // Just ensure background is drawn (already done above)
        return;
    }

    // Draw header section
    auto headerBounds = getLocalBounds().removeFromTop(HEADER_HEIGHT);
    drawHeader(g, headerBounds);

    // Draw posts section
    auto postsBounds = getPostsAreaBounds();
    if (userPosts.isEmpty())
    {
        drawEmptyState(g, postsBounds);
    }
}

void Profile::drawBackground(juce::Graphics& g)
{
    g.fillAll(Colors::background);
}

void Profile::drawHeader(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    // Header background with gradient
    g.setGradientFill(juce::ColourGradient(
        Colors::headerBg,
        0.0f, 0.0f,
        Colors::background,
        0.0f, static_cast<float>(bounds.getHeight()),
        false
    ));
    g.fillRect(bounds);

    // Back button
    auto backBounds = getBackButtonBounds();
    g.setColour(Colors::textSecondary);
    g.setFont(20.0f);
    g.drawText("←", backBounds, juce::Justification::centred);

    // Share button
    auto shareBounds = getShareButtonBounds();
    g.setColour(Colors::textSecondary);
    g.setFont(16.0f);
    g.drawText("↗", shareBounds, juce::Justification::centred);

    // Avatar
    drawAvatar(g, getAvatarBounds());

    // User info (display name, username)
    auto avatarBounds = getAvatarBounds();
    auto userInfoBounds = juce::Rectangle<int>(
        avatarBounds.getRight() + PADDING,
        avatarBounds.getY(),
        bounds.getWidth() - avatarBounds.getRight() - PADDING * 3,
        AVATAR_SIZE
    );
    drawUserInfo(g, userInfoBounds);

    // Stats row
    int statsY = avatarBounds.getBottom() + 15;
    auto statsBounds = juce::Rectangle<int>(PADDING, statsY, bounds.getWidth() - PADDING * 2, 50);
    drawStats(g, statsBounds);

    // Action buttons
    int buttonsY = statsY + 55;
    auto buttonsBounds = juce::Rectangle<int>(PADDING, buttonsY, bounds.getWidth() - PADDING * 2, BUTTON_HEIGHT);
    drawActionButtons(g, buttonsBounds);

    // Bio
    int bioY = buttonsY + BUTTON_HEIGHT + 15;
    auto bioBounds = juce::Rectangle<int>(PADDING, bioY, bounds.getWidth() - PADDING * 2, 50);
    drawBio(g, bioBounds);

    // Social links and genre tags in a row
    int linksY = bioY + 55;
    auto linksBounds = juce::Rectangle<int>(PADDING, linksY, bounds.getWidth() / 2 - PADDING, 25);
    drawSocialLinks(g, linksBounds);

    auto genreBounds = juce::Rectangle<int>(bounds.getWidth() / 2, linksY, bounds.getWidth() / 2 - PADDING, 25);
    drawGenreTags(g, genreBounds);

    // Member since
    int memberY = linksY + 30;
    auto memberBounds = juce::Rectangle<int>(PADDING, memberY, bounds.getWidth() - PADDING * 2, 20);
    drawMemberSince(g, memberBounds);
}

void Profile::drawAvatar(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    // Use the display name for initials, falling back to username
    juce::String name = profile.displayName.isEmpty() ? profile.username : profile.displayName;

    // Draw highlighted circle if user has active story (Instagram-style)
    if (hasActiveStory)
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
        g.drawEllipse(bounds.toFloat().expanded(2.0f), 3.0f);
    }

    ImageLoader::drawCircularAvatar(g, bounds, avatarImage,
        ImageLoader::getInitials(name),
        Colors::accent.darker(0.5f),
        Colors::textPrimary,
        36.0f);

    // Avatar border (only if no story highlight)
    if (!hasActiveStory)
    {
        g.setColour(Colors::accent.withAlpha(0.5f));
        g.drawEllipse(bounds.toFloat(), 3.0f);
    }

    // Draw online indicator (green/cyan dot in bottom-right corner)
    if (profile.isOnline || profile.isInStudio)
    {
        const int indicatorSize = 18;
        const int borderWidth = 3;

        // Position at bottom-right of avatar
        auto indicatorBounds = juce::Rectangle<int>(
            bounds.getRight() - indicatorSize + 3,
            bounds.getBottom() - indicatorSize + 3,
            indicatorSize,
            indicatorSize
        ).toFloat();

        // Draw dark border (matches card background)
        g.setColour(Colors::background);
        g.fillEllipse(indicatorBounds);

        // Draw indicator (cyan for in_studio, green for just online)
        auto innerBounds = indicatorBounds.reduced(borderWidth);
        g.setColour(profile.isInStudio ? SidechainColors::inStudioIndicator() : SidechainColors::onlineIndicator());
        g.fillEllipse(innerBounds);
    }
}

void Profile::drawUserInfo(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    // Display name with optional lock icon for private accounts
    g.setColour(Colors::textPrimary);
    g.setFont(juce::Font(22.0f, juce::Font::bold));
    juce::String name = profile.displayName.isEmpty() ? profile.username : profile.displayName;

    if (profile.isPrivate)
    {
        // Measure the name width to position the lock icon
        auto nameWidth = g.getCurrentFont().getStringWidth(name);
        g.drawText(name, bounds.getX(), bounds.getY() + 10, bounds.getWidth(), 28,
                   juce::Justification::centredLeft);

        // Draw lock icon after the name
        g.setColour(Colors::textSecondary);
        g.setFont(16.0f);
        g.drawText(juce::String::fromUTF8("\xF0\x9F\x94\x92"), // 🔒 Lock emoji
                   bounds.getX() + nameWidth + 8, bounds.getY() + 12, 24, 24,
                   juce::Justification::centredLeft);
    }
    else
    {
        g.drawText(name, bounds.getX(), bounds.getY() + 10, bounds.getWidth(), 28,
                   juce::Justification::centredLeft);
    }

    // Username
    g.setColour(Colors::textSecondary);
    g.setFont(14.0f);
    g.drawText("@" + profile.username, bounds.getX(), bounds.getY() + 40, bounds.getWidth(), 20,
               juce::Justification::centredLeft);

    // Location and DAW on same line
    juce::String infoLine;
    if (profile.location.isNotEmpty())
        infoLine = profile.location;
    if (profile.dawPreference.isNotEmpty())
    {
        if (infoLine.isNotEmpty())
            infoLine += " • ";
        infoLine += profile.dawPreference;
    }

    if (infoLine.isNotEmpty())
    {
        g.setColour(Colors::textSecondary);
        g.setFont(12.0f);
        g.drawText(infoLine, bounds.getX(), bounds.getY() + 62, bounds.getWidth(), 18,
                   juce::Justification::centredLeft);
    }

    // "Follows you" badge
    if (profile.isFollowedBy && !profile.isOwnProfile(currentUserId))
    {
        auto badgeBounds = juce::Rectangle<int>(bounds.getX(), bounds.getY() + 82, 75, 18);
        g.setColour(Colors::badge);
        g.fillRoundedRectangle(badgeBounds.toFloat(), 4.0f);
        g.setColour(Colors::textSecondary);
        g.setFont(10.0f);
        g.drawText("Follows you", badgeBounds, juce::Justification::centred);
    }
}

void Profile::drawStats(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    int statSpacing = bounds.getWidth() / 3;

    // Posts count
    auto postsBounds = juce::Rectangle<int>(bounds.getX(), bounds.getY(), statSpacing, bounds.getHeight());
    g.setColour(Colors::textPrimary);
    g.setFont(juce::Font(18.0f, juce::Font::bold));
    g.drawText(StringFormatter::formatCount(profile.postCount), postsBounds.withHeight(22), juce::Justification::centred);
    g.setColour(Colors::textSecondary);
    g.setFont(12.0f);
    g.drawText("Posts", postsBounds.withY(postsBounds.getY() + 22).withHeight(20), juce::Justification::centred);

    // Followers count (tappable)
    auto followersBounds = juce::Rectangle<int>(bounds.getX() + statSpacing, bounds.getY(), statSpacing, bounds.getHeight());
    g.setColour(Colors::textPrimary);
    g.setFont(juce::Font(18.0f, juce::Font::bold));
    g.drawText(StringFormatter::formatCount(profile.followerCount), followersBounds.withHeight(22), juce::Justification::centred);
    g.setColour(Colors::textSecondary);
    g.setFont(12.0f);
    g.drawText("Followers", followersBounds.withY(followersBounds.getY() + 22).withHeight(20), juce::Justification::centred);

    // Following count (tappable)
    auto followingBounds = juce::Rectangle<int>(bounds.getX() + statSpacing * 2, bounds.getY(), statSpacing, bounds.getHeight());
    g.setColour(Colors::textPrimary);
    g.setFont(juce::Font(18.0f, juce::Font::bold));
    g.drawText(StringFormatter::formatCount(profile.followingCount), followingBounds.withHeight(22), juce::Justification::centred);
    g.setColour(Colors::textSecondary);
    g.setFont(12.0f);
    g.drawText("Following", followingBounds.withY(followingBounds.getY() + 22).withHeight(20), juce::Justification::centred);
}

void Profile::drawActionButtons(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    bool isOwnProfile = profile.isOwnProfile(currentUserId);

    if (isOwnProfile)
    {
        // Edit Profile button (full width)
        auto editBounds = getEditButtonBounds();
        g.setColour(Colors::badge);
        g.fillRoundedRectangle(editBounds.toFloat(), 6.0f);
        g.setColour(Colors::textPrimary);
        g.setFont(14.0f);
        g.drawText("Edit Profile", editBounds, juce::Justification::centred);

        // Saved Posts button (below edit button)
        auto savedBounds = getSavedPostsButtonBounds();
        g.setColour(Colors::badge);
        g.fillRoundedRectangle(savedBounds.toFloat(), 6.0f);
        g.setColour(Colors::textPrimary);
        g.setFont(14.0f);
        g.drawText("Saved Posts", savedBounds, juce::Justification::centred);

        // Archived Posts button (below saved posts button)
        auto archivedBounds = getArchivedPostsButtonBounds();
        g.setColour(Colors::badge);
        g.fillRoundedRectangle(archivedBounds.toFloat(), 6.0f);
        g.setColour(Colors::textPrimary);
        g.setFont(14.0f);
        g.drawText("Archived Posts", archivedBounds, juce::Justification::centred);

        // Notification Settings button (below archived posts button)
        auto notifBounds = getNotificationSettingsButtonBounds();
        g.setColour(Colors::badge);
        g.fillRoundedRectangle(notifBounds.toFloat(), 6.0f);
        g.setColour(Colors::textPrimary);
        g.setFont(14.0f);
        g.drawText("Notifications", notifBounds, juce::Justification::centred);

        // Activity Status button (below notification settings button)
        auto activityBounds = getActivityStatusButtonBounds();
        g.setColour(Colors::badge);
        g.fillRoundedRectangle(activityBounds.toFloat(), 6.0f);
        g.setColour(Colors::textPrimary);
        g.setFont(14.0f);
        g.drawText("Activity Status", activityBounds, juce::Justification::centred);
    }
    else
    {
        // Follow/Following/Request button (left side)
        auto followBounds = getFollowButtonBounds();
        if (profile.isFollowing)
        {
            // Already following
            g.setColour(Colors::followingButton);
            g.fillRoundedRectangle(followBounds.toFloat(), 6.0f);
            g.setColour(Colors::textSecondary);
            g.setFont(14.0f);
            g.drawText("Following", followBounds, juce::Justification::centred);
        }
        else if (profile.followRequestStatus == "pending")
        {
            // Pending follow request
            g.setColour(Colors::followingButton);
            g.fillRoundedRectangle(followBounds.toFloat(), 6.0f);
            g.setColour(Colors::textSecondary);
            g.setFont(14.0f);
            g.drawText("Requested", followBounds, juce::Justification::centred);
        }
        else if (profile.isPrivate)
        {
            // Private account - show "Request" button
            g.setColour(Colors::followButton);
            g.fillRoundedRectangle(followBounds.toFloat(), 6.0f);
            g.setColour(Colors::textPrimary);
            g.setFont(14.0f);
            g.drawText("Request", followBounds, juce::Justification::centred);
        }
        else
        {
            // Public account - show "Follow" button
            g.setColour(Colors::followButton);
            g.fillRoundedRectangle(followBounds.toFloat(), 6.0f);
            g.setColour(Colors::textPrimary);
            g.setFont(14.0f);
            g.drawText("Follow", followBounds, juce::Justification::centred);
        }

        // Message button (right side)
        auto messageBounds = getMessageButtonBounds();
        g.setColour(Colors::badge);
        g.fillRoundedRectangle(messageBounds.toFloat(), 6.0f);
        g.setColour(Colors::textPrimary);
        g.setFont(14.0f);
        g.drawText("Message", messageBounds, juce::Justification::centred);
    }
}

void Profile::drawBio(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    if (profile.bio.isEmpty())
        return;

    g.setColour(Colors::textPrimary);
    g.setFont(13.0f);

    // Word wrap the bio text
    juce::GlyphArrangement glyphs;
    glyphs.addFittedText(g.getCurrentFont(), profile.bio,
                         static_cast<float>(bounds.getX()), static_cast<float>(bounds.getY()),
                         static_cast<float>(bounds.getWidth()), static_cast<float>(bounds.getHeight()),
                         juce::Justification::topLeft, 3);
    glyphs.draw(g);
}

void Profile::drawSocialLinks(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    if (!profile.socialLinks.isObject())
        return;

    auto* obj = profile.socialLinks.getDynamicObject();
    if (obj == nullptr)
        return;

    int x = bounds.getX();
    int iconSize = 20;
    int spacing = 8;

    // Draw social platform icons
    for (const auto& prop : obj->getProperties())
    {
        juce::String platform = prop.name.toString();
        juce::String url = prop.value.toString();

        if (url.isEmpty())
            continue;

        // Draw icon based on platform
        g.setColour(Colors::link);
        g.setFont(14.0f);

        juce::String icon;
        if (platform.containsIgnoreCase("instagram"))
            icon = "📷";
        else if (platform.containsIgnoreCase("soundcloud"))
            icon = "☁";
        else if (platform.containsIgnoreCase("spotify"))
            icon = "🎵";
        else if (platform.containsIgnoreCase("twitter") || platform.containsIgnoreCase("x"))
            icon = "𝕏";
        else if (platform.containsIgnoreCase("youtube"))
            icon = "▶";
        else
            icon = "🔗";

        g.drawText(icon, x, bounds.getY(), iconSize, bounds.getHeight(), juce::Justification::centred);
        x += iconSize + spacing;

        if (x > bounds.getRight() - iconSize)
            break;
    }
}

void Profile::drawGenreTags(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    if (profile.genre.isEmpty())
        return;

    // Split genre string by comma or space
    juce::StringArray genres;
    genres.addTokens(profile.genre, ", ", "");

    int x = bounds.getX();
    int tagHeight = 20;
    int tagPadding = 8;
    int spacing = 6;

    g.setFont(11.0f);

    for (const auto& genre : genres)
    {
        if (genre.trim().isEmpty())
            continue;

        int tagWidth = static_cast<int>(g.getCurrentFont().getStringWidth(genre)) + tagPadding * 2;

        if (x + tagWidth > bounds.getRight())
            break;

        auto tagBounds = juce::Rectangle<int>(x, bounds.getY() + (bounds.getHeight() - tagHeight) / 2,
                                               tagWidth, tagHeight);
        g.setColour(Colors::badge);
        g.fillRoundedRectangle(tagBounds.toFloat(), 4.0f);

        g.setColour(Colors::textSecondary);
        g.drawText(genre.trim(), tagBounds, juce::Justification::centred);

        x += tagWidth + spacing;
    }
}

void Profile::drawMemberSince(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    juce::String memberSince = profile.getMemberSince();
    if (memberSince.isEmpty())
        return;

    g.setColour(Colors::textSecondary);
    g.setFont(11.0f);
    g.drawText(memberSince, bounds, juce::Justification::centredLeft);
}

void Profile::drawLoadingState(juce::Graphics& g)
{
    g.setColour(Colors::textSecondary);
    g.setFont(16.0f);
    g.drawText("Loading profile...", getLocalBounds(), juce::Justification::centred);
}

void Profile::drawErrorState(juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    g.setColour(Colors::errorRed);
    g.setFont(16.0f);
    g.drawText("Failed to load profile", bounds.withHeight(30).withY(bounds.getCentreY() - 30),
               juce::Justification::centred);

    g.setColour(Colors::textSecondary);
    g.setFont(13.0f);
    g.drawText(errorMessage, bounds.withHeight(20).withY(bounds.getCentreY()),
               juce::Justification::centred);

    // Retry button
    auto retryBounds = bounds.withSize(120, 36).withCentre(juce::Point<int>(bounds.getCentreX(), bounds.getCentreY() + 40));
    g.setColour(Colors::accent);
    g.fillRoundedRectangle(retryBounds.toFloat(), 6.0f);
    g.setColour(Colors::textPrimary);
    g.setFont(14.0f);
    g.drawText("Retry", retryBounds, juce::Justification::centred);
}

void Profile::drawEmptyState(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    bool isOwnProfile = profile.isOwnProfile(currentUserId);

    g.setColour(Colors::textSecondary);
    g.setFont(16.0f);

    if (isOwnProfile)
    {
        g.drawText("You haven't posted any loops yet", bounds.withHeight(30), juce::Justification::centred);
        g.setFont(13.0f);
        g.drawText("Start recording to share your music!", bounds.withY(bounds.getY() + 35).withHeight(20),
                   juce::Justification::centred);
    }
    else
    {
        g.drawText("No posts yet", bounds.withHeight(30), juce::Justification::centred);
    }
}

//==============================================================================
void Profile::resized()
{
    Log::debug("Profile::resized: Component resized to " + juce::String(getWidth()) + "x" + juce::String(getHeight()));
    auto bounds = getLocalBounds();

    // Calculate content area offset (header + optional highlights)
    int contentTopOffset = HEADER_HEIGHT;

    // Position story highlights below header
    if (storyHighlights)
    {
        storyHighlights->setBounds(0, HEADER_HEIGHT, bounds.getWidth() - 10, HIGHLIGHTS_HEIGHT);
        contentTopOffset += HIGHLIGHTS_HEIGHT;
    }

    // Position scroll bar (starts after header and highlights)
    scrollBar->setBounds(bounds.getRight() - 10, contentTopOffset, 10, bounds.getHeight() - contentTopOffset);

    // Update scroll bar range
    int contentHeight = calculateContentHeight();
    int visibleHeight = bounds.getHeight() - contentTopOffset;
    scrollBar->setRangeLimits(0.0, static_cast<double>(contentHeight));
    scrollBar->setCurrentRange(static_cast<double>(scrollOffset), static_cast<double>(visibleHeight));
    Log::debug("Profile::resized: Scroll range updated - contentHeight: " + juce::String(contentHeight) + ", visibleHeight: " + juce::String(visibleHeight));

    // Position post cards
    updatePostCards();

    // Reposition followers list panel if visible
    if (followersListPanel && followersListVisible)
    {
        int panelWidth = juce::jmin(350, static_cast<int>(getWidth() * 0.4f));
        followersListPanel->setBounds(getWidth() - panelWidth, 0, panelWidth, getHeight());
        Log::debug("Profile::resized: Followers list panel repositioned - width: " + juce::String(panelWidth));
    }

    // Position error state component to fill the entire area
    if (errorStateComponent != nullptr)
    {
        errorStateComponent->setBounds(bounds);
    }

    // TODO: Phase 4.1.10 - Add profile verification system (future: badges)
    // Note: Profile shows follower/following counts but clicking them opens FollowersList panel - this is already implemented
}

void Profile::mouseUp(const juce::MouseEvent& event)
{
    auto pos = event.getPosition();
    Log::debug("Profile::mouseUp: Mouse clicked at (" + juce::String(pos.x) + ", " + juce::String(pos.y) + ")");

    // Back button
    if (getBackButtonBounds().contains(pos))
    {
        Log::debug("Profile::mouseUp: Back button clicked");
        if (onBackPressed)
            onBackPressed();
        else
            Log::warn("Profile::mouseUp: Back button clicked but callback not set");
        return;
    }

    // Avatar click - if user has story, open story viewer
    if (getAvatarBounds().contains(pos) && hasActiveStory)
    {
        Log::info("Profile::mouseUp: Avatar clicked with active story - userId: " + profile.id);
        if (onViewStoryClicked)
            onViewStoryClicked(profile.id);
        else
            Log::warn("Profile::mouseUp: View story clicked but callback not set");
        return;
    }

    // Share button
    if (getShareButtonBounds().contains(pos))
    {
        Log::info("Profile::mouseUp: Share button clicked");
        shareProfile();
        return;
    }

    // Followers stat
    if (getFollowersBounds().contains(pos))
    {
        Log::info("Profile::mouseUp: Followers stat clicked - userId: " + profile.id);
        showFollowersList(profile.id, FollowersList::ListType::Followers);
        if (onFollowersClicked)
            onFollowersClicked(profile.id);
        else
            Log::warn("Profile::mouseUp: Followers clicked but callback not set");
        return;
    }

    // Following stat
    if (getFollowingBounds().contains(pos))
    {
        Log::info("Profile::mouseUp: Following stat clicked - userId: " + profile.id);
        showFollowersList(profile.id, FollowersList::ListType::Following);
        if (onFollowingClicked)
            onFollowingClicked(profile.id);
        else
            Log::warn("Profile::mouseUp: Following clicked but callback not set");
        return;
    }

    // Follow/Edit button
    if (profile.isOwnProfile(currentUserId))
    {
        if (getEditButtonBounds().contains(pos))
        {
            Log::info("Profile::mouseUp: Edit profile button clicked");
            if (onEditProfile)
                onEditProfile();
            else
                Log::warn("Profile::mouseUp: Edit profile clicked but callback not set");
            return;
        }

        if (getSavedPostsButtonBounds().contains(pos))
        {
            Log::info("Profile::mouseUp: Saved posts button clicked");
            if (onSavedPostsClicked)
                onSavedPostsClicked();
            else
                Log::warn("Profile::mouseUp: Saved posts clicked but callback not set");
            return;
        }

        if (getArchivedPostsButtonBounds().contains(pos))
        {
            Log::info("Profile::mouseUp: Archived posts button clicked");
            if (onArchivedPostsClicked)
                onArchivedPostsClicked();
            else
                Log::warn("Profile::mouseUp: Archived posts clicked but callback not set");
            return;
        }

        if (getNotificationSettingsButtonBounds().contains(pos))
        {
            Log::info("Profile::mouseUp: Notification settings button clicked");
            if (onNotificationSettingsClicked)
                onNotificationSettingsClicked();
            else
                Log::warn("Profile::mouseUp: Notification settings clicked but callback not set");
            return;
        }

        if (getActivityStatusButtonBounds().contains(pos))
        {
            Log::info("Profile::mouseUp: Activity status button clicked");
            if (onActivityStatusClicked)
                onActivityStatusClicked();
            else
                Log::warn("Profile::mouseUp: Activity status clicked but callback not set");
            return;
        }
    }
    else
    {
        if (getFollowButtonBounds().contains(pos))
        {
            Log::info("Profile::mouseUp: Follow/Unfollow button clicked - userId: " + profile.id);
            handleFollowToggle();
            return;
        }

        if (getMessageButtonBounds().contains(pos))
        {
            Log::info("Profile::mouseUp: Message button clicked - userId: " + profile.id);
            if (onMessageClicked)
                onMessageClicked(profile.id);
            else
                Log::warn("Profile::mouseUp: Message clicked but callback not set");
            return;
        }
    }

    // Check for retry button in error state
    if (hasError)
    {
        auto retryBounds = getLocalBounds().withSize(120, 36)
            .withCentre(juce::Point<int>(getWidth() / 2, getHeight() / 2 + 40));
        if (retryBounds.contains(pos))
        {
            Log::info("Profile::mouseUp: Retry button clicked");
            refresh();
            return;
        }
    }
}

void Profile::scrollBarMoved(juce::ScrollBar* /*scrollBarThatHasMoved*/, double newRangeStart)
{
    int oldOffset = scrollOffset;
    scrollOffset = static_cast<int>(newRangeStart);
    Log::debug("Profile::scrollBarMoved: Scroll offset changed from " + juce::String(oldOffset) + " to " + juce::String(scrollOffset));
    updatePostCards();
    repaint();
}

//==============================================================================
juce::Rectangle<int> Profile::getBackButtonBounds() const
{
    return juce::Rectangle<int>(PADDING, 15, 40, 30);
}

juce::Rectangle<int> Profile::getAvatarBounds() const
{
    return juce::Rectangle<int>(PADDING, 50, AVATAR_SIZE, AVATAR_SIZE);
}

juce::Rectangle<int> Profile::getFollowersBounds() const
{
    int statsY = getAvatarBounds().getBottom() + 15;
    int statSpacing = (getWidth() - PADDING * 2) / 3;
    return juce::Rectangle<int>(PADDING + statSpacing, statsY, statSpacing, 50);
}

juce::Rectangle<int> Profile::getFollowingBounds() const
{
    int statsY = getAvatarBounds().getBottom() + 15;
    int statSpacing = (getWidth() - PADDING * 2) / 3;
    return juce::Rectangle<int>(PADDING + statSpacing * 2, statsY, statSpacing, 50);
}

juce::Rectangle<int> Profile::getFollowButtonBounds() const
{
    int buttonsY = getAvatarBounds().getBottom() + 70;
    int buttonWidth = (getWidth() - PADDING * 3) / 2;  // Half width minus spacing
    return juce::Rectangle<int>(PADDING, buttonsY, buttonWidth, BUTTON_HEIGHT);
}

juce::Rectangle<int> Profile::getMessageButtonBounds() const
{
    int buttonsY = getAvatarBounds().getBottom() + 70;
    int buttonWidth = (getWidth() - PADDING * 3) / 2;  // Half width minus spacing
    return juce::Rectangle<int>(PADDING * 2 + buttonWidth, buttonsY, buttonWidth, BUTTON_HEIGHT);
}

juce::Rectangle<int> Profile::getEditButtonBounds() const
{
    return getFollowButtonBounds();  // Same position
}

juce::Rectangle<int> Profile::getSavedPostsButtonBounds() const
{
    auto editBounds = getEditButtonBounds();
    return editBounds.translated(0, BUTTON_HEIGHT + 8);  // Below edit button with spacing
}

juce::Rectangle<int> Profile::getArchivedPostsButtonBounds() const
{
    auto savedBounds = getSavedPostsButtonBounds();
    return savedBounds.translated(0, BUTTON_HEIGHT + 8);  // Below saved posts button with spacing
}

juce::Rectangle<int> Profile::getNotificationSettingsButtonBounds() const
{
    auto archivedBounds = getArchivedPostsButtonBounds();
    return archivedBounds.translated(0, BUTTON_HEIGHT + 8);  // Below archived posts button with spacing
}

juce::Rectangle<int> Profile::getActivityStatusButtonBounds() const
{
    auto notifBounds = getNotificationSettingsButtonBounds();
    return notifBounds.translated(0, BUTTON_HEIGHT + 8);  // Below notification settings button with spacing
}

juce::Rectangle<int> Profile::getShareButtonBounds() const
{
    return juce::Rectangle<int>(getWidth() - PADDING - 40, 15, 40, 30);
}

juce::Rectangle<int> Profile::getPostsAreaBounds() const
{
    int topOffset = HEADER_HEIGHT + HIGHLIGHTS_HEIGHT;
    return juce::Rectangle<int>(0, topOffset, getWidth() - 12, getHeight() - topOffset);
}

//==============================================================================
void Profile::fetchProfile(const juce::String& userId)
{
    if (userId.isEmpty())
    {
        Log::error("Profile::fetchProfile: userId is empty");
        hasError = true;
        errorMessage = "Invalid user ID";
        isLoading = false;
        repaint();
        return;
    }

    if (networkClient == nullptr)
    {
        Log::error("Profile::fetchProfile: NetworkClient is null");
        hasError = true;
        errorMessage = "Network not available";
        isLoading = false;
        repaint();
        return;
    }

    // Safely construct endpoint (avoid assertion failures with invalid strings)
    juce::String safeUserId = userId.trim();
    if (safeUserId.isEmpty())
    {
        Log::error("Profile::fetchProfile: userId is empty after trimming");
        hasError = true;
        errorMessage = "Invalid user ID";
        isLoading = false;
        repaint();
        return;
    }

    juce::String endpoint = "/api/v1/users/" + safeUserId + "/profile";
    Log::info("Profile::fetchProfile: Fetching profile from: " + endpoint);

    networkClient->get(endpoint, [this, userId](Outcome<juce::var> result) {
        juce::MessageManager::callAsync([this, result, userId]() {
            isLoading = false;

            if (result.isOk() && result.getValue().isObject())
            {
                auto response = result.getValue();
                Log::info("Profile::fetchProfile: Profile fetch successful for userId: " + userId);
                setProfile(UserProfile::fromJson(response));
            }
            else
            {
                Log::error("Profile::fetchProfile: Profile fetch failed for userId: " + userId);
                hasError = true;
                if (result.isOk())
                {
                    auto response = result.getValue();
                    if (response.isObject() && response.hasProperty("error"))
                    {
                        errorMessage = response["error"].toString();
                        Log::warn("Profile::fetchProfile: Error message: " + errorMessage);
                    }
                    else
                    {
                        errorMessage = "Failed to load profile";
                        Log::warn("Profile::fetchProfile: No error message in response");
                    }
                }
                else
                {
                    errorMessage = result.getError();
                    Log::warn("Profile::fetchProfile: Error: " + errorMessage);
                }

                // Configure and show error state component
                if (errorStateComponent != nullptr)
                {
                    errorStateComponent->configureFromError(errorMessage);
                    errorStateComponent->setVisible(true);
                }
            }

            repaint();
        });
    });
}

void Profile::fetchUserPosts(const juce::String& userId)
{
    if (networkClient == nullptr)
    {
        Log::warn("Profile::fetchUserPosts: NetworkClient is null");
        return;
    }

    juce::String endpoint = "/api/v1/users/" + userId + "/posts?limit=20";
    Log::info("Profile::fetchUserPosts: Fetching posts from: " + endpoint);

    networkClient->get(endpoint, [this, userId](Outcome<juce::var> result) {
        juce::MessageManager::callAsync([this, result, userId]() {
            if (result.isOk() && result.getValue().isObject())
            {
                auto response = result.getValue();
                Log::debug("Profile::fetchUserPosts: Posts fetch successful for userId: " + userId);
                userPosts.clear();

                auto postsArray = Json::getArray(response, "posts");
                if (Json::isArray(postsArray))
                {
                    int validPosts = 0;
                    for (int i = 0; i < postsArray.size(); ++i)
                    {
                        FeedPost post = FeedPost::fromJson(postsArray[i]);
                        if (post.isValid())
                        {
                            userPosts.add(post);
                            validPosts++;
                        }
                    }
                    Log::info("Profile::fetchUserPosts: Loaded " + juce::String(validPosts) + " valid posts out of " + juce::String(postsArray.size()) + " total");
                }
                else
                {
                    Log::warn("Profile::fetchUserPosts: No posts array in response");
                }

                updatePostCards();
            }
            else
            {
                Log::error("Profile::fetchUserPosts: Posts fetch failed for userId: " + userId);
            }

            repaint();
        });
    });
}

void Profile::handleFollowToggle()
{
    if (networkClient == nullptr || profile.id.isEmpty())
    {
        Log::warn("Profile::handleFollowToggle: Cannot toggle follow - NetworkClient: " + juce::String(networkClient != nullptr ? "valid" : "null") + ", profile.id: " + profile.id);
        return;
    }

    bool wasFollowing = profile.isFollowing;
    bool willFollow = !wasFollowing;

    Log::info("Profile::handleFollowToggle: Toggling follow for userId: " + profile.id + " - wasFollowing: " + juce::String(wasFollowing ? "true" : "false") + ", willFollow: " + juce::String(willFollow ? "true" : "false"));

    // Optimistic UI update
    profile.isFollowing = willFollow;
    profile.followerCount += willFollow ? 1 : -1;
    repaint();

    // Use NetworkClient methods for follow/unfollow
    auto callback = [this, wasFollowing](Outcome<juce::var> result) {
        juce::MessageManager::callAsync([this, result, wasFollowing]() {
            if (result.isError())
            {
                Log::error("Profile::handleFollowToggle: Follow toggle failed, reverting optimistic update");
                // Revert on failure
                profile.isFollowing = wasFollowing;
                profile.followerCount += wasFollowing ? 1 : -1;
                repaint();
            }
            else
            {
                Log::info("Profile::handleFollowToggle: Follow toggle successful - isFollowing: " + juce::String(profile.isFollowing ? "true" : "false"));
                if (onFollowToggled)
                {
                    Log::debug("Profile::handleFollowToggle: Calling onFollowToggled callback");
                    onFollowToggled(profile.id);
                }
                else
                {
                    Log::warn("Profile::handleFollowToggle: Follow toggle succeeded but callback not set");
                }
            }
        });
    };

    if (willFollow)
    {
        Log::debug("Profile::handleFollowToggle: Calling followUser API");
        networkClient->followUser(profile.id, callback);
    }
    else
    {
        Log::debug("Profile::handleFollowToggle: Calling unfollowUser API");
        networkClient->unfollowUser(profile.id, callback);
    }
}

void Profile::shareProfile()
{
    juce::String profileUrl = "https://sidechain.live/user/" + profile.username;
    Log::info("Profile::shareProfile: Sharing profile - username: " + profile.username + ", URL: " + profileUrl);
    juce::SystemClipboard::copyTextToClipboard(profileUrl);
    Log::debug("Profile::shareProfile: Profile link copied to clipboard");
}

//==============================================================================
void Profile::updatePostCards()
{
    Log::debug("Profile::updatePostCards: Updating post cards - current: " + juce::String(postCards.size()) + ", needed: " + juce::String(userPosts.size()));

    // Create or update post cards
    while (postCards.size() < userPosts.size())
    {
        auto* card = new PostCard();
        card->onPlayClicked = [this](const FeedPost& post) {
            Log::debug("Profile::updatePostCards: Play clicked for post: " + post.id);
            if (onPlayClicked)
                onPlayClicked(post);
            else
                Log::warn("Profile::updatePostCards: Play clicked but callback not set");
        };
        card->onPauseClicked = [this](const FeedPost& post) {
            Log::debug("Profile::updatePostCards: Pause clicked for post: " + post.id);
            if (onPauseClicked)
                onPauseClicked(post);
            else
                Log::warn("Profile::updatePostCards: Pause clicked but callback not set");
        };
        card->onUserClicked = [this](const FeedPost& post) {
            // If clicking on the same user whose profile we're viewing, do nothing
            // Otherwise, navigate to that user's profile
            if (post.userId == profile.id)
            {
                Log::debug("Profile::updatePostCards: User clicked on own profile - no action needed");
                return;
            }

            Log::debug("Profile::updatePostCards: User clicked on post card - navigating to user: " + post.userId);
            if (onNavigateToProfile && post.userId.isNotEmpty())
                onNavigateToProfile(post.userId);
        };
        postCards.add(card);
        addAndMakeVisible(card);
        Log::debug("Profile::updatePostCards: Created new post card #" + juce::String(postCards.size()));
    }

    // Remove extra cards
    while (postCards.size() > userPosts.size())
    {
        Log::debug("Profile::updatePostCards: Removing extra post card");
        postCards.removeLast();
    }

    // Update card data and positions
    auto postsArea = getPostsAreaBounds();
    int contentTopOffset = HEADER_HEIGHT + HIGHLIGHTS_HEIGHT;
    int y = contentTopOffset - scrollOffset;
    int visibleCount = 0;

    for (int i = 0; i < userPosts.size(); ++i)
    {
        auto* card = postCards[i];
        card->setPost(userPosts[i]);
        card->setBounds(PADDING, y, postsArea.getWidth() - PADDING * 2, POST_CARD_HEIGHT);

        // Update playing state
        if (userPosts[i].id == currentlyPlayingPostId)
        {
            card->setIsPlaying(true);
            card->setPlaybackProgress(currentPlaybackProgress);
            Log::debug("Profile::updatePostCards: Post " + userPosts[i].id + " is currently playing");
        }
        else
        {
            card->setIsPlaying(false);
            card->setPlaybackProgress(0.0f);
        }

        // Show/hide based on visibility
        bool isVisible = (y + POST_CARD_HEIGHT > contentTopOffset) && (y < getHeight());
        card->setVisible(isVisible);
        if (isVisible)
            visibleCount++;

        y += POST_CARD_HEIGHT + 10;
    }

    Log::debug("Profile::updatePostCards: Updated " + juce::String(userPosts.size()) + " post cards, " + juce::String(visibleCount) + " visible");
}

int Profile::calculateContentHeight() const
{
    return static_cast<int>(userPosts.size()) * (POST_CARD_HEIGHT + 10);
}

//==============================================================================
void Profile::setCurrentlyPlayingPost(const juce::String& postId)
{
    Log::debug("Profile::setCurrentlyPlayingPost: Setting playing post - postId: " + postId);
    currentlyPlayingPostId = postId;
    updatePostCards();
}

void Profile::setPlaybackProgress(float progress)
{
    currentPlaybackProgress = progress;

    for (auto* card : postCards)
    {
        if (card->getPostId() == currentlyPlayingPostId)
        {
            card->setPlaybackProgress(progress);
            break;
        }
    }
}

void Profile::clearPlayingState()
{
    Log::debug("Profile::clearPlayingState: Clearing playing state");
    currentlyPlayingPostId = "";
    currentPlaybackProgress = 0.0f;

    for (auto* card : postCards)
    {
        card->setIsPlaying(false);
        card->setPlaybackProgress(0.0f);
    }
}

//==============================================================================
void Profile::showFollowersList(const juce::String& userId, FollowersList::ListType type)
{
    if (followersListPanel == nullptr || userId.isEmpty())
    {
        Log::warn("Profile::showFollowersList: Cannot show list - panel: " + juce::String(followersListPanel != nullptr ? "valid" : "null") + ", userId: " + userId);
        return;
    }

    juce::String typeStr = type == FollowersList::ListType::Followers ? "Followers" : "Following";
    Log::info("Profile::showFollowersList: Showing " + typeStr + " list for userId: " + userId);

    // Set up the panel
    followersListPanel->setNetworkClient(networkClient);
    followersListPanel->setCurrentUserId(currentUserId);

    // Position panel on right side (40% of width, max 350px)
    int panelWidth = juce::jmin(350, static_cast<int>(getWidth() * 0.4f));
    followersListPanel->setBounds(getWidth() - panelWidth, 0, panelWidth, getHeight());
    Log::debug("Profile::showFollowersList: Panel positioned - width: " + juce::String(panelWidth));

    // Load the list
    followersListPanel->loadList(userId, type);

    followersListPanel->setVisible(true);
    followersListPanel->toFront(true);
    followersListVisible = true;
    Log::debug("Profile::showFollowersList: Followers list panel shown");
}

void Profile::hideFollowersList()
{
    Log::debug("Profile::hideFollowersList: Hiding followers list panel");
    if (followersListPanel)
    {
        followersListPanel->setVisible(false);
    }
    followersListVisible = false;
}

//==============================================================================
void Profile::queryPresenceForProfile()
{
    if (!streamChatClient || profile.id.isEmpty())
    {
        Log::debug("Profile::queryPresenceForProfile: Skipping - streamChatClient is null or profile ID is empty");
        return;
    }

    Log::debug("Profile::queryPresenceForProfile: Querying presence for user: " + profile.id);

    std::vector<juce::String> userIds = { profile.id };

    streamChatClient->queryPresence(userIds, [this](Outcome<std::vector<StreamChatClient::UserPresence>> result) {
        if (result.isError())
        {
            Log::warn("Profile::queryPresenceForProfile: Failed to query presence: " + result.getError());
            return;
        }

        auto presenceList = result.getValue();
        if (presenceList.empty())
        {
            Log::debug("Profile::queryPresenceForProfile: No presence data returned");
            return;
        }

        const auto& presence = presenceList[0];
        profile.isOnline = presence.online;
        profile.isInStudio = (presence.status == "in_studio" || presence.status == "in studio");

        // Format last active time
        if (!presence.lastActive.isEmpty())
        {
            // Parse ISO timestamp and format as "last active X ago"
            juce::Time lastActiveTime = juce::Time::fromISO8601(presence.lastActive);
            if (lastActiveTime.toMilliseconds() > 0)
            {
                auto diff = juce::Time::getCurrentTime() - lastActiveTime;
                int minutes = static_cast<int>(diff.inMinutes());
                int hours = static_cast<int>(diff.inHours());
                int days = static_cast<int>(diff.inDays());

                if (days > 0)
                    profile.lastActive = juce::String(days) + "d ago";
                else if (hours > 0)
                    profile.lastActive = juce::String(hours) + "h ago";
                else if (minutes > 0)
                    profile.lastActive = juce::String(minutes) + "m ago";
                else
                    profile.lastActive = "Just now";
            }
        }

        repaint();
    });
}

void Profile::updateUserPresence(const juce::String& userId, bool isOnline, const juce::String& status)
{
    if (userId.isEmpty() || userId != profile.id)
        return;

    bool isInStudio = (status == "in_studio" || status == "in studio" || status == "recording");

    profile.isOnline = isOnline;
    profile.isInStudio = isInStudio;

    // Repaint to show updated online status
    repaint();
}

void Profile::checkForActiveStories(const juce::String& userId)
{
    if (networkClient == nullptr || userId.isEmpty())
    {
        hasActiveStory = false;
        repaint();
        return;
    }

    // Fetch stories feed and check if this user has any active stories
    networkClient->getStoriesFeed([this, userId](Outcome<juce::var> result) {
        juce::MessageManager::callAsync([this, result, userId]() {
            bool hasStory = false;

            if (result.isOk() && result.getValue().isObject())
            {
                auto response = result.getValue();
                if (response.hasProperty("stories"))
                {
                    auto* storiesArray = response["stories"].getArray();
                    if (storiesArray)
                    {
                        // Check if any story belongs to this user and is not expired
                        for (const auto& storyVar : *storiesArray)
                        {
                            juce::String storyUserId = storyVar["user_id"].toString();
                            if (storyUserId == userId)
                            {
                                // Check expiration
                                juce::String expiresAtStr = storyVar["expires_at"].toString();
                                if (expiresAtStr.isNotEmpty())
                                {
                                    juce::Time expiresAt = juce::Time::fromISO8601(expiresAtStr);
                                    if (expiresAt.toMilliseconds() > 0 && juce::Time::getCurrentTime() < expiresAt)
                                    {
                                        hasStory = true;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
            }

            if (hasActiveStory != hasStory)
            {
                hasActiveStory = hasStory;
                repaint();
            }
        });
    });
}

//==============================================================================
juce::String Profile::getTooltip()
{
    auto mousePos = getMouseXYRelative();

    // Back button
    if (getBackButtonBounds().contains(mousePos))
        return "Go back";

    // Avatar (view story if available)
    if (getAvatarBounds().contains(mousePos))
    {
        if (hasActiveStory)
            return "View " + profile.username + "'s story";
        return "";
    }

    // Stats
    if (getFollowersBounds().contains(mousePos))
        return "View followers";

    if (getFollowingBounds().contains(mousePos))
        return "View following";

    // Action buttons - depends on whether it's own profile or another user's
    bool isOwnProfile = profile.isOwnProfile(currentUserId);

    if (isOwnProfile)
    {
        if (getEditButtonBounds().contains(mousePos))
            return "Edit your profile";

        if (getSavedPostsButtonBounds().contains(mousePos))
            return "View saved posts";

        if (getArchivedPostsButtonBounds().contains(mousePos))
            return "View archived posts";

        if (getNotificationSettingsButtonBounds().contains(mousePos))
            return "Notification settings";

        if (getActivityStatusButtonBounds().contains(mousePos))
            return "Activity status privacy";
    }
    else
    {
        if (getFollowButtonBounds().contains(mousePos))
        {
            if (profile.isPrivate && !profile.isFollowing)
            {
                if (profile.followRequestStatus == "pending")
                    return "Cancel follow request";
                return "Request to follow";
            }
            return profile.isFollowing ? "Unfollow" : "Follow";
        }

        if (getMessageButtonBounds().contains(mousePos))
            return "Send a message";
    }

    // Share button
    if (getShareButtonBounds().contains(mousePos))
        return "Copy profile link";

    return {};
}
