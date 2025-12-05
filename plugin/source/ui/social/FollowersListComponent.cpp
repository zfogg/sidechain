#include "FollowersListComponent.h"
#include "../../network/NetworkClient.h"
#include "../../util/Json.h"
#include "../../util/ImageCache.h"
#include "../../util/Colors.h"
#include "../../util/UIHelpers.h"

//==============================================================================
// FollowUserRowComponent Implementation
//==============================================================================

FollowUserRowComponent::FollowUserRowComponent()
{
    setSize(400, ROW_HEIGHT);
}

void FollowUserRowComponent::setUser(const FollowListUser& newUser)
{
    user = newUser;
    avatarImage = juce::Image();

    // Load avatar via ImageCache
    if (user.avatarUrl.isNotEmpty())
    {
        ImageLoader::load(user.avatarUrl, [this](const juce::Image& img) {
            avatarImage = img;
            repaint();
        });
    }

    repaint();
}

void FollowUserRowComponent::setFollowing(bool following)
{
    user.isFollowing = following;
    repaint();
}

void FollowUserRowComponent::paint(juce::Graphics& g)
{
    // Background
    g.setColour(isHovered ? SidechainColors::backgroundLighter() : SidechainColors::backgroundLight());
    g.fillRect(getLocalBounds());

    // Border at bottom
    g.setColour(SidechainColors::border());
    g.drawLine(0.0f, static_cast<float>(getHeight() - 1), static_cast<float>(getWidth()), static_cast<float>(getHeight() - 1), 0.5f);

    auto avatarBounds = getAvatarBounds();

    // Avatar
    juce::String name = user.displayName.isNotEmpty() ? user.displayName : user.username;
    ImageLoader::drawCircularAvatar(g, avatarBounds, avatarImage,
        ImageLoader::getInitials(name),
        SidechainColors::surface(),
        SidechainColors::textPrimary(),
        18.0f);

    // Avatar border
    g.setColour(SidechainColors::border());
    g.drawEllipse(avatarBounds.toFloat(), 1.0f);

    // User info
    int textX = avatarBounds.getRight() + 12;
    int textWidth = getFollowButtonBounds().getX() - textX - 10;

    // Display name or username
    g.setColour(SidechainColors::textPrimary());
    g.setFont(15.0f);
    g.drawText(name, textX, 12, textWidth, 20, juce::Justification::centredLeft);

    // @username (if display name is different)
    if (user.displayName.isNotEmpty() && user.displayName != user.username)
    {
        g.setColour(SidechainColors::textMuted());
        g.setFont(12.0f);
        g.drawText("@" + user.username, textX, 32, textWidth, 16, juce::Justification::centredLeft);
    }

    // "Follows you" badge
    if (user.followsYou)
    {
        g.setColour(SidechainColors::textMuted());
        g.setFont(10.0f);
        int badgeY = user.displayName.isNotEmpty() ? 48 : 32;
        g.drawText("Follows you", textX, badgeY, 80, 14, juce::Justification::centredLeft);
    }

    // Follow button (don't show for current user)
    auto followBounds = getFollowButtonBounds();

    if (user.isFollowing)
    {
        UI::drawOutlineButton(g, followBounds, "Following",
            SidechainColors::border(), SidechainColors::textPrimary(), false, 4.0f);
    }
    else
    {
        UI::drawButton(g, followBounds, "Follow",
            SidechainColors::accent(), SidechainColors::background(), false, 4.0f);
    }
}

void FollowUserRowComponent::resized()
{
    // Layout handled in paint
}

void FollowUserRowComponent::mouseUp(const juce::MouseEvent& event)
{
    auto pos = event.getPosition();

    // Check follow button click
    if (getFollowButtonBounds().contains(pos))
    {
        if (onFollowToggled)
            onFollowToggled(user, !user.isFollowing);
        return;
    }

    // Otherwise, navigate to user
    if (onUserClicked)
        onUserClicked(user);
}

void FollowUserRowComponent::mouseEnter(const juce::MouseEvent& /*event*/)
{
    isHovered = true;
    repaint();
}

void FollowUserRowComponent::mouseExit(const juce::MouseEvent& /*event*/)
{
    isHovered = false;
    repaint();
}

juce::Rectangle<int> FollowUserRowComponent::getAvatarBounds() const
{
    return juce::Rectangle<int>(15, 10, 50, 50);
}

juce::Rectangle<int> FollowUserRowComponent::getFollowButtonBounds() const
{
    return juce::Rectangle<int>(getWidth() - 95, 20, 80, 30);
}

//==============================================================================
// FollowersListComponent Implementation
//==============================================================================

FollowersListComponent::FollowersListComponent()
{
    setupUI();
}

FollowersListComponent::~FollowersListComponent()
{
    stopTimer();
}

void FollowersListComponent::setupUI()
{
    // Close button
    closeButton = std::make_unique<juce::TextButton>("X");
    closeButton->onClick = [this]() {
        if (onClose)
            onClose();
    };
    addAndMakeVisible(closeButton.get());

    // Viewport for scrollable list
    viewport = std::make_unique<juce::Viewport>();
    contentContainer = std::make_unique<juce::Component>();
    viewport->setViewedComponent(contentContainer.get(), false);
    viewport->setScrollBarsShown(true, false);
    addAndMakeVisible(viewport.get());
}

void FollowersListComponent::loadList(const juce::String& userId, ListType type)
{
    if (userId.isEmpty() || networkClient == nullptr)
        return;

    targetUserId = userId;
    listType = type;
    currentOffset = 0;
    users.clear();
    userRows.clear();
    errorMessage = "";
    isLoading = true;
    repaint();

    auto callback = [this](bool success, const juce::var& data) {
        handleUsersLoaded(success, data);
    };

    if (type == ListType::Followers)
        networkClient->getFollowers(userId, 20, 0, callback);
    else
        networkClient->getFollowing(userId, 20, 0, callback);
}

void FollowersListComponent::refresh()
{
    if (targetUserId.isEmpty())
        return;

    loadList(targetUserId, listType);
}

void FollowersListComponent::handleUsersLoaded(bool success, const juce::var& usersData)
{
    isLoading = false;

    if (success && Json::isObject(usersData))
    {
        // Parse users array
        juce::String usersKey = (listType == ListType::Followers) ? "followers" : "following";
        auto usersArray = Json::getArray(usersData, usersKey.toRawUTF8());

        if (Json::isArray(usersArray))
        {
            auto* arr = usersArray.getArray();
            if (arr != nullptr)
            {
                for (const auto& item : *arr)
                {
                    FollowListUser user = FollowListUser::fromJson(item);
                    if (user.isValid())
                        users.add(user);
                }
            }
        }

        totalCount = Json::getInt(usersData, "total_count", users.size());
        hasMore = users.size() < totalCount;
        currentOffset = users.size();

        updateUsersList();
    }
    else
    {
        errorMessage = "Failed to load " + juce::String(listType == ListType::Followers ? "followers" : "following");
    }

    repaint();
}

void FollowersListComponent::loadMoreUsers()
{
    if (isLoading || !hasMore || networkClient == nullptr)
        return;

    isLoading = true;
    repaint();

    auto callback = [this](bool success, const juce::var& data) {
        isLoading = false;

        if (success && Json::isObject(data))
        {
            juce::String usersKey = (listType == ListType::Followers) ? "followers" : "following";
            auto usersArray = Json::getArray(data, usersKey.toRawUTF8());

            if (Json::isArray(usersArray))
            {
                auto* arr = usersArray.getArray();
                if (arr != nullptr)
                {
                    for (const auto& item : *arr)
                    {
                        FollowListUser user = FollowListUser::fromJson(item);
                        if (user.isValid())
                            users.add(user);
                    }
                }
            }

            totalCount = Json::getInt(data, "total_count", users.size());
            hasMore = users.size() < totalCount;
            currentOffset = users.size();

            updateUsersList();
        }

        repaint();
    };

    if (listType == ListType::Followers)
        networkClient->getFollowers(targetUserId, 20, currentOffset, callback);
    else
        networkClient->getFollowing(targetUserId, 20, currentOffset, callback);
}

void FollowersListComponent::updateUsersList()
{
    userRows.clear();

    int yPos = 0;
    for (const auto& user : users)
    {
        auto* row = new FollowUserRowComponent();
        row->setUser(user);
        setupRowCallbacks(row);
        row->setBounds(0, yPos, contentContainer->getWidth(), FollowUserRowComponent::ROW_HEIGHT);
        contentContainer->addAndMakeVisible(row);
        userRows.add(row);

        yPos += FollowUserRowComponent::ROW_HEIGHT;
    }

    contentContainer->setSize(viewport->getWidth() - 10, yPos);
}

void FollowersListComponent::setupRowCallbacks(FollowUserRowComponent* row)
{
    row->onUserClicked = [this](const FollowListUser& user) {
        if (onUserClicked)
            onUserClicked(user.id);
    };

    row->onFollowToggled = [this](const FollowListUser& user, bool willFollow) {
        handleFollowToggled(user, willFollow);
    };
}

void FollowersListComponent::handleFollowToggled(const FollowListUser& user, bool willFollow)
{
    if (networkClient == nullptr)
        return;

    // Optimistic update
    for (auto* row : userRows)
    {
        if (row->getUser().id == user.id)
        {
            row->setFollowing(willFollow);
            break;
        }
    }

    // Send to server
    auto revertCallback = [this, userId = user.id, wasFollowing = user.isFollowing](bool success, const juce::var& /*response*/) {
        if (!success)
        {
            // Revert on failure
            for (auto* row : userRows)
            {
                if (row->getUser().id == userId)
                {
                    row->setFollowing(wasFollowing);
                    break;
                }
            }
        }
    };

    if (willFollow)
        networkClient->followUser(user.id, revertCallback);
    else
        networkClient->unfollowUser(user.id, revertCallback);
}

void FollowersListComponent::timerCallback()
{
    // Auto-refresh every 60 seconds
    refresh();
}

void FollowersListComponent::paint(juce::Graphics& g)
{
    // Background
    g.fillAll(SidechainColors::background());

    // Header
    auto headerBounds = getLocalBounds().removeFromTop(HEADER_HEIGHT);
    UI::drawCard(g, headerBounds, SidechainColors::backgroundLight());

    // Header title
    g.setColour(SidechainColors::textPrimary());
    g.setFont(16.0f);
    juce::String title = (listType == ListType::Followers) ? "Followers" : "Following";
    if (totalCount > 0)
        title += " (" + juce::String(totalCount) + ")";
    g.drawText(title, headerBounds.withTrimmedLeft(15), juce::Justification::centredLeft);

    // Loading indicator
    if (isLoading && users.isEmpty())
    {
        g.setColour(SidechainColors::textMuted());
        g.setFont(12.0f);
        g.drawText("Loading...", getLocalBounds(), juce::Justification::centred);
    }

    // Error message
    if (errorMessage.isNotEmpty())
    {
        g.setColour(SidechainColors::buttonDanger());
        g.setFont(12.0f);
        g.drawText(errorMessage, getLocalBounds(), juce::Justification::centred);
    }

    // Empty state
    if (!isLoading && users.isEmpty() && errorMessage.isEmpty())
    {
        g.setColour(SidechainColors::textMuted());
        g.setFont(14.0f);
        juce::String emptyText = (listType == ListType::Followers)
            ? "No followers yet"
            : "Not following anyone yet";
        g.drawText(emptyText, getLocalBounds(), juce::Justification::centred);
    }
}

void FollowersListComponent::resized()
{
    auto bounds = getLocalBounds();

    // Close button
    closeButton->setBounds(bounds.getWidth() - 45, 10, 30, 30);

    // Header at top
    bounds.removeFromTop(HEADER_HEIGHT);

    // Viewport fills the rest
    viewport->setBounds(bounds);
    contentContainer->setSize(viewport->getWidth() - 10, contentContainer->getHeight());
    updateUsersList();
}
