#pragma once

#include "../../models/AggregatedFeedGroup.h"
#include "../../models/Notification.h"
#include <JuceHeader.h>

// ==============================================================================
/**
 * NotificationItem represents a single notification group from getstream.io
 *
 * Now uses AggregatedFeedGroup as the underlying model for consistency.
 * getstream.io groups notifications by aggregation format: {{ verb }}_{{
 * time.strftime("%Y-%m-%d") }}
 *
 * Examples:
 * - "Alice and 3 others liked your loop" (grouped by verb+target+day)
 * - "Bob started following you" (single follow notification)
 */
struct NotificationItem {
  Sidechain::AggregatedFeedGroup group; // The underlying aggregated group
  bool isRead = false;
  bool isSeen = false;

  // Derived from first activity in group
  juce::String actorId;
  juce::String actorName;
  juce::String actorAvatarUrl;
  juce::String targetId;      // e.g., "loop:123" or "user:456"
  juce::String targetType;    // "loop", "user", "comment"
  juce::String targetPreview; // Preview text or title

  // Parse from JSON response (old format for backward compatibility)
  static NotificationItem fromJson(const juce::var &json);

  // Create from typed Notification model
  static NotificationItem fromNotification(const Sidechain::Notification &notif);

  // Create from AggregatedFeedGroup
  static NotificationItem fromAggregatedGroup(const Sidechain::AggregatedFeedGroup &group, bool read = false,
                                              bool seen = false);

  // Generate display text like "Alice and 3 others liked your loop"
  juce::String getDisplayText() const;

  // Get relative time like "2h ago"
  juce::String getRelativeTime() const;

  // Get icon path/type for the verb
  juce::String getVerbIcon() const;
};
