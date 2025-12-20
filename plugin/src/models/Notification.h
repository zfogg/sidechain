#pragma once

#include "../util/json/JsonValidation.h"
#include <JuceHeader.h>
#include <nlohmann/json.hpp>
#include <vector>

namespace Sidechain {

// ==============================================================================
/**
 * Notification - User notification entity
 *
 * Represents a notification about user activity (likes, follows, comments, etc.)
 * Can be aggregated (e.g., "Alice and 3 others liked your post")
 */
struct Notification {
  // ==============================================================================
  // Core identity
  juce::String id;
  juce::String verb; // "like", "follow", "comment", "repost", "mention"

  // ==============================================================================
  // Actors (users who performed the action)
  std::vector<std::string> actorIds;
  std::vector<std::string> actorUsernames;
  std::vector<std::string> actorAvatarUrls;

  // ==============================================================================
  // Target (what was acted upon)
  juce::String targetType;    // "post", "user", "comment", "story"
  juce::String targetId;      // ID of the target entity
  juce::String targetPreview; // Preview text/title of target

  // ==============================================================================
  // Content (for comments/mentions)
  juce::String contentText; // Comment text or mention context

  // ==============================================================================
  // Status
  bool isRead = false;
  bool isSeen = false; // Seen in notification list but not clicked

  // ==============================================================================
  // Aggregation
  int actorCount = 0;   // Total number of actors (may be > actorIds.size())
  juce::String groupId; // ID for grouping related notifications

  // ==============================================================================
  // Timestamps
  juce::Time createdAt;
  juce::Time updatedAt; // Last time this notification was updated (new actors)

  // ==============================================================================
  // Typed JSON serialization
  SIDECHAIN_JSON_TYPE(Notification)

  // ==============================================================================
  // Validation
  bool isValid() const {
    return id.isNotEmpty() && verb.isNotEmpty() && !actorIds.empty();
  }

  juce::String getId() const {
    return id;
  }

  // ==============================================================================
  // Display helpers
  juce::String getPrimaryActorId() const {
    return actorIds.empty() ? juce::String() : Json::toJuceString(actorIds[0]);
  }

  juce::String getPrimaryActorUsername() const {
    return actorUsernames.empty() ? juce::String() : Json::toJuceString(actorUsernames[0]);
  }

  juce::String getPrimaryActorAvatar() const {
    return actorAvatarUrls.empty() ? juce::String() : Json::toJuceString(actorAvatarUrls[0]);
  }

  bool isAggregated() const {
    return actorCount > 1 || actorIds.size() > 1;
  }

  int getAdditionalActorCount() const {
    return std::max(0, actorCount - 1);
  }

  juce::String getDisplayText() const {
    auto primaryActor = getPrimaryActorUsername();
    if (primaryActor.isEmpty())
      primaryActor = "Someone";

    juce::String text;

    // Build actor part
    if (isAggregated()) {
      auto count = getAdditionalActorCount();
      if (count == 1)
        text = primaryActor + " and 1 other";
      else
        text = primaryActor + " and " + juce::String(count) + " others";
    } else {
      text = primaryActor;
    }

    // Build verb part
    if (verb == "like")
      text += " liked your ";
    else if (verb == "follow")
      return text + " started following you";
    else if (verb == "comment")
      text += " commented on your ";
    else if (verb == "repost")
      text += " reposted your ";
    else if (verb == "mention")
      return text + " mentioned you";
    else
      text += " interacted with your ";

    // Add target type
    text += targetType;

    return text;
  }

  juce::String getVerbIcon() const {
    if (verb == "like")
      return "❤️";
    if (verb == "follow")
      return "👤";
    if (verb == "comment")
      return "💬";
    if (verb == "repost")
      return "🔄";
    if (verb == "mention")
      return "@";
    return "🔔";
  }
};

// ==============================================================================
// JSON Serialization Implementation

inline void to_json(nlohmann::json &j, const Notification &notif) {
  j = nlohmann::json{
      {"id", Json::fromJuceString(notif.id)},
      {"verb", Json::fromJuceString(notif.verb)},
      {"actor_ids", notif.actorIds},
      {"actor_usernames", notif.actorUsernames},
      {"actor_avatar_urls", notif.actorAvatarUrls},
      {"target_type", Json::fromJuceString(notif.targetType)},
      {"target_id", Json::fromJuceString(notif.targetId)},
      {"target_preview", Json::fromJuceString(notif.targetPreview)},
      {"content_text", Json::fromJuceString(notif.contentText)},
      {"is_read", notif.isRead},
      {"is_seen", notif.isSeen},
      {"actor_count", notif.actorCount},
      {"group_id", Json::fromJuceString(notif.groupId)},
      {"created_at", notif.createdAt.toISO8601(true).toStdString()},
      {"updated_at", notif.updatedAt.toISO8601(true).toStdString()},
  };
}

inline void from_json(const nlohmann::json &j, Notification &notif) {
  // Required fields
  JSON_REQUIRE_STRING(j, "id", notif.id);
  JSON_REQUIRE_STRING(j, "verb", notif.verb);

  // Optional fields
  JSON_OPTIONAL_STRING(j, "target_type", notif.targetType, "");
  JSON_OPTIONAL_STRING(j, "target_id", notif.targetId, "");
  JSON_OPTIONAL_STRING(j, "target_preview", notif.targetPreview, "");
  JSON_OPTIONAL_STRING(j, "content_text", notif.contentText, "");
  JSON_OPTIONAL_STRING(j, "group_id", notif.groupId, "");

  JSON_OPTIONAL(j, "is_read", notif.isRead, false);
  JSON_OPTIONAL(j, "is_seen", notif.isSeen, false);
  JSON_OPTIONAL(j, "actor_count", notif.actorCount, 0);

  // Parse actor arrays
  if (j.contains("actor_ids") && j["actor_ids"].is_array()) {
    notif.actorIds = j["actor_ids"].get<std::vector<std::string>>();
  }
  if (j.contains("actor_usernames") && j["actor_usernames"].is_array()) {
    notif.actorUsernames = j["actor_usernames"].get<std::vector<std::string>>();
  }
  if (j.contains("actor_avatar_urls") && j["actor_avatar_urls"].is_array()) {
    notif.actorAvatarUrls = j["actor_avatar_urls"].get<std::vector<std::string>>();
  }

  // If actor_count not explicitly set, use actor_ids size
  if (notif.actorCount == 0 && !notif.actorIds.empty()) {
    notif.actorCount = static_cast<int>(notif.actorIds.size());
  }

  // Parse timestamps
  if (j.contains("created_at") && !j["created_at"].is_null()) {
    notif.createdAt = juce::Time::fromISO8601(Json::toJuceString(j["created_at"].get<std::string>()));
  }
  if (j.contains("updated_at") && !j["updated_at"].is_null()) {
    notif.updatedAt = juce::Time::fromISO8601(Json::toJuceString(j["updated_at"].get<std::string>()));
  }
}

} // namespace Sidechain
