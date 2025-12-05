#include "ProfileComponent.h"
#include "../../network/NetworkClient.h"
#include "../feed/PostCardComponent.h"

//==============================================================================
// UserProfile implementation
//==============================================================================
UserProfile UserProfile::fromJson(const juce::var& json)
{
    UserProfile profile;

    if (!json.isObject())
        return profile;

    profile.id = json.getProperty("id", "").toString();
    profile.username = json.getProperty("username", "").toString();
    profile.displayName = json.getProperty("display_name", "").toString();
    profile.bio = json.getProperty("bio", "").toString();
    profile.location = json.getProperty("location", "").toString();
    profile.avatarUrl = json.getProperty("avatar_url", "").toString();
    profile.profilePictureUrl = json.getProperty("profile_picture_url", "").toString();
    profile.dawPreference = json.getProperty("daw_preference", "").toString();
    profile.genre = json.getProperty("genre", "").toString();
    profile.socialLinks = json.getProperty("social_links", juce::var());
    profile.followerCount = static_cast<int>(json.getProperty("follower_count", 0));
    profile.followingCount = static_cast<int>(json.getProperty("following_count", 0));
    profile.postCount = static_cast<int>(json.getProperty("post_count", 0));
    profile.isFollowing = static_cast<bool>(json.getProperty("is_following", false));
    profile.isFollowedBy = static_cast<bool>(json.getProperty("is_followed_by", false));

    // Parse created_at timestamp
    juce::String createdAtStr = json.getProperty("created_at", "").toString();
    if (createdAtStr.isNotEmpty())
    {
        // Try to parse ISO 8601 format
        profile.createdAt = juce::Time::fromISO8601(createdAtStr);
    }

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
    setSize(600, 800);

    scrollBar = std::make_unique<juce::ScrollBar>(true);
    scrollBar->addListener(this);
    scrollBar->setAutoHide(true);
    addAndMakeVisible(scrollBar.get());

    // Create followers list panel (initially hidden)
    followersListPanel = std::make_unique<FollowersListComponent>();
    followersListPanel->onClose = [this]() {
        hideFollowersList();
    };
    followersListPanel->onUserClicked = [this](const juce::String& userId) {
        hideFollowersList();
        // Navigate to the clicked user's profile
        loadProfile(userId);
    };
    addChildComponent(followersListPanel.get());
}

ProfileComponent::~ProfileComponent()
{
    scrollBar->removeListener(this);
}

//==============================================================================
void ProfileComponent::loadProfile(const juce::String& userId)
{
    if (userId.isEmpty())
        return;

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
        return;

    loadProfile(currentUserId);
}

void ProfileComponent::setProfile(const UserProfile& newProfile)
{
    profile = newProfile;
    isLoading = false;
    hasError = false;
    avatarLoadRequested = false;
    avatarImage = juce::Image();
    repaint();

    // Fetch user's posts
    if (profile.id.isNotEmpty())
        fetchUserPosts(profile.id);
}

void ProfileComponent::refresh()
{
    if (profile.id.isNotEmpty())
        loadProfile(profile.id);
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
    // Create circular clipping path
    juce::Path circlePath;
    circlePath.addEllipse(bounds.toFloat());

    g.saveState();
    g.reduceClipRegion(circlePath);

    if (avatarImage.isValid())
    {
        auto scaledImage = avatarImage.rescaled(bounds.getWidth(), bounds.getHeight(),
                                                 juce::Graphics::highResamplingQuality);
        g.drawImageAt(scaledImage, bounds.getX(), bounds.getY());
    }
    else
    {
        // Draw placeholder with gradient
        g.setGradientFill(juce::ColourGradient(
            Colors::accent.darker(0.3f),
            static_cast<float>(bounds.getX()), static_cast<float>(bounds.getY()),
            Colors::accent.darker(0.6f),
            static_cast<float>(bounds.getRight()), static_cast<float>(bounds.getBottom()),
            true
        ));
        g.fillEllipse(bounds.toFloat());

        // Draw initial
        g.setColour(Colors::textPrimary);
        g.setFont(juce::Font(36.0f, juce::Font::bold));
        juce::String initial = profile.displayName.isEmpty()
            ? (profile.username.isEmpty() ? "?" : profile.username.substring(0, 1).toUpperCase())
            : profile.displayName.substring(0, 1).toUpperCase();
        g.drawText(initial, bounds, juce::Justification::centred);
    }

    g.restoreState();

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
    g.drawText(juce::String(profile.postCount), postsBounds.withHeight(22), juce::Justification::centred);
    g.setColour(Colors::textSecondary);
    g.setFont(12.0f);
    g.drawText("Posts", postsBounds.withY(postsBounds.getY() + 22).withHeight(20), juce::Justification::centred);

    // Followers count (tappable)
    auto followersBounds = juce::Rectangle<int>(bounds.getX() + statSpacing, bounds.getY(), statSpacing, bounds.getHeight());
    g.setColour(Colors::textPrimary);
    g.setFont(juce::Font(18.0f, juce::Font::bold));
    g.drawText(juce::String(profile.followerCount), followersBounds.withHeight(22), juce::Justification::centred);
    g.setColour(Colors::textSecondary);
    g.setFont(12.0f);
    g.drawText("Followers", followersBounds.withY(followersBounds.getY() + 22).withHeight(20), juce::Justification::centred);

    // Following count (tappable)
    auto followingBounds = juce::Rectangle<int>(bounds.getX() + statSpacing * 2, bounds.getY(), statSpacing, bounds.getHeight());
    g.setColour(Colors::textPrimary);
    g.setFont(juce::Font(18.0f, juce::Font::bold));
    g.drawText(juce::String(profile.followingCount), followingBounds.withHeight(22), juce::Justification::centred);
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
        // Follow/Following button
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
    auto bounds = getLocalBounds();

    // Position scroll bar
    scrollBar->setBounds(bounds.getRight() - 10, HEADER_HEIGHT, 10, bounds.getHeight() - HEADER_HEIGHT);

    // Update scroll bar range
    int contentHeight = calculateContentHeight();
    int visibleHeight = bounds.getHeight() - HEADER_HEIGHT;
    scrollBar->setRangeLimits(0.0, static_cast<double>(contentHeight));
    scrollBar->setCurrentRange(static_cast<double>(scrollOffset), static_cast<double>(visibleHeight));

    // Position post cards
    updatePostCards();

    // Reposition followers list panel if visible
    if (followersListPanel && followersListVisible)
    {
        int panelWidth = juce::jmin(350, static_cast<int>(getWidth() * 0.4f));
        followersListPanel->setBounds(getWidth() - panelWidth, 0, panelWidth, getHeight());
    }
}

void ProfileComponent::mouseUp(const juce::MouseEvent& event)
{
    auto pos = event.getPosition();

    // Back button
    if (getBackButtonBounds().contains(pos))
    {
        if (onBackPressed)
            onBackPressed();
        return;
    }

    // Share button
    if (getShareButtonBounds().contains(pos))
    {
        shareProfile();
        return;
    }

    // Followers stat
    if (getFollowersBounds().contains(pos))
    {
        showFollowersList(profile.id, FollowersListComponent::ListType::Followers);
        if (onFollowersClicked)
            onFollowersClicked(profile.id);
        return;
    }

    // Following stat
    if (getFollowingBounds().contains(pos))
    {
        showFollowersList(profile.id, FollowersListComponent::ListType::Following);
        if (onFollowingClicked)
            onFollowingClicked(profile.id);
        return;
    }

    // Follow/Edit button
    if (profile.isOwnProfile(currentUserId))
    {
        if (getEditButtonBounds().contains(pos))
        {
            if (onEditProfile)
                onEditProfile();
            return;
        }
    }
    else
    {
        if (getFollowButtonBounds().contains(pos))
        {
            handleFollowToggle();
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
            refresh();
            return;
        }
    }
}

void ProfileComponent::scrollBarMoved(juce::ScrollBar* /*scrollBarThatHasMoved*/, double newRangeStart)
{
    scrollOffset = static_cast<int>(newRangeStart);
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
    return juce::Rectangle<int>(PADDING, buttonsY, getWidth() - PADDING * 2, BUTTON_HEIGHT);
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
        hasError = true;
        errorMessage = "Network not available";
        isLoading = false;
        repaint();
        return;
    }

    juce::String endpoint = "/users/" + userId + "/profile";

    networkClient->get(endpoint, [this](bool success, const juce::var& response) {
        juce::MessageManager::callAsync([this, success, response]() {
            isLoading = false;

            if (success && response.isObject())
            {
                setProfile(UserProfile::fromJson(response));
            }
            else
            {
                hasError = true;
                if (response.isObject() && response.hasProperty("error"))
                    errorMessage = response["error"].toString();
                else
                    errorMessage = "Failed to load profile";
            }

            repaint();
        });
    });
}

void ProfileComponent::fetchUserPosts(const juce::String& userId)
{
    if (networkClient == nullptr)
        return;

    juce::String endpoint = "/users/" + userId + "/posts?limit=20";

    networkClient->get(endpoint, [this](bool success, const juce::var& response) {
        juce::MessageManager::callAsync([this, success, response]() {
            if (success && response.isObject())
            {
                userPosts.clear();

                auto postsArray = response.getProperty("posts", juce::var());
                if (postsArray.isArray())
                {
                    for (int i = 0; i < postsArray.size(); ++i)
                    {
                        FeedPost post = FeedPost::fromJson(postsArray[i]);
                        if (post.isValid())
                            userPosts.add(post);
                    }
                }

                updatePostCards();
            }

            repaint();
        });
    });
}

void ProfileComponent::handleFollowToggle()
{
    if (networkClient == nullptr || profile.id.isEmpty())
        return;

    bool wasFollowing = profile.isFollowing;

    // Optimistic UI update
    profile.isFollowing = !profile.isFollowing;
    profile.followerCount += profile.isFollowing ? 1 : -1;
    repaint();

    juce::String endpoint = "/users/" + profile.id + (wasFollowing ? "/unfollow" : "/follow");

    networkClient->post(endpoint, juce::var(), [this, wasFollowing](bool success, const juce::var& /*response*/) {
        juce::MessageManager::callAsync([this, success, wasFollowing]() {
            if (!success)
            {
                // Revert on failure
                profile.isFollowing = wasFollowing;
                profile.followerCount += wasFollowing ? 1 : -1;
                repaint();
            }
            else if (onFollowToggled)
            {
                onFollowToggled(profile.id);
            }
        });
    });
}

void ProfileComponent::shareProfile()
{
    juce::String profileUrl = "https://sidechain.live/user/" + profile.username;
    juce::SystemClipboard::copyTextToClipboard(profileUrl);

    // Could show a toast notification here
    DBG("Profile link copied: " + profileUrl);
}

//==============================================================================
void ProfileComponent::updatePostCards()
{
    // Create or update post cards
    while (postCards.size() < userPosts.size())
    {
        auto* card = new PostCardComponent();
        card->onPlayClicked = [this](const FeedPost& post) {
            if (onPlayClicked)
                onPlayClicked(post);
        };
        card->onPauseClicked = [this](const FeedPost& post) {
            if (onPauseClicked)
                onPauseClicked(post);
        };
        card->onUserClicked = [this](const FeedPost& /*post*/) {
            // Already on profile, do nothing or scroll to top
        };
        postCards.add(card);
        addAndMakeVisible(card);
    }

    // Remove extra cards
    while (postCards.size() > userPosts.size())
    {
        postCards.removeLast();
    }

    // Update card data and positions
    auto postsArea = getPostsAreaBounds();
    int y = HEADER_HEIGHT - scrollOffset;

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
        }
        else
        {
            card->setIsPlaying(false);
            card->setPlaybackProgress(0.0f);
        }

        // Show/hide based on visibility
        bool isVisible = (y + POST_CARD_HEIGHT > HEADER_HEIGHT) && (y < getHeight());
        card->setVisible(isVisible);

        y += POST_CARD_HEIGHT + 10;
    }
}

int ProfileComponent::calculateContentHeight() const
{
    return static_cast<int>(userPosts.size()) * (POST_CARD_HEIGHT + 10);
}

//==============================================================================
void ProfileComponent::setCurrentlyPlayingPost(const juce::String& postId)
{
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
        return;

    // Set up the panel
    followersListPanel->setNetworkClient(networkClient);
    followersListPanel->setCurrentUserId(currentUserId);

    // Position panel on right side (40% of width, max 350px)
    int panelWidth = juce::jmin(350, static_cast<int>(getWidth() * 0.4f));
    followersListPanel->setBounds(getWidth() - panelWidth, 0, panelWidth, getHeight());

    // Load the list
    followersListPanel->loadList(userId, type);

    followersListPanel->setVisible(true);
    followersListPanel->toFront(true);
    followersListVisible = true;
}

void ProfileComponent::hideFollowersList()
{
    if (followersListPanel)
    {
        followersListPanel->setVisible(false);
    }
    followersListVisible = false;
}
