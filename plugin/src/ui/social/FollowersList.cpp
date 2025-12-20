#include "FollowersList.h"
#include "../../network/NetworkClient.h"
#include "../../stores/AppStore.h"

#include "../../util/Colors.h"
#include "../../util/Json.h"
#include "../../util/Log.h"
#include "../../util/Result.h"
#include "../../util/StringUtils.h"
#include "../../util/UIHelpers.h"

using namespace Sidechain::Stores;

// ==============================================================================
// FollowUserRow Implementation
// ==============================================================================

FollowUserRow::FollowUserRow() {
  setSize(400, ROW_HEIGHT);

  // Set up hover state - triggers visual feedback on hover
  hoverState.onHoverChanged = [this](bool hovered) {
    // Repaint to show/hide hover effects (highlight, follow button)
    if (hovered) {
      // Show hover effects (row highlight, follow/unfollow button)
      repaint();
    } else {
      // Hide hover effects when mouse leaves
      repaint();
    }
  };
}

void FollowUserRow::setUser(const FollowListUser &newUser) {
  user = newUser;
  // Fetch avatar image via AppStore reactive observable (with caching)
  if (user.avatarUrl.isNotEmpty() && appStore) {
    juce::Component::SafePointer<FollowUserRow> safeThis(this);
    appStore->loadImageObservable(user.avatarUrl)
        .subscribe(
            [safeThis](const juce::Image &image) {
              if (safeThis == nullptr)
                return;
              if (image.isValid())
                safeThis->repaint();
            },
            [safeThis](std::exception_ptr) {
              if (safeThis == nullptr)
                return;
              Log::warn("FollowersList: Failed to load user avatar");
            });
  }
  repaint();
}

void FollowUserRow::setFollowing(bool following) {
  user.isFollowing = following;
  repaint();
}

void FollowUserRow::paint(juce::Graphics &g) {
  // Background
  g.setColour(hoverState.isHovered() ? SidechainColors::backgroundLighter() : SidechainColors::backgroundLight());
  g.fillRect(getLocalBounds());

  // Border at bottom
  g.setColour(SidechainColors::border());
  g.drawLine(0.0f, static_cast<float>(getHeight() - 1), static_cast<float>(getWidth()),
             static_cast<float>(getHeight() - 1), 0.5f);

  auto avatarBounds = getAvatarBounds();

  // Avatar
  juce::String name = user.displayName.isNotEmpty() ? user.displayName : user.username;

  // Use reactive observable for image loading (with caching)
  if (appStore && user.avatarUrl.isNotEmpty()) {
    juce::Component::SafePointer<FollowUserRow> safeThis(this);
    appStore->loadImageObservable(user.avatarUrl)
        .subscribe(
            [safeThis](const juce::Image &image) {
              if (safeThis == nullptr)
                return;
              if (image.isValid())
                safeThis->repaint();
            },
            [safeThis](std::exception_ptr) {
              if (safeThis == nullptr)
                return;
              Log::warn("FollowersList: Failed to load avatar in paint");
            });
  }

  // Fallback: colored circle with user initials (will be replaced with actual image when loaded)
  juce::String initials = StringUtils::getInitials(name);

  g.setColour(SidechainColors::surface());
  g.fillEllipse(avatarBounds.toFloat());

  g.setColour(SidechainColors::textPrimary());
  g.setFont(juce::Font(juce::FontOptions().withHeight(static_cast<float>(avatarBounds.getHeight()) * 0.4f)).boldened());
  g.drawText(initials, avatarBounds, juce::Justification::centred);

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
  if (user.displayName.isNotEmpty() && user.displayName != user.username) {
    g.setColour(SidechainColors::textMuted());
    g.setFont(12.0f);
    g.drawText("@" + user.username, textX, 32, textWidth, 16, juce::Justification::centredLeft);
  }

  // "Follows you" badge
  if (user.followsYou) {
    g.setColour(SidechainColors::textMuted());
    g.setFont(10.0f);
    int badgeY = user.displayName.isNotEmpty() ? 48 : 32;
    g.drawText("Follows you", textX, badgeY, 80, 14, juce::Justification::centredLeft);
  }

  // Follow button (don't show for current user)
  auto followBounds = getFollowButtonBounds();

  if (user.isFollowing) {
    UIHelpers::drawOutlineButton(g, followBounds, "Following", SidechainColors::border(),
                                 SidechainColors::textPrimary(), false, 4.0f);
  } else {
    UIHelpers::drawButton(g, followBounds, "Follow", SidechainColors::accent(), SidechainColors::background(), false,
                          4.0f);
  }
}

void FollowUserRow::resized() {
  // Layout handled in paint
}

void FollowUserRow::mouseUp(const juce::MouseEvent &event) {
  auto pos = event.getPosition();

  // Check follow button click
  if (getFollowButtonBounds().contains(pos)) {
    if (onFollowToggled)
      onFollowToggled(user, !user.isFollowing);
    return;
  }

  // Otherwise, navigate to user
  if (onUserClicked)
    onUserClicked(user);
}

void FollowUserRow::mouseEnter(const juce::MouseEvent & /*event*/) {
  hoverState.setHovered(true);
}

void FollowUserRow::mouseExit(const juce::MouseEvent & /*event*/) {
  hoverState.setHovered(false);
}

juce::Rectangle<int> FollowUserRow::getAvatarBounds() const {
  return juce::Rectangle<int>(15, 10, 50, 50);
}

juce::Rectangle<int> FollowUserRow::getFollowButtonBounds() const {
  return juce::Rectangle<int>(getWidth() - 95, 20, 80, 30);
}

// ==============================================================================
// FollowersList Implementation
// ==============================================================================

FollowersList::FollowersList() {
  Log::info("FollowersList: Initializing");
  setupUI();
}

FollowersList::~FollowersList() {
  Log::debug("FollowersList: Destroying");
  stopTimer();
}

void FollowersList::setupUI() {
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

// ==============================================================================
// TODO: Store integration (migration to AppStore reactive pattern is planned for Phase 2)
// For now, using direct NetworkClient calls for followers/following operations

void FollowersList::loadList(const juce::String &userId, ListType type) {
  if (userId.isEmpty() || networkClient == nullptr) {
    Log::warn("FollowersList: Cannot load list - userId empty or network "
              "client null");
    return;
  }

  juce::String typeStr = type == ListType::Followers ? "followers" : "following";
  Log::info("FollowersList: Loading " + typeStr + " for user: " + userId);

  targetUserId = userId;
  listType = type;
  currentOffset = 0;
  users.clear();
  userRows.clear();
  errorMessage = "";
  isLoading = true;
  repaint();

  auto callback = [this](Outcome<juce::var> responseOutcome) {
    if (responseOutcome.isOk())
      handleUsersLoaded(true, responseOutcome.getValue());
    else
      handleUsersLoaded(false, juce::var());
  };

  if (type == ListType::Followers)
    networkClient->getFollowers(userId, 20, 0, callback);
  else
    networkClient->getFollowing(userId, 20, 0, callback);
}

void FollowersList::refresh() {
  if (targetUserId.isEmpty())
    return;

  loadList(targetUserId, listType);
}

void FollowersList::handleUsersLoaded(bool success, const juce::var &usersData) {
  isLoading = false;
  juce::String typeStr = listType == ListType::Followers ? "followers" : "following";

  if (success && Json::isObject(usersData)) {
    // Parse users array
    juce::String usersKey = (listType == ListType::Followers) ? "followers" : "following";
    auto usersArray = Json::getArray(usersData, usersKey.toRawUTF8());

    if (Json::isArray(usersArray)) {
      auto *arr = usersArray.getArray();
      if (arr != nullptr) {
        for (const auto &item : *arr) {
          FollowListUser user = FollowListUser::fromJson(item);
          if (user.isValid())
            users.add(user);
        }
      }
    }

    totalCount = Json::getInt(usersData, "total_count", users.size());
    hasMore = users.size() < totalCount;
    currentOffset = users.size();

    Log::info("FollowersList: Loaded " + juce::String(users.size()) + " " + typeStr +
              " (total: " + juce::String(totalCount) + ")");
    updateUsersList();
  } else {
    Log::error("FollowersList: Failed to load " + typeStr);
    errorMessage = "Failed to load " + typeStr;
  }

  repaint();
}

void FollowersList::loadMoreUsers() {
  if (isLoading || !hasMore || networkClient == nullptr)
    return;

  isLoading = true;
  repaint();

  auto callback = [this](Outcome<juce::var> responseOutcome) {
    isLoading = false;

    if (responseOutcome.isOk()) {
      auto data = responseOutcome.getValue();
      if (Json::isObject(data)) {
        juce::String usersKey = (listType == ListType::Followers) ? "followers" : "following";
        auto usersArray = Json::getArray(data, usersKey.toRawUTF8());

        if (Json::isArray(usersArray)) {
          auto *arr = usersArray.getArray();
          if (arr != nullptr) {
            for (const auto &item : *arr) {
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
    }

    repaint();
  };

  if (listType == ListType::Followers)
    networkClient->getFollowers(targetUserId, 20, currentOffset, callback);
  else
    networkClient->getFollowing(targetUserId, 20, currentOffset, callback);
}

void FollowersList::updateUsersList() {
  userRows.clear();

  int yPos = 0;
  for (const auto &user : users) {
    auto *row = new FollowUserRow();
    row->setUser(user);
    setupRowCallbacks(row);
    row->setBounds(0, yPos, contentContainer->getWidth(), FollowUserRow::ROW_HEIGHT);
    contentContainer->addAndMakeVisible(row);
    userRows.add(row);

    yPos += FollowUserRow::ROW_HEIGHT;
  }

  contentContainer->setSize(viewport->getWidth() - 10, yPos);
}

void FollowersList::setupRowCallbacks(FollowUserRow *row) {
  row->onUserClicked = [this](const FollowListUser &user) {
    if (onUserClicked)
      onUserClicked(user.id);
  };

  row->onFollowToggled = [this](const FollowListUser &user, bool willFollow) { handleFollowToggled(user, willFollow); };
}

void FollowersList::handleFollowToggled(const FollowListUser &user, bool willFollow) {
  if (networkClient == nullptr)
    return;

  // Optimistic update
  for (auto *row : userRows) {
    if (row->getUser().id == user.id) {
      row->setFollowing(willFollow);
      break;
    }
  }

  // Use AppStore to handle follow/unfollow with cache invalidation
  auto &appStore = AppStore::getInstance();

  if (willFollow) {
    appStore.followUser(user.id);
  } else {
    appStore.unfollowUser(user.id);
  }
}

void FollowersList::timerCallback() {
  // Auto-refresh every 60 seconds
  refresh();
}

void FollowersList::paint(juce::Graphics &g) {
  // Background
  g.fillAll(SidechainColors::background());

  // Header
  auto headerBounds = getLocalBounds().removeFromTop(HEADER_HEIGHT);
  UIHelpers::drawCard(g, headerBounds, SidechainColors::backgroundLight());

  // Header title
  g.setColour(SidechainColors::textPrimary());
  g.setFont(16.0f);
  juce::String title = (listType == ListType::Followers) ? "Followers" : "Following";
  if (totalCount > 0)
    title += " (" + juce::String(totalCount) + ")";
  g.drawText(title, headerBounds.withTrimmedLeft(15), juce::Justification::centredLeft);

  // Loading indicator
  if (isLoading && users.isEmpty()) {
    g.setColour(SidechainColors::textMuted());
    g.setFont(12.0f);
    g.drawText("Loading...", getLocalBounds(), juce::Justification::centred);
  }

  // Error message
  if (errorMessage.isNotEmpty()) {
    g.setColour(SidechainColors::buttonDanger());
    g.setFont(12.0f);
    g.drawText(errorMessage, getLocalBounds(), juce::Justification::centred);
  }

  // Empty state
  if (!isLoading && users.isEmpty() && errorMessage.isEmpty()) {
    g.setColour(SidechainColors::textMuted());
    g.setFont(14.0f);
    juce::String emptyText = (listType == ListType::Followers) ? "No followers yet" : "Not following anyone yet";
    g.drawText(emptyText, getLocalBounds(), juce::Justification::centred);
  }
}

void FollowersList::resized() {
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
