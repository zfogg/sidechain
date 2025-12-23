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
  hoverState.onHoverChanged = [this](bool /*hovered*/) { repaint(); };
}

void FollowUserRow::setUser(const std::shared_ptr<const Sidechain::User> &user) {
  userPtr = user;
  if (!userPtr) {
    Log::warn("FollowUserRow: Setting user to null");
    return;
  }

  // Fetch avatar image via AppStore reactive observable (with caching)
  if (appStore && userPtr->avatarUrl.isNotEmpty()) {
    UIHelpers::loadImageAsync<FollowUserRow>(
        this, appStore, userPtr->avatarUrl, [](FollowUserRow *comp, const juce::Image &) { comp->repaint(); },
        [](FollowUserRow *) { Log::warn("FollowersList: Failed to load user avatar"); }, "FollowersList");
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

  // NOTE: Image loading is handled in setUser() - not in paint()
  // to avoid creating new subscriptions on every repaint

  // Draw circular avatar with initials fallback
  UIHelpers::drawAvatarWithInitials(g, avatarBounds, juce::Image(), name, SidechainColors::surface(),
                                    SidechainColors::textPrimary(), SidechainColors::border());

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

FollowersList::FollowersList(Sidechain::Stores::AppStore *store)
    : AppStoreComponent(store, [store](auto cb) {
        return store ? store->subscribeToFollowers(cb) : std::function<void()>([]() {});
      }) {
  Log::info("FollowersList: Initializing");
  setupUI();
}

FollowersList::~FollowersList() {
  Log::debug("FollowersList: Destroying");
  stopTimer();
  // AppStoreComponent destructor handles storeUnsubscriber cleanup
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

  // Dispatch action to load followers or following via AppStore
  // AppStore will update FollowersState slice
  if (type == ListType::Followers) {
    appStore->loadFollowers(userId, 20, 0);
  } else {
    appStore->loadFollowing(userId, 20, 0);
  }
}

void FollowersList::refresh() {
  if (currentState.targetUserId.empty()) {
    return;
  }

  // Re-load from the target user stored in current state
  auto targetUserIdStr = juce::String(currentState.targetUserId);
  auto type = (currentState.listType == FollowersState::Followers) ? ListType::Followers : ListType::Following;
  loadList(targetUserIdStr, type);
}

void FollowersList::updateUsersList() {
  // Render users from immutable FollowersState
  userRows.clear();

  int yPos = 0;
  for (const auto &user : currentState.users) {
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

  // Header title from immutable state
  g.setColour(SidechainColors::textPrimary());
  g.setFont(16.0f);
  juce::String title = (currentState.listType == FollowersState::Followers) ? "Followers" : "Following";
  if (currentState.totalCount > 0) {
    title += " (" + juce::String(currentState.totalCount) + ")";
  }
  g.drawText(title, headerBounds.withTrimmedLeft(15), juce::Justification::centredLeft);

  // Loading indicator
  if (currentState.isLoading && currentState.users.empty()) {
    g.setColour(SidechainColors::textMuted());
    g.setFont(12.0f);
    g.drawText("Loading...", getLocalBounds(), juce::Justification::centred);
  }

  // Error message
  if (!currentState.errorMessage.empty()) {
    g.setColour(SidechainColors::buttonDanger());
    g.setFont(12.0f);
    g.drawText(juce::String(currentState.errorMessage), getLocalBounds(), juce::Justification::centred);
  }

  // Empty state
  if (!currentState.isLoading && currentState.users.empty() && currentState.errorMessage.empty()) {
    g.setColour(SidechainColors::textMuted());
    g.setFont(14.0f);
    juce::String emptyText =
        (currentState.listType == FollowersState::Followers) ? "No followers yet" : "Not following anyone yet";
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

void FollowersList::onAppStateChanged(const Sidechain::Stores::FollowersState &state) {
  Log::debug("FollowersList::onAppStateChanged: FollowersState updated");

  if (!state.errorMessage.empty()) {
    currentState.errorMessage = state.errorMessage;
    repaint();
    return;
  }

  currentState = state;
  updateUsersList();
  repaint();
}
