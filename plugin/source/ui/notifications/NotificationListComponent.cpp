#include "NotificationListComponent.h"
#include "../../util/Time.h"
#include "../../util/Colors.h"
#include "../../util/Json.h"
#include "../../util/UIHelpers.h"
#include "../../util/HoverState.h"
#include "../../util/Log.h"

//==============================================================================
// NotificationItem implementation
//==============================================================================

NotificationItem NotificationItem::fromJson(const juce::var& json)
{
    NotificationItem item;

    item.id = Json::getString(json, "id");
    item.groupKey = Json::getString(json, "group");
    item.verb = Json::getString(json, "verb");
    item.activityCount = Json::getInt(json, "activity_count", 1);
    item.actorCount = Json::getInt(json, "actor_count", 1);
    item.isRead = Json::getBool(json, "is_read");
    item.isSeen = Json::getBool(json, "is_seen");
    item.createdAt = Json::getString(json, "created_at");
    item.updatedAt = Json::getString(json, "updated_at");

    // Parse activities array to get actor info
    auto activities = Json::getArray(json, "activities");
    if (Json::isArray(activities) && Json::arraySize(activities) > 0)
    {
        auto firstActivity = Json::getObjectAt(activities, 0);
        juce::String actor = Json::getString(firstActivity, "actor");

        // Actor format is "user:username" or just the ID
        if (actor.startsWith("user:"))
            item.actorId = actor.substring(5);
        else
            item.actorId = actor;

        // Get actor name from extra data if available
        auto extra = Json::getObject(firstActivity, "extra");
        if (Json::isObject(extra))
        {
            item.actorName = Json::getString(extra, "actor_name", item.actorId);
            item.targetId = Json::getString(extra, "loop_id");
            item.targetPreview = Json::getString(extra, "preview");

            if (item.targetId.isEmpty())
                item.targetId = Json::getString(extra, "target_id");
        }

        // Parse object (target)
        juce::String object = Json::getString(firstActivity, "object");
        if (object.startsWith("loop:"))
        {
            item.targetType = "loop";
            if (item.targetId.isEmpty())
                item.targetId = object.substring(5);
        }
        else if (object.startsWith("user:"))
        {
            item.targetType = "user";
            if (item.targetId.isEmpty())
                item.targetId = object.substring(5);
        }
        else if (object.startsWith("comment:"))
        {
            item.targetType = "comment";
            if (item.targetId.isEmpty())
                item.targetId = object.substring(8);
        }
    }

    // Use actor ID as name fallback
    if (item.actorName.isEmpty())
        item.actorName = item.actorId;

    return item;
}

juce::String NotificationItem::getDisplayText() const
{
    juce::String text;

    // Build actor text
    if (actorCount > 1)
    {
        text = actorName + " and " + juce::String(actorCount - 1) + " other";
        if (actorCount > 2)
            text += "s";
    }
    else
    {
        text = actorName;
    }

    // Add verb description
    if (verb == "like")
    {
        text += " liked your loop";
    }
    else if (verb == "follow")
    {
        text += " started following you";
    }
    else if (verb == "comment")
    {
        text += " commented on your loop";
        if (targetPreview.isNotEmpty())
            text += ": \"" + targetPreview.substring(0, 50) + "\"";
    }
    else if (verb == "mention")
    {
        text += " mentioned you";
        if (targetPreview.isNotEmpty())
            text += ": \"" + targetPreview.substring(0, 50) + "\"";
    }
    else if (verb == "repost")
    {
        text += " reposted your loop";
    }
    else
    {
        text += " " + verb;
    }

    return text;
}

juce::String NotificationItem::getRelativeTime() const
{
    // Parse ISO timestamp and calculate relative time
    juce::String timeStr = updatedAt.isNotEmpty() ? updatedAt : createdAt;

    if (timeStr.isEmpty())
        return "";

    // Parse ISO 8601 timestamp
    juce::Time notifTime = juce::Time::fromISO8601(timeStr);
    return TimeUtils::formatTimeAgoShort(notifTime);
}

juce::String NotificationItem::getVerbIcon() const
{
    // Return emoji or icon identifier
    if (verb == "like")
        return "heart";
    else if (verb == "follow")
        return "person";
    else if (verb == "comment")
        return "comment";
    else if (verb == "mention")
        return "at";
    else if (verb == "repost")
        return "repost";
    return "bell";
}

//==============================================================================
// NotificationRowComponent implementation
//==============================================================================

NotificationRowComponent::NotificationRowComponent()
{
    setSize(NotificationListComponent::PREFERRED_WIDTH, ROW_HEIGHT);
    
    // Set up hover state
    hoverState.onHoverChanged = [this](bool hovered) {
        repaint();
    };
}

void NotificationRowComponent::setNotification(const NotificationItem& notif)
{
    notification = notif;
    repaint();
}

void NotificationRowComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    // Background
    if (hoverState.isHovered())
        g.fillAll(SidechainColors::backgroundLighter());
    else if (!notification.isRead)
        g.fillAll(SidechainColors::backgroundLight()); // Slightly lighter for unread
    else
        g.fillAll(SidechainColors::background());

    // Unread indicator (blue dot on left)
    if (!notification.isRead)
    {
        auto indicatorBounds = bounds.removeFromLeft(8).reduced(0, (ROW_HEIGHT - 8) / 2);
        drawUnreadIndicator(g, indicatorBounds);
    }
    else
    {
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

void NotificationRowComponent::drawAvatar(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    // Avatar background circle
    auto avatarCircle = bounds.withSizeKeepingCentre(40, 40).toFloat();

    // Generate color from actor name
    auto hash = static_cast<unsigned int>(notification.actorName.hashCode());
    float hue = (hash % 360) / 360.0f;
    g.setColour(juce::Colour::fromHSV(hue, 0.6f, 0.7f, 1.0f));
    g.fillEllipse(avatarCircle);

    // Draw initials
    juce::String initials;
    if (notification.actorName.isNotEmpty())
    {
        initials = notification.actorName.substring(0, 1).toUpperCase();
        // Try to get second initial from word boundary
        int spaceIdx = notification.actorName.indexOf(" ");
        if (spaceIdx > 0 && spaceIdx < notification.actorName.length() - 1)
            initials += notification.actorName.substring(spaceIdx + 1, spaceIdx + 2).toUpperCase();
    }

    g.setColour(SidechainColors::textPrimary());
    g.setFont(juce::Font(14.0f, juce::Font::bold));
    g.drawText(initials, avatarCircle.toNearestInt(), juce::Justification::centred, false);

    // Draw verb icon overlay (bottom right of avatar)
    auto iconBounds = juce::Rectangle<int>(
        static_cast<int>(avatarCircle.getRight() - 14),
        static_cast<int>(avatarCircle.getBottom() - 14),
        16, 16
    );
    drawVerbIcon(g, iconBounds);
}

void NotificationRowComponent::drawVerbIcon(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    // Icon background
    juce::Colour iconColor;
    if (notification.verb == "like")
        iconColor = SidechainColors::like();
    else if (notification.verb == "follow")
        iconColor = SidechainColors::follow();
    else if (notification.verb == "comment")
        iconColor = SidechainColors::comment();
    else
        iconColor = SidechainColors::textMuted();

    g.setColour(iconColor);
    g.fillEllipse(bounds.toFloat());

    // Draw simple icon shape
    g.setColour(SidechainColors::textPrimary());
    auto iconInner = bounds.reduced(3).toFloat();

    if (notification.verb == "like")
    {
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
    }
    else if (notification.verb == "follow")
    {
        // Person shape
        float cx = iconInner.getCentreX();
        float cy = iconInner.getCentreY();
        g.fillEllipse(cx - 2.5f, cy - 4.0f, 5.0f, 5.0f); // Head
        g.fillEllipse(cx - 4.0f, cy + 1.0f, 8.0f, 5.0f); // Body
    }
    else if (notification.verb == "comment")
    {
        // Speech bubble
        g.fillRoundedRectangle(iconInner.reduced(1.0f), 2.0f);
    }
}

void NotificationRowComponent::drawText(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    // Main text
    g.setColour(notification.isRead ? SidechainColors::textSecondary() : SidechainColors::textPrimary());
    g.setFont(juce::Font(13.0f, notification.isRead ? juce::Font::plain : juce::Font::bold));

    auto textBounds = bounds.removeFromTop(bounds.getHeight() - 16);
    g.drawFittedText(notification.getDisplayText(), textBounds,
                     juce::Justification::centredLeft, 2, 1.0f);

    // Timestamp (bottom)
    g.setColour(SidechainColors::textMuted());
    g.setFont(juce::Font(11.0f));
    g.drawText(notification.getRelativeTime(), bounds,
               juce::Justification::centredLeft, false);
}

void NotificationRowComponent::drawUnreadIndicator(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    g.setColour(SidechainColors::link());
    g.fillEllipse(bounds.toFloat().withSizeKeepingCentre(6.0f, 6.0f));
}

void NotificationRowComponent::mouseEnter(const juce::MouseEvent&)
{
    hoverState.setHovered(true);
}

void NotificationRowComponent::mouseExit(const juce::MouseEvent&)
{
    hoverState.setHovered(false);
}

void NotificationRowComponent::mouseDown(const juce::MouseEvent&)
{
    if (onClicked)
        onClicked(notification);
}

//==============================================================================
// NotificationListComponent implementation
//==============================================================================

NotificationListComponent::NotificationListComponent()
{
    Log::info("NotificationListComponent: Initializing");
    addAndMakeVisible(viewport);
    viewport.setViewedComponent(&contentComponent, false);
    viewport.setScrollBarsShown(true, false);
    viewport.getVerticalScrollBar().addListener(this);

    setSize(PREFERRED_WIDTH, MAX_HEIGHT);
}

NotificationListComponent::~NotificationListComponent()
{
    Log::debug("NotificationListComponent: Destroying");
    viewport.getVerticalScrollBar().removeListener(this);
}

void NotificationListComponent::setNotifications(const juce::Array<NotificationItem>& newNotifications)
{
    notifications = newNotifications;
    isLoading = false;
    errorMessage.clear();
    Log::info("NotificationListComponent: Set " + juce::String(notifications.size()) + " notifications");
    rebuildRowComponents();
    repaint();
}

void NotificationListComponent::clearNotifications()
{
    notifications.clear();
    rowComponents.clear();
    repaint();
}

void NotificationListComponent::setLoading(bool loading)
{
    isLoading = loading;
    if (loading)
        errorMessage.clear();
    repaint();
}

void NotificationListComponent::setError(const juce::String& error)
{
    errorMessage = error;
    isLoading = false;
    Log::error("NotificationListComponent: Error - " + error);
    repaint();
}

void NotificationListComponent::setUnseenCount(int count)
{
    unseenCount = count;
    repaint();
}

void NotificationListComponent::setUnreadCount(int count)
{
    unreadCount = count;
    repaint();
}

void NotificationListComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    // Background
    g.fillAll(SidechainColors::background());

    // Header
    auto headerBounds = bounds.removeFromTop(HEADER_HEIGHT);
    drawHeader(g, headerBounds);

    // Separator line
    UI::drawDivider(g, bounds.getX(), bounds.getY(), bounds.getWidth(), SidechainColors::border());
    bounds.removeFromTop(1);

    // Content area - viewport handles the scrolling
    if (isLoading)
    {
        drawLoadingState(g, bounds);
    }
    else if (errorMessage.isNotEmpty())
    {
        drawErrorState(g, bounds);
    }
    else if (notifications.isEmpty())
    {
        drawEmptyState(g, bounds);
    }
}

void NotificationListComponent::drawHeader(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    // Header background
    UI::drawCard(g, bounds, SidechainColors::backgroundLight());

    bounds = bounds.reduced(16, 0);

    // Title
    g.setColour(SidechainColors::textPrimary());
    g.setFont(juce::Font(16.0f, juce::Font::bold));
    g.drawText("Notifications", bounds, juce::Justification::centredLeft, false);

    // "Mark all as read" button (right side)
    if (unreadCount > 0)
    {
        auto markAllBounds = getMarkAllReadButtonBounds();
        bool markAllHovered = markAllBounds.contains(getMouseXYRelative());

        g.setColour(markAllHovered ? SidechainColors::link() : SidechainColors::link().withAlpha(0.7f));
        g.setFont(juce::Font(12.0f));
        g.drawText("Mark all read", markAllBounds, juce::Justification::centredRight, false);
    }

    // Close button (X) - far right
    auto closeBounds = getCloseButtonBounds();
    bool closeHovered = closeBounds.contains(getMouseXYRelative());

    g.setColour(closeHovered ? SidechainColors::textPrimary() : SidechainColors::textSecondary());
    g.setFont(juce::Font(18.0f, juce::Font::bold));
    g.drawText(juce::CharPointer_UTF8("\xc3\x97"), closeBounds, juce::Justification::centred, false); // ×
}

void NotificationListComponent::drawEmptyState(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    g.setColour(SidechainColors::textMuted());
    g.setFont(juce::Font(14.0f));
    g.drawText("No notifications yet", bounds, juce::Justification::centred, false);
}

void NotificationListComponent::drawLoadingState(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    g.setColour(SidechainColors::textMuted());
    g.setFont(juce::Font(14.0f));
    g.drawText("Loading notifications...", bounds, juce::Justification::centred, false);
}

void NotificationListComponent::drawErrorState(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    g.setColour(SidechainColors::error());
    g.setFont(juce::Font(14.0f));
    g.drawText(errorMessage, bounds, juce::Justification::centred, true);
}

void NotificationListComponent::resized()
{
    auto bounds = getLocalBounds();
    bounds.removeFromTop(HEADER_HEIGHT + 1); // Header + separator

    viewport.setBounds(bounds);
    layoutRows();
}

void NotificationListComponent::rebuildRowComponents()
{
    rowComponents.clear();

    for (const auto& notification : notifications)
    {
        auto* row = rowComponents.add(new NotificationRowComponent());
        row->setNotification(notification);
        row->onClicked = [this](const NotificationItem& item) {
            if (onNotificationClicked)
                onNotificationClicked(item);
        };
        contentComponent.addAndMakeVisible(row);
    }

    layoutRows();
}

void NotificationListComponent::layoutRows()
{
    int totalHeight = notifications.size() * NotificationRowComponent::ROW_HEIGHT;
    contentComponent.setSize(viewport.getWidth() - viewport.getScrollBarThickness(), totalHeight);

    int y = 0;
    for (auto* row : rowComponents)
    {
        row->setBounds(0, y, contentComponent.getWidth(), NotificationRowComponent::ROW_HEIGHT);
        y += NotificationRowComponent::ROW_HEIGHT;
    }
}

void NotificationListComponent::scrollBarMoved(juce::ScrollBar*, double newRangeStart)
{
    scrollOffset = static_cast<int>(newRangeStart);
}

juce::Rectangle<int> NotificationListComponent::getMarkAllReadButtonBounds() const
{
    return juce::Rectangle<int>(getWidth() - 130, 0, 90, HEADER_HEIGHT);
}

juce::Rectangle<int> NotificationListComponent::getCloseButtonBounds() const
{
    return juce::Rectangle<int>(getWidth() - 40, 0, 32, HEADER_HEIGHT);
}

void NotificationListComponent::mouseDown(const juce::MouseEvent& event)
{
    auto pos = event.getPosition();

    if (getCloseButtonBounds().contains(pos))
    {
        if (onCloseClicked)
            onCloseClicked();
    }
    else if (getMarkAllReadButtonBounds().contains(pos) && unreadCount > 0)
    {
        if (onMarkAllReadClicked)
            onMarkAllReadClicked();
    }
}
