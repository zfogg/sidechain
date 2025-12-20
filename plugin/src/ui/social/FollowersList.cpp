#include "FollowersList.h"
#include "../../stores/AppStore.h"
#include "../../stores/EntityStore.h"
#include "../../models/User.h"

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
    repaint();
  };
}

void FollowUserRow::setUser(const std::shared_ptr<const Sidechain::User> &user) {
  userPtr = user;
  if (!userPtr) {
    Log::warn("FollowUserRow: Setting user to null");
    return;
  }

  // Fetch avatar image via AppStore reactive observable (with caching)
  if (appStore && userPtr->avatarUrl.isNotEmpty()) {
    juce::Component::SafePointer<FollowUserRow> safeThis(this);
    appStore->loadImageObservable(userPtr->avatarUrl)
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

void FollowUserRow::paint(juce::Graphics &g) {
  if (!userPtr) {
    return;
  }

  // Background
  g.setColour(hoverState.isHovered() ? SidechainColors::backgroundLighter() : SidechainColors::backgroundLight());
  g.fillRect(getLocalBounds());

  // Border at bottom
  g.setColour(SidechainColors::border());
  g.drawLine(0.0f, static_cast<float>(getHeight() - 1), static_cast<float>(getWidth()),
             static_cast<float>(getHeight() - 1), 0.5f);

  auto avatarBounds = getAvatarBounds();

  // Avatar - display name or username
  juce::String displayName(userPtr->displayName);
  juce::String username(userPtr->username);
  juce::String name = !displayName.isEmpty() ? displayName : username;

  // Use reactive observable for image loading (with caching)
  if (appStore && userPtr->avatarUrl.isNotEmpty()) {
    juce::String avatarUrl(userPtr->avatarUrl);
    juce::Component::SafePointer<FollowUserRow> safeThis(this);
    appStore->loadImageObservable(avatarUrl).subscribe(
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

  // Fallback: colored circle with user initials
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
  if (!displayName.isEmpty() && displayName != username) {
    g.setColour(SidechainColors::textMuted());
    g.setFont(12.0f);
    g.drawText("@" + username, textX, 32, textWidth, 16, juce::Justification::centredLeft);
  }

  // Follow button
  auto followBounds = getFollowButtonBounds();
  bool isFollowing = userPtr->isFollowing;

  if (isFollowing) {
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
  if (!userPtr) {
    return;
  }

  auto pos = event.getPosition();

  // Check follow button click
  if (getFollowButtonBounds().contains(pos)) {
    if (onFollowToggled) {
      onFollowToggled(juce::String(userPtr->id), !userPtr->isFollowing);
    }
    return;
  }

  // Otherwise, navigate to user
  if (onUserClicked) {
    onUserClicked(juce::String(userPtr->id));
  }
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

void FollowersList::loadList(const juce::String &userId, ListType type) {
  if (userId.isEmpty() || appStore == nullptr) {
    Log::warn("FollowersList: Cannot load list - userId empty or appStore null");
    return;
  }

  juce::String typeStr = type == ListType::Followers ? "followers" : "following";
  Log::info("FollowersList: Loading " + typeStr + " for user: " + userId);

  // Redux: Subscribe to immutable FollowersSlice from AppStore
  // Component gets notified whenever the slice changes (loading, users, errors)
  auto &sliceManager = Sidechain::Stores::Slices::AppSliceManager::getInstance();
  sliceManager.getFollowersSlice()->subscribe([this](const Sidechain::Stores::FollowersState &state) {
    // Received new immutable state snapshot
    currentSlice = state;
    updateUsersList();
    repaint();
  });

  // Redux: Dispatch action to load followers or following
  // Action will normalize JSON → cache in EntityStore → update FollowersSlice
  if (type == ListType::Followers) {
    appStore->loadFollowers(userId, 20, 0);
  } else {
    appStore->loadFollowing(userId, 20, 0);
  }
}

void FollowersList::refresh() {
  if (currentSlice.targetUserId.empty()) {
    return;
  }

  // Re-load from the target user stored in current slice
  auto targetUserIdStr = juce::String(currentSlice.targetUserId);
  auto type = (currentSlice.listType == FollowersState::Followers) ? ListType::Followers : ListType::Following;
  loadList(targetUserIdStr, type);
}

void FollowersList::updateUsersList() {
  // Render users from immutable FollowersSlice state
  userRows.clear();

  int yPos = 0;
  for (const auto &user : currentSlice.users) {
    if (!user) {
      continue;
    }

    auto *row = new FollowUserRow();
    row->setAppStore(appStore);
    row->setUser(user); // Pass immutable const User*
    setupRowCallbacks(row);
    row->setBounds(0, yPos, contentContainer->getWidth(), FollowUserRow::ROW_HEIGHT);
    contentContainer->addAndMakeVisible(row);
    userRows.add(row);

    yPos += FollowUserRow::ROW_HEIGHT;
  }

  contentContainer->setSize(viewport->getWidth() - 10, yPos);
}

void FollowersList::setupRowCallbacks(FollowUserRow *row) {
  // Dispatch user click action
  row->onUserClicked = [this](const juce::String &userId) {
    if (onUserClicked) {
      onUserClicked(userId);
    }
  };

  // Dispatch follow/unfollow action through AppStore
  row->onFollowToggled = [this](const juce::String &userId, bool willFollow) {
    if (appStore == nullptr) {
      return;
    }

    // Redux: Dispatch action to follow/unfollow user
    if (willFollow) {
      appStore->followUser(userId);
    } else {
      appStore->unfollowUser(userId);
    }
  };
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

  // Header title from immutable slice state
  g.setColour(SidechainColors::textPrimary());
  g.setFont(16.0f);
  juce::String title = (currentSlice.listType == FollowersState::Followers) ? "Followers" : "Following";
  if (currentSlice.totalCount > 0) {
    title += " (" + juce::String(currentSlice.totalCount) + ")";
  }
  g.drawText(title, headerBounds.withTrimmedLeft(15), juce::Justification::centredLeft);

  // Loading indicator
  if (currentSlice.isLoading && currentSlice.users.empty()) {
    g.setColour(SidechainColors::textMuted());
    g.setFont(12.0f);
    g.drawText("Loading...", getLocalBounds(), juce::Justification::centred);
  }

  // Error message
  if (!currentSlice.errorMessage.empty()) {
    g.setColour(SidechainColors::buttonDanger());
    g.setFont(12.0f);
    g.drawText(juce::String(currentSlice.errorMessage), getLocalBounds(), juce::Justification::centred);
  }

  // Empty state
  if (!currentSlice.isLoading && currentSlice.users.empty() && currentSlice.errorMessage.empty()) {
    g.setColour(SidechainColors::textMuted());
    g.setFont(14.0f);
    juce::String emptyText =
        (currentSlice.listType == FollowersState::Followers) ? "No followers yet" : "Not following anyone yet";
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
