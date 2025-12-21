#include "NotificationList.h"
#include "../../util/Colors.h"
#include "../../util/HoverState.h"
#include "../../util/Json.h"
#include "../../util/Log.h"
#include "../../util/StringUtils.h"
#include "../../util/Time.h"
#include "../../util/UIHelpers.h"
#include <nlohmann/json.hpp>

// ==============================================================================
// NotificationItem implementation
// ==============================================================================

NotificationItem NotificationItem::fromJson(const juce::var &json) {
  NotificationItem item;

  // Parse as AggregatedFeedGroup first
  item.group = Sidechain::AggregatedFeedGroup::fromJson(json);
  item.isRead = Json::getBool(json, "is_read");
  item.isSeen = Json::getBool(json, "is_seen");

  // Extract actor info from first activity
  if (!item.group.activities.isEmpty()) {
    const auto &firstPost = item.group.activities.getFirst();
    item.actorId = firstPost.userId;
    item.actorName = firstPost.username;
    item.actorAvatarUrl = firstPost.userAvatarUrl;

    // Extract target info
    item.targetId = firstPost.id;
    item.targetPreview = firstPost.filename; // Use filename as preview

    // Determine target type from context
    if (item.group.verb == "follow")
      item.targetType = "user";
    else if (item.group.verb == "comment")
      item.targetType = "comment";
    else
      item.targetType = "loop";
  }

  // Use actor ID as name fallback
  if (item.actorName.isEmpty())
    item.actorName = item.actorId;

  return item;
}

NotificationItem NotificationItem::fromAggregatedGroup(const Sidechain::AggregatedFeedGroup &group, bool read,
                                                       bool seen) {
  NotificationItem item;
  item.group = group;
  item.isRead = read;
  item.isSeen = seen;

  // Extract actor info from first activity
  if (!group.activities.isEmpty()) {
    const auto &firstPost = group.activities.getFirst();
    item.actorId = firstPost.userId;
    item.actorName = firstPost.username;
    item.actorAvatarUrl = firstPost.userAvatarUrl;

    // Extract target info
    item.targetId = firstPost.id;
    item.targetPreview = firstPost.filename; // Use filename as preview

    // Determine target type from context
    if (group.verb == "follow")
      item.targetType = "user";
    else if (group.verb == "comment")
      item.targetType = "comment";
    else
      item.targetType = "loop";
  }

  // Use actor ID as name fallback
  if (item.actorName.isEmpty())
    item.actorName = item.actorId;

  return item;
}

// Phase 2 features:
// - Notification sound option (user preference)
// - Notification preferences (mute specific types)
// - WebSocket push for real-time updates (polling is sufficient for MVP)

juce::String NotificationItem::getDisplayText() const {
  juce::String text;

  // Build actor text using group.actorCount
  if (group.actorCount > 1) {
    text = actorName + " and " + juce::String(group.actorCount - 1) + " other";
    if (group.actorCount > 2)
      text += "s";
  } else {
    text = actorName;
  }

  // Add verb description using group.verb
  if (group.verb == "like") {
    text += " liked your loop";
  } else if (group.verb == "follow") {
    text += " started following you";
  } else if (group.verb == "comment") {
    text += " commented on your loop";
    if (targetPreview.isNotEmpty())
      text += ": \"" + targetPreview.substring(0, 50) + "\"";
  } else if (group.verb == "mention") {
    text += " mentioned you";
    if (targetPreview.isNotEmpty())
      text += ": \"" + targetPreview.substring(0, 50) + "\"";
  } else if (group.verb == "repost") {
    text += " reposted your loop";
  } else {
    text += " " + group.verb;
  }

  return text;
}

juce::String NotificationItem::getRelativeTime() const {
  // Use group.updatedAt
  return TimeUtils::formatTimeAgoShort(group.updatedAt);
}

juce::String NotificationItem::getVerbIcon() const {
  // Return emoji or icon identifier using group.verb
  if (group.verb == "like")
    return "heart";
  else if (group.verb == "follow")
    return "person";
  else if (group.verb == "comment")
    return "comment";
  else if (group.verb == "mention")
    return "at";
  else if (group.verb == "repost")
    return "repost";
  return "bell";
}

// ==============================================================================
// NotificationRow implementation
// ==============================================================================

NotificationRow::NotificationRow() {
  setSize(NotificationList::PREFERRED_WIDTH, ROW_HEIGHT);

  // Set up hover state - triggers visual feedback on hover
  hoverState.onHoverChanged = [this](bool hovered) {
    // Repaint to show/hide row highlight effects
    if (hovered) {
      // Show hover effects (row highlight, action buttons)
      repaint();
    } else {
      // Hide hover effects when mouse leaves
      repaint();
    }
  };
}

void NotificationRow::setNotification(const NotificationItem &notif) {
  notification = notif;
  repaint();
}

void NotificationRow::paint(juce::Graphics &g) {
  auto bounds = getLocalBounds();

  // Background
  if (hoverState.isHovered())
    g.fillAll(SidechainColors::backgroundLighter());
  else if (!notification.isRead)
    g.fillAll(SidechainColors::backgroundLight()); // Slightly lighter for unread
  else
    g.fillAll(SidechainColors::background());

  // Unread indicator (blue dot on left)
  if (!notification.isRead) {
    auto indicatorBounds = bounds.removeFromLeft(8).reduced(0, (ROW_HEIGHT - 8) / 2);
    drawUnreadIndicator(g, indicatorBounds);
  } else {
    bounds.removeFromLeft(8);
  }

  int padding = 12;
  bounds = bounds.reduced(padding, 8);

  // Avatar area
  auto avatarBounds = bounds.removeFromLeft(44);
  drawAvatar(g, avatarBounds);

  bounds.removeFromLeft(12); // Gap

  // Text area
  drawText(g, bounds);
}

void NotificationRow::drawAvatar(juce::Graphics &g, juce::Rectangle<int> bounds) {
  // Avatar background circle
  auto avatarCircle = bounds.withSizeKeepingCentre(40, 40).toFloat();

  // Generate color from actor name
  auto hash = static_cast<unsigned int>(notification.actorName.hashCode());
  float hue = (hash % 360) / 360.0f;
  g.setColour(juce::Colour::fromHSV(hue, 0.6f, 0.7f, 1.0f));
  g.fillEllipse(avatarCircle);

  // Draw initials
  juce::String initials = StringUtils::getInitials(notification.actorName);

  g.setColour(SidechainColors::textPrimary());
  g.setFont(juce::Font(juce::FontOptions().withHeight(14.0f).withStyle("Bold")));
  g.drawText(initials, avatarCircle.toNearestInt(), juce::Justification::centred, false);

  // Draw verb icon overlay (bottom right of avatar)
  auto iconBounds = juce::Rectangle<int>(static_cast<int>(avatarCircle.getRight() - 14),
                                         static_cast<int>(avatarCircle.getBottom() - 14), 16, 16);
  drawVerbIcon(g, iconBounds);
}

void NotificationRow::drawVerbIcon(juce::Graphics &g, juce::Rectangle<int> bounds) {
  // Icon background
  juce::Colour iconColor;
  if (notification.group.verb == "like")
    iconColor = SidechainColors::like();
  else if (notification.group.verb == "follow")
    iconColor = SidechainColors::follow();
  else if (notification.group.verb == "comment")
    iconColor = SidechainColors::comment();
  else
    iconColor = SidechainColors::textMuted();

  g.setColour(iconColor);
  g.fillEllipse(bounds.toFloat());

  // Draw simple icon shape
  g.setColour(SidechainColors::textPrimary());
  auto iconInner = bounds.reduced(3).toFloat();

  if (notification.group.verb == "like") {
    // Heart shape (simplified)
    juce::Path heart;
    float cx = iconInner.getCentreX();
    float cy = iconInner.getCentreY();
    float size = iconInner.getWidth() * 0.35f;

    heart.addEllipse(cx - size, cy - size * 0.3f, size, size);
    heart.addEllipse(cx, cy - size * 0.3f, size, size);

    juce::Path triangle;
    triangle.startNewSubPath(cx - size, cy + size * 0.1f);
    triangle.lineTo(cx + size, cy + size * 0.1f);
    triangle.lineTo(cx, cy + size * 1.2f);
    triangle.closeSubPath();

    g.fillPath(heart);
    g.fillPath(triangle);
  } else if (notification.group.verb == "follow") {
    // Person shape
    float cx = iconInner.getCentreX();
    float cy = iconInner.getCentreY();
    g.fillEllipse(cx - 2.5f, cy - 4.0f, 5.0f, 5.0f); // Head
    g.fillEllipse(cx - 4.0f, cy + 1.0f, 8.0f, 5.0f); // Body
  } else if (notification.group.verb == "comment") {
    // Speech bubble
    g.fillRoundedRectangle(iconInner.reduced(1.0f), 2.0f);
  }
}

void NotificationRow::drawText(juce::Graphics &g, juce::Rectangle<int> bounds) {
  // Main text
  g.setColour(notification.isRead ? SidechainColors::textSecondary() : SidechainColors::textPrimary());
  g.setFont(juce::Font(juce::FontOptions().withHeight(13.0f).withStyle(notification.isRead ? "Regular" : "Bold")));

  auto textBounds = bounds.removeFromTop(bounds.getHeight() - 16);
  g.drawFittedText(notification.getDisplayText(), textBounds, juce::Justification::centredLeft, 2, 1.0f);

  // Timestamp (bottom)
  g.setColour(SidechainColors::textMuted());
  g.setFont(juce::Font(juce::FontOptions().withHeight(11.0f)));
  g.drawText(notification.getRelativeTime(), bounds, juce::Justification::centredLeft, false);
}

void NotificationRow::drawUnreadIndicator(juce::Graphics &g, juce::Rectangle<int> bounds) {
  g.setColour(SidechainColors::link());
  g.fillEllipse(bounds.toFloat().withSizeKeepingCentre(6.0f, 6.0f));
}

void NotificationRow::mouseEnter(const juce::MouseEvent &) {
  hoverState.setHovered(true);
}

void NotificationRow::mouseExit(const juce::MouseEvent &) {
  hoverState.setHovered(false);
}

void NotificationRow::mouseDown(const juce::MouseEvent &) {
  if (onClicked)
    onClicked(notification);
}

// ==============================================================================
// NotificationList implementation
// ==============================================================================

NotificationList::NotificationList(Sidechain::Stores::AppStore *store)
    : Sidechain::UI::AppStoreComponent<Sidechain::Stores::NotificationState>(store) {
  Log::info("NotificationList: Initializing");
  addAndMakeVisible(viewport);
  viewport.setViewedComponent(&contentComponent, false);
  viewport.setScrollBarsShown(true, false);
  viewport.getVerticalScrollBar().addListener(this);

  setSize(PREFERRED_WIDTH, MAX_HEIGHT);

  // Subscribe to AppStore after UI setup
  subscribeToAppStore();
}

NotificationList::~NotificationList() {
  Log::debug("NotificationList: Destroying");
  viewport.getVerticalScrollBar().removeListener(this);
}

// ==============================================================================
// AppStoreComponent implementation

void NotificationList::onAppStateChanged(const Sidechain::Stores::NotificationState &state) {
  // Convert std::vector<shared_ptr<Notification>> to juce::Array<NotificationItem>
  juce::Array<NotificationItem> notificationItems;
  for (const auto &notif : state.notifications) {
    if (notif) {
      // Create NotificationItem from Notification by converting to JSON first
      try {
        nlohmann::json j;
        to_json(j, *notif);
        auto jsonStr = j.dump();
        auto jVar = juce::JSON::parse(jsonStr);
        notificationItems.add(NotificationItem::fromJson(jVar));
      } catch (...) {
        // Skip invalid items
      }
    }
  }

  setNotifications(notificationItems);
  setUnseenCount(state.unseenCount);
  setUnreadCount(state.unreadCount);
  setLoading(state.isLoading);
  if (state.notificationError.isNotEmpty()) {
    setError(state.notificationError);
  }
}

void NotificationList::subscribeToAppStore() {
  if (!appStore)
    return;

  juce::Component::SafePointer<NotificationList> safeThis(this);
  storeUnsubscriber = appStore->subscribeToNotifications([safeThis](const Sidechain::Stores::NotificationState &state) {
    if (!safeThis)
      return;
    juce::MessageManager::callAsync([safeThis, state]() {
      if (safeThis)
        safeThis->onAppStateChanged(state);
    });
  });
}

// ==============================================================================
void NotificationList::setNotifications(const juce::Array<NotificationItem> &newNotifications) {
  notifications = newNotifications;
  isLoading = false;
  errorMessage.clear();
  Log::info("NotificationList: Set " + juce::String(notifications.size()) + " notifications");
  rebuildRowComponents();
  repaint();
}

void NotificationList::clearNotifications() {
  notifications.clear();
  rowComponents.clear();
  repaint();
}

void NotificationList::setLoading(bool loading) {
  isLoading = loading;
  if (loading)
    errorMessage.clear();
  repaint();
}

void NotificationList::setError(const juce::String &error) {
  errorMessage = error;
  isLoading = false;
  Log::error("NotificationList: Error - " + error);
  repaint();
}

void NotificationList::setUnseenCount(int count) {
  unseenCount = count;
  repaint();
}

void NotificationList::setUnreadCount(int count) {
  unreadCount = count;
  repaint();
}

void NotificationList::paint(juce::Graphics &g) {
  auto bounds = getLocalBounds();

  // Background
  g.fillAll(SidechainColors::background());

  // Header
  auto headerBounds = bounds.removeFromTop(HEADER_HEIGHT);
  drawHeader(g, headerBounds);

  // Separator line
  UIHelpers::drawDivider(g, bounds.getX(), bounds.getY(), bounds.getWidth(), SidechainColors::border());
  bounds.removeFromTop(1);

  // Content area - viewport handles the scrolling
  if (isLoading) {
    drawLoadingState(g, bounds);
  } else if (errorMessage.isNotEmpty()) {
    drawErrorState(g, bounds);
  } else if (notifications.isEmpty()) {
    drawEmptyState(g, bounds);
  }
}

void NotificationList::drawHeader(juce::Graphics &g, juce::Rectangle<int> bounds) {
  // Header background
  UIHelpers::drawCard(g, bounds, SidechainColors::backgroundLight());

  bounds = bounds.reduced(16, 0);

  // Title
  g.setColour(SidechainColors::textPrimary());
  g.setFont(juce::Font(juce::FontOptions().withHeight(16.0f).withStyle("Bold")));
  g.drawText("Notifications", bounds, juce::Justification::centredLeft, false);

  // "Mark all as read" button (right side)
  if (unreadCount > 0) {
    auto markAllBounds = getMarkAllReadButtonBounds();
    bool markAllHovered = markAllBounds.contains(getMouseXYRelative());

    g.setColour(markAllHovered ? SidechainColors::link() : SidechainColors::link().withAlpha(0.7f));
    g.setFont(juce::Font(juce::FontOptions().withHeight(12.0f)));
    g.drawText("Mark all read", markAllBounds, juce::Justification::centredRight, false);
  }

  // Close button (X) - far right
  auto closeBounds = getCloseButtonBounds();
  bool closeHovered = closeBounds.contains(getMouseXYRelative());

  g.setColour(closeHovered ? SidechainColors::textPrimary() : SidechainColors::textSecondary());
  g.setFont(juce::Font(juce::FontOptions().withHeight(18.0f).withStyle("Bold")));
  g.drawText(juce::CharPointer_UTF8("\xc3\x97"), closeBounds, juce::Justification::centred, false); // ×
}

void NotificationList::drawEmptyState(juce::Graphics &g, juce::Rectangle<int> bounds) {
  g.setColour(SidechainColors::textMuted());
  g.setFont(juce::Font(juce::FontOptions().withHeight(14.0f)));
  g.drawText("No notifications yet", bounds, juce::Justification::centred, false);
}

void NotificationList::drawLoadingState(juce::Graphics &g, juce::Rectangle<int> bounds) {
  g.setColour(SidechainColors::textMuted());
  g.setFont(juce::Font(juce::FontOptions().withHeight(14.0f)));
  g.drawText("Loading notifications...", bounds, juce::Justification::centred, false);
}

void NotificationList::drawErrorState(juce::Graphics &g, juce::Rectangle<int> bounds) {
  g.setColour(SidechainColors::error());
  g.setFont(juce::Font(juce::FontOptions().withHeight(14.0f)));
  g.drawText(errorMessage, bounds, juce::Justification::centred, true);
}

void NotificationList::resized() {
  auto bounds = getLocalBounds();
  bounds.removeFromTop(HEADER_HEIGHT + 1); // Header + separator

  viewport.setBounds(bounds);
  layoutRows();
}

void NotificationList::rebuildRowComponents() {
  rowComponents.clear();

  for (const auto &notification : notifications) {
    auto *row = rowComponents.add(new NotificationRow());
    row->setNotification(notification);
    row->onClicked = [this](const NotificationItem &item) {
      if (onNotificationClicked)
        onNotificationClicked(item);
    };
    contentComponent.addAndMakeVisible(row);
  }

  layoutRows();
}

void NotificationList::layoutRows() {
  int totalHeight = notifications.size() * NotificationRow::ROW_HEIGHT;
  contentComponent.setSize(viewport.getWidth() - viewport.getScrollBarThickness(), totalHeight);

  int y = 0;
  for (auto *row : rowComponents) {
    row->setBounds(0, y, contentComponent.getWidth(), NotificationRow::ROW_HEIGHT);
    y += NotificationRow::ROW_HEIGHT;
  }
}

void NotificationList::scrollBarMoved(juce::ScrollBar *, double newRangeStart) {
  scrollOffset = static_cast<int>(newRangeStart);
}

juce::Rectangle<int> NotificationList::getMarkAllReadButtonBounds() const {
  return juce::Rectangle<int>(getWidth() - 130, 0, 90, HEADER_HEIGHT);
}

juce::Rectangle<int> NotificationList::getCloseButtonBounds() const {
  return juce::Rectangle<int>(getWidth() - 40, 0, 32, HEADER_HEIGHT);
}

void NotificationList::mouseDown(const juce::MouseEvent &event) {
  auto pos = event.getPosition();

  if (getCloseButtonBounds().contains(pos)) {
    if (onCloseClicked)
      onCloseClicked();
  } else if (getMarkAllReadButtonBounds().contains(pos) && unreadCount > 0) {
    // Mark all notifications as read via AppStore
    if (appStore) {
      appStore->markNotificationsAsRead();
    } else if (onMarkAllReadClicked) {
      // Fallback to legacy callback if no store
      onMarkAllReadClicked();
    }
  }
}
