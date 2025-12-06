#include "ProfileComponent.h"
#include "../../network/NetworkClient.h"
#include "../../util/Json.h"
#include "../../util/ImageCache.h"
#include "../../util/Log.h"
#include "../../util/StringFormatter.h"
#include "../feed/PostCardComponent.h"

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
    // Prefer profile_picture_url, fall back to avatar_url
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
// ProfileComponent implementation
//==============================================================================
ProfileComponent::ProfileComponent()
{
    Log::info("ProfileComponent: Initializing profile component");
    
    scrollBar = std::make_unique<juce::ScrollBar>(true);
    scrollBar->addListener(this);
    scrollBar->setAutoHide(true);
    addAndMakeVisible(scrollBar.get());
    Log::debug("ProfileComponent: Scroll bar created and added");

    // Create followers list panel (initially hidden)
    followersListPanel = std::make_unique<FollowersListComponent>();
    followersListPanel->onClose = [this]() {
        Log::debug("ProfileComponent: Followers list close requested");
        hideFollowersList();
    };
    followersListPanel->onUserClicked = [this](const juce::String& userId) {
        Log::info("ProfileComponent: User clicked in followers list - userId: " + userId);
        hideFollowersList();
        // Navigate to the clicked user's profile
        loadProfile(userId);
    };
    addChildComponent(followersListPanel.get());
    Log::debug("ProfileComponent: Followers list panel created");

    // IMPORTANT: setSize must be called LAST because it triggers resized()
    // which uses scrollBar and other child components
    setSize(600, 800);
    Log::info("ProfileComponent: Initialization complete");
}

ProfileComponent::~ProfileComponent()
{
    Log::debug("ProfileComponent: Destroying profile component");
    scrollBar->removeListener(this);
}

//==============================================================================
void ProfileComponent::setNetworkClient(NetworkClient* client)
{
    networkClient = client;
    Log::info("ProfileComponent: NetworkClient set " + juce::String(client != nullptr ? "(valid)" : "(null)"));
}

void ProfileComponent::setCurrentUserId(const juce::String& userId)
{
    currentUserId = userId;
    Log::info("ProfileComponent: Current user ID set to: " + userId);
}

//==============================================================================
void ProfileComponent::loadProfile(const juce::String& userId)
{
    if (userId.isEmpty())
    {
        Log::warn("ProfileComponent::loadProfile: Empty userId provided");
        return;
    }

    Log::info("ProfileComponent::loadProfile: Loading profile for userId: " + userId);
    isLoading = true;
    hasError = false;
    errorMessage = "";
    profile = UserProfile();
    userPosts.clear();
    postCards.clear();
    repaint();

    fetchProfile(userId);
}

void ProfileComponent::loadOwnProfile()
{
    if (currentUserId.isEmpty())
    {
        Log::warn("ProfileComponent::loadOwnProfile: currentUserId is empty");
        return;
    }

    Log::info("ProfileComponent::loadOwnProfile: Loading own profile - userId: " + currentUserId);
    loadProfile(currentUserId);
}

void ProfileComponent::setProfile(const UserProfile& newProfile)
{
    Log::info("ProfileComponent::setProfile: Setting profile - id: " + newProfile.id + ", username: " + newProfile.username);
    profile = newProfile;
    isLoading = false;
    hasError = false;
    avatarImage = juce::Image();

    // Load avatar via ImageCache
    juce::String avatarUrl = profile.getAvatarUrl();
    if (avatarUrl.isNotEmpty())
    {
        Log::debug("ProfileComponent::setProfile: Loading avatar from: " + avatarUrl);
        ImageLoader::load(avatarUrl, [this](const juce::Image& img) {
            if (img.isValid())
            {
                Log::debug("ProfileComponent::setProfile: Avatar loaded successfully");
                avatarImage = img;
            }
            else
            {
                Log::warn("ProfileComponent::setProfile: Failed to load avatar image");
            }
            repaint();
        });
    }
    else
    {
        Log::debug("ProfileComponent::setProfile: No avatar URL available");
    }

    repaint();

    // Fetch user's posts
    if (profile.id.isNotEmpty())
    {
        Log::debug("ProfileComponent::setProfile: Fetching user posts for: " + profile.id);
        fetchUserPosts(profile.id);
    }
    else
    {
        Log::warn("ProfileComponent::setProfile: Profile ID is empty, skipping post fetch");
    }
}

void ProfileComponent::refresh()
{
    if (profile.id.isNotEmpty())
    {
        Log::info("ProfileComponent::refresh: Refreshing profile - id: " + profile.id);
        loadProfile(profile.id);
    }
    else
    {
        Log::warn("ProfileComponent::refresh: Cannot refresh - profile ID is empty");
    }
}

//==============================================================================
void ProfileComponent::paint(juce::Graphics& g)
{
    drawBackground(g);

    if (isLoading)
    {
        drawLoadingState(g);
        return;
    }

    if (hasError)
    {
        drawErrorState(g);
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

void ProfileComponent::drawBackground(juce::Graphics& g)
{
    g.fillAll(Colors::background);
}

void ProfileComponent::drawHeader(juce::Graphics& g, juce::Rectangle<int> bounds)
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

void ProfileComponent::drawAvatar(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    // Use the display name for initials, falling back to username
    juce::String name = profile.displayName.isEmpty() ? profile.username : profile.displayName;

    ImageLoader::drawCircularAvatar(g, bounds, avatarImage,
        ImageLoader::getInitials(name),
        Colors::accent.darker(0.5f),
        Colors::textPrimary,
        36.0f);

    // Avatar border
    g.setColour(Colors::accent.withAlpha(0.5f));
    g.drawEllipse(bounds.toFloat(), 3.0f);
}

void ProfileComponent::drawUserInfo(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    // Display name
    g.setColour(Colors::textPrimary);
    g.setFont(juce::Font(22.0f, juce::Font::bold));
    juce::String name = profile.displayName.isEmpty() ? profile.username : profile.displayName;
    g.drawText(name, bounds.getX(), bounds.getY() + 10, bounds.getWidth(), 28,
               juce::Justification::centredLeft);

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

void ProfileComponent::drawStats(juce::Graphics& g, juce::Rectangle<int> bounds)
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

void ProfileComponent::drawActionButtons(juce::Graphics& g, juce::Rectangle<int> bounds)
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
    }
    else
    {
        // Follow/Following button (left side)
        auto followBounds = getFollowButtonBounds();
        if (profile.isFollowing)
        {
            g.setColour(Colors::followingButton);
            g.fillRoundedRectangle(followBounds.toFloat(), 6.0f);
            g.setColour(Colors::textSecondary);
            g.setFont(14.0f);
            g.drawText("Following", followBounds, juce::Justification::centred);
        }
        else
        {
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

void ProfileComponent::drawBio(juce::Graphics& g, juce::Rectangle<int> bounds)
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

void ProfileComponent::drawSocialLinks(juce::Graphics& g, juce::Rectangle<int> bounds)
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

void ProfileComponent::drawGenreTags(juce::Graphics& g, juce::Rectangle<int> bounds)
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

void ProfileComponent::drawMemberSince(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    juce::String memberSince = profile.getMemberSince();
    if (memberSince.isEmpty())
        return;

    g.setColour(Colors::textSecondary);
    g.setFont(11.0f);
    g.drawText(memberSince, bounds, juce::Justification::centredLeft);
}

void ProfileComponent::drawLoadingState(juce::Graphics& g)
{
    g.setColour(Colors::textSecondary);
    g.setFont(16.0f);
    g.drawText("Loading profile...", getLocalBounds(), juce::Justification::centred);
}

void ProfileComponent::drawErrorState(juce::Graphics& g)
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

void ProfileComponent::drawEmptyState(juce::Graphics& g, juce::Rectangle<int> bounds)
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
void ProfileComponent::resized()
{
    Log::debug("ProfileComponent::resized: Component resized to " + juce::String(getWidth()) + "x" + juce::String(getHeight()));
    auto bounds = getLocalBounds();

    // Position scroll bar
    scrollBar->setBounds(bounds.getRight() - 10, HEADER_HEIGHT, 10, bounds.getHeight() - HEADER_HEIGHT);

    // Update scroll bar range
    int contentHeight = calculateContentHeight();
    int visibleHeight = bounds.getHeight() - HEADER_HEIGHT;
    scrollBar->setRangeLimits(0.0, static_cast<double>(contentHeight));
    scrollBar->setCurrentRange(static_cast<double>(scrollOffset), static_cast<double>(visibleHeight));
    Log::debug("ProfileComponent::resized: Scroll range updated - contentHeight: " + juce::String(contentHeight) + ", visibleHeight: " + juce::String(visibleHeight));

    // Position post cards
    updatePostCards();

    // Reposition followers list panel if visible
    if (followersListPanel && followersListVisible)
    {
        int panelWidth = juce::jmin(350, static_cast<int>(getWidth() * 0.4f));
        followersListPanel->setBounds(getWidth() - panelWidth, 0, panelWidth, getHeight());
        Log::debug("ProfileComponent::resized: Followers list panel repositioned - width: " + juce::String(panelWidth));
    }
}

void ProfileComponent::mouseUp(const juce::MouseEvent& event)
{
    auto pos = event.getPosition();
    Log::debug("ProfileComponent::mouseUp: Mouse clicked at (" + juce::String(pos.x) + ", " + juce::String(pos.y) + ")");

    // Back button
    if (getBackButtonBounds().contains(pos))
    {
        Log::debug("ProfileComponent::mouseUp: Back button clicked");
        if (onBackPressed)
            onBackPressed();
        else
            Log::warn("ProfileComponent::mouseUp: Back button clicked but callback not set");
        return;
    }

    // Share button
    if (getShareButtonBounds().contains(pos))
    {
        Log::info("ProfileComponent::mouseUp: Share button clicked");
        shareProfile();
        return;
    }

    // Followers stat
    if (getFollowersBounds().contains(pos))
    {
        Log::info("ProfileComponent::mouseUp: Followers stat clicked - userId: " + profile.id);
        showFollowersList(profile.id, FollowersListComponent::ListType::Followers);
        if (onFollowersClicked)
            onFollowersClicked(profile.id);
        else
            Log::warn("ProfileComponent::mouseUp: Followers clicked but callback not set");
        return;
    }

    // Following stat
    if (getFollowingBounds().contains(pos))
    {
        Log::info("ProfileComponent::mouseUp: Following stat clicked - userId: " + profile.id);
        showFollowersList(profile.id, FollowersListComponent::ListType::Following);
        if (onFollowingClicked)
            onFollowingClicked(profile.id);
        else
            Log::warn("ProfileComponent::mouseUp: Following clicked but callback not set");
        return;
    }

    // Follow/Edit button
    if (profile.isOwnProfile(currentUserId))
    {
        if (getEditButtonBounds().contains(pos))
        {
            Log::info("ProfileComponent::mouseUp: Edit profile button clicked");
            if (onEditProfile)
                onEditProfile();
            else
                Log::warn("ProfileComponent::mouseUp: Edit profile clicked but callback not set");
            return;
        }
    }
    else
    {
        if (getFollowButtonBounds().contains(pos))
        {
            Log::info("ProfileComponent::mouseUp: Follow/Unfollow button clicked - userId: " + profile.id);
            handleFollowToggle();
            return;
        }

        if (getMessageButtonBounds().contains(pos))
        {
            Log::info("ProfileComponent::mouseUp: Message button clicked - userId: " + profile.id);
            if (onMessageClicked)
                onMessageClicked(profile.id);
            else
                Log::warn("ProfileComponent::mouseUp: Message clicked but callback not set");
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
            Log::info("ProfileComponent::mouseUp: Retry button clicked");
            refresh();
            return;
        }
    }
}

void ProfileComponent::scrollBarMoved(juce::ScrollBar* /*scrollBarThatHasMoved*/, double newRangeStart)
{
    int oldOffset = scrollOffset;
    scrollOffset = static_cast<int>(newRangeStart);
    Log::debug("ProfileComponent::scrollBarMoved: Scroll offset changed from " + juce::String(oldOffset) + " to " + juce::String(scrollOffset));
    updatePostCards();
    repaint();
}

//==============================================================================
juce::Rectangle<int> ProfileComponent::getBackButtonBounds() const
{
    return juce::Rectangle<int>(PADDING, 15, 40, 30);
}

juce::Rectangle<int> ProfileComponent::getAvatarBounds() const
{
    return juce::Rectangle<int>(PADDING, 50, AVATAR_SIZE, AVATAR_SIZE);
}

juce::Rectangle<int> ProfileComponent::getFollowersBounds() const
{
    int statsY = getAvatarBounds().getBottom() + 15;
    int statSpacing = (getWidth() - PADDING * 2) / 3;
    return juce::Rectangle<int>(PADDING + statSpacing, statsY, statSpacing, 50);
}

juce::Rectangle<int> ProfileComponent::getFollowingBounds() const
{
    int statsY = getAvatarBounds().getBottom() + 15;
    int statSpacing = (getWidth() - PADDING * 2) / 3;
    return juce::Rectangle<int>(PADDING + statSpacing * 2, statsY, statSpacing, 50);
}

juce::Rectangle<int> ProfileComponent::getFollowButtonBounds() const
{
    int buttonsY = getAvatarBounds().getBottom() + 70;
    int buttonWidth = (getWidth() - PADDING * 3) / 2;  // Half width minus spacing
    return juce::Rectangle<int>(PADDING, buttonsY, buttonWidth, BUTTON_HEIGHT);
}

juce::Rectangle<int> ProfileComponent::getMessageButtonBounds() const
{
    int buttonsY = getAvatarBounds().getBottom() + 70;
    int buttonWidth = (getWidth() - PADDING * 3) / 2;  // Half width minus spacing
    return juce::Rectangle<int>(PADDING * 2 + buttonWidth, buttonsY, buttonWidth, BUTTON_HEIGHT);
}

juce::Rectangle<int> ProfileComponent::getEditButtonBounds() const
{
    return getFollowButtonBounds();  // Same position
}

juce::Rectangle<int> ProfileComponent::getShareButtonBounds() const
{
    return juce::Rectangle<int>(getWidth() - PADDING - 40, 15, 40, 30);
}

juce::Rectangle<int> ProfileComponent::getPostsAreaBounds() const
{
    return juce::Rectangle<int>(0, HEADER_HEIGHT, getWidth() - 12, getHeight() - HEADER_HEIGHT);
}

//==============================================================================
void ProfileComponent::fetchProfile(const juce::String& userId)
{
    if (networkClient == nullptr)
    {
        Log::error("ProfileComponent::fetchProfile: NetworkClient is null");
        hasError = true;
        errorMessage = "Network not available";
        isLoading = false;
        repaint();
        return;
    }

    juce::String endpoint = "/api/v1/users/" + userId + "/profile";
    Log::info("ProfileComponent::fetchProfile: Fetching profile from: " + endpoint);

    networkClient->get(endpoint, [this, userId](Outcome<juce::var> result) {
        juce::MessageManager::callAsync([this, result, userId]() {
            isLoading = false;

            if (result.isOk() && result.getValue().isObject())
            {
                auto response = result.getValue();
                Log::info("ProfileComponent::fetchProfile: Profile fetch successful for userId: " + userId);
                setProfile(UserProfile::fromJson(response));
            }
            else
            {
                Log::error("ProfileComponent::fetchProfile: Profile fetch failed for userId: " + userId);
                hasError = true;
                if (result.isOk())
                {
                    auto response = result.getValue();
                    if (response.isObject() && response.hasProperty("error"))
                    {
                        errorMessage = response["error"].toString();
                        Log::warn("ProfileComponent::fetchProfile: Error message: " + errorMessage);
                    }
                    else
                    {
                        errorMessage = "Failed to load profile";
                        Log::warn("ProfileComponent::fetchProfile: No error message in response");
                    }
                }
                else
                {
                    errorMessage = result.getError();
                    Log::warn("ProfileComponent::fetchProfile: Error: " + errorMessage);
                }
            }

            repaint();
        });
    });
}

void ProfileComponent::fetchUserPosts(const juce::String& userId)
{
    if (networkClient == nullptr)
    {
        Log::warn("ProfileComponent::fetchUserPosts: NetworkClient is null");
        return;
    }

    juce::String endpoint = "/api/v1/users/" + userId + "/posts?limit=20";
    Log::info("ProfileComponent::fetchUserPosts: Fetching posts from: " + endpoint);

    networkClient->get(endpoint, [this, userId](Outcome<juce::var> result) {
        juce::MessageManager::callAsync([this, result, userId]() {
            if (result.isOk() && result.getValue().isObject())
            {
                auto response = result.getValue();
                Log::debug("ProfileComponent::fetchUserPosts: Posts fetch successful for userId: " + userId);
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
                    Log::info("ProfileComponent::fetchUserPosts: Loaded " + juce::String(validPosts) + " valid posts out of " + juce::String(postsArray.size()) + " total");
                }
                else
                {
                    Log::warn("ProfileComponent::fetchUserPosts: No posts array in response");
                }

                updatePostCards();
            }
            else
            {
                Log::error("ProfileComponent::fetchUserPosts: Posts fetch failed for userId: " + userId);
            }

            repaint();
        });
    });
}

void ProfileComponent::handleFollowToggle()
{
    if (networkClient == nullptr || profile.id.isEmpty())
    {
        Log::warn("ProfileComponent::handleFollowToggle: Cannot toggle follow - NetworkClient: " + juce::String(networkClient != nullptr ? "valid" : "null") + ", profile.id: " + profile.id);
        return;
    }

    bool wasFollowing = profile.isFollowing;
    bool willFollow = !wasFollowing;

    Log::info("ProfileComponent::handleFollowToggle: Toggling follow for userId: " + profile.id + " - wasFollowing: " + juce::String(wasFollowing ? "true" : "false") + ", willFollow: " + juce::String(willFollow ? "true" : "false"));

    // Optimistic UI update
    profile.isFollowing = willFollow;
    profile.followerCount += willFollow ? 1 : -1;
    repaint();

    juce::String endpoint = "/users/" + profile.id + (wasFollowing ? "/unfollow" : "/follow");
    Log::debug("ProfileComponent::handleFollowToggle: Calling endpoint: " + endpoint);

    networkClient->post(endpoint, juce::var(), [this, wasFollowing](Outcome<juce::var> result) {
        juce::MessageManager::callAsync([this, result, wasFollowing]() {
            if (result.isError())
            {
                Log::error("ProfileComponent::handleFollowToggle: Follow toggle failed, reverting optimistic update");
                // Revert on failure
                profile.isFollowing = wasFollowing;
                profile.followerCount += wasFollowing ? 1 : -1;
                repaint();
            }
            else
            {
                Log::info("ProfileComponent::handleFollowToggle: Follow toggle successful - isFollowing: " + juce::String(profile.isFollowing ? "true" : "false"));
                if (onFollowToggled)
                {
                    Log::debug("ProfileComponent::handleFollowToggle: Calling onFollowToggled callback");
                    onFollowToggled(profile.id);
                }
                else
                {
                    Log::warn("ProfileComponent::handleFollowToggle: Follow toggle succeeded but callback not set");
                }
            }
        });
    });
}

void ProfileComponent::shareProfile()
{
    juce::String profileUrl = "https://sidechain.live/user/" + profile.username;
    Log::info("ProfileComponent::shareProfile: Sharing profile - username: " + profile.username + ", URL: " + profileUrl);
    juce::SystemClipboard::copyTextToClipboard(profileUrl);
    Log::debug("ProfileComponent::shareProfile: Profile link copied to clipboard");
}

//==============================================================================
void ProfileComponent::updatePostCards()
{
    Log::debug("ProfileComponent::updatePostCards: Updating post cards - current: " + juce::String(postCards.size()) + ", needed: " + juce::String(userPosts.size()));
    
    // Create or update post cards
    while (postCards.size() < userPosts.size())
    {
        auto* card = new PostCardComponent();
        card->onPlayClicked = [this](const FeedPost& post) {
            Log::debug("ProfileComponent::updatePostCards: Play clicked for post: " + post.id);
            if (onPlayClicked)
                onPlayClicked(post);
            else
                Log::warn("ProfileComponent::updatePostCards: Play clicked but callback not set");
        };
        card->onPauseClicked = [this](const FeedPost& post) {
            Log::debug("ProfileComponent::updatePostCards: Pause clicked for post: " + post.id);
            if (onPauseClicked)
                onPauseClicked(post);
            else
                Log::warn("ProfileComponent::updatePostCards: Pause clicked but callback not set");
        };
        card->onUserClicked = [](const FeedPost& /*post*/) {
            // Already on profile, do nothing or scroll to top
            Log::debug("ProfileComponent::updatePostCards: User clicked on post card (already on profile)");
        };
        postCards.add(card);
        addAndMakeVisible(card);
        Log::debug("ProfileComponent::updatePostCards: Created new post card #" + juce::String(postCards.size()));
    }

    // Remove extra cards
    while (postCards.size() > userPosts.size())
    {
        Log::debug("ProfileComponent::updatePostCards: Removing extra post card");
        postCards.removeLast();
    }

    // Update card data and positions
    auto postsArea = getPostsAreaBounds();
    int y = HEADER_HEIGHT - scrollOffset;
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
            Log::debug("ProfileComponent::updatePostCards: Post " + userPosts[i].id + " is currently playing");
        }
        else
        {
            card->setIsPlaying(false);
            card->setPlaybackProgress(0.0f);
        }

        // Show/hide based on visibility
        bool isVisible = (y + POST_CARD_HEIGHT > HEADER_HEIGHT) && (y < getHeight());
        card->setVisible(isVisible);
        if (isVisible)
            visibleCount++;

        y += POST_CARD_HEIGHT + 10;
    }
    
    Log::debug("ProfileComponent::updatePostCards: Updated " + juce::String(userPosts.size()) + " post cards, " + juce::String(visibleCount) + " visible");
}

int ProfileComponent::calculateContentHeight() const
{
    return static_cast<int>(userPosts.size()) * (POST_CARD_HEIGHT + 10);
}

//==============================================================================
void ProfileComponent::setCurrentlyPlayingPost(const juce::String& postId)
{
    Log::debug("ProfileComponent::setCurrentlyPlayingPost: Setting playing post - postId: " + postId);
    currentlyPlayingPostId = postId;
    updatePostCards();
}

void ProfileComponent::setPlaybackProgress(float progress)
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

void ProfileComponent::clearPlayingState()
{
    Log::debug("ProfileComponent::clearPlayingState: Clearing playing state");
    currentlyPlayingPostId = "";
    currentPlaybackProgress = 0.0f;

    for (auto* card : postCards)
    {
        card->setIsPlaying(false);
        card->setPlaybackProgress(0.0f);
    }
}

//==============================================================================
void ProfileComponent::showFollowersList(const juce::String& userId, FollowersListComponent::ListType type)
{
    if (followersListPanel == nullptr || userId.isEmpty())
    {
        Log::warn("ProfileComponent::showFollowersList: Cannot show list - panel: " + juce::String(followersListPanel != nullptr ? "valid" : "null") + ", userId: " + userId);
        return;
    }

    juce::String typeStr = type == FollowersListComponent::ListType::Followers ? "Followers" : "Following";
    Log::info("ProfileComponent::showFollowersList: Showing " + typeStr + " list for userId: " + userId);

    // Set up the panel
    followersListPanel->setNetworkClient(networkClient);
    followersListPanel->setCurrentUserId(currentUserId);

    // Position panel on right side (40% of width, max 350px)
    int panelWidth = juce::jmin(350, static_cast<int>(getWidth() * 0.4f));
    followersListPanel->setBounds(getWidth() - panelWidth, 0, panelWidth, getHeight());
    Log::debug("ProfileComponent::showFollowersList: Panel positioned - width: " + juce::String(panelWidth));

    // Load the list
    followersListPanel->loadList(userId, type);

    followersListPanel->setVisible(true);
    followersListPanel->toFront(true);
    followersListVisible = true;
    Log::debug("ProfileComponent::showFollowersList: Followers list panel shown");
}

void ProfileComponent::hideFollowersList()
{
    Log::debug("ProfileComponent::hideFollowersList: Hiding followers list panel");
    if (followersListPanel)
    {
        followersListPanel->setVisible(false);
    }
    followersListVisible = false;
}
