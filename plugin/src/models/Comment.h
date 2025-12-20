#pragma once

#include "../util/json/JsonValidation.h"
#include "../util/SerializableModel.h"
#include <JuceHeader.h>
#include <nlohmann/json.hpp>
#include <map>
#include <vector>

namespace Sidechain {

// ==============================================================================
/**
 * CommentReaction - Emoji reaction on a comment
 *
 * Maps to user reactions like 👍, ❤️, etc. on comments
 */
struct CommentReaction {
  juce::String emoji;               // Emoji character (e.g., "👍")
  std::vector<std::string> userIds; // User IDs who reacted with this emoji

  bool isEmpty() const {
    return emoji.isEmpty() || userIds.empty();
  }
};

// JSON serialization for CommentReaction
inline void to_json(nlohmann::json &j, const CommentReaction &reaction) {
  j = nlohmann::json{
      {"emoji", Json::fromJuceString(reaction.emoji)},
      {"user_ids", reaction.userIds},
  };
}

inline void from_json(const nlohmann::json &j, CommentReaction &reaction) {
  JSON_OPTIONAL_STRING(j, "emoji", reaction.emoji, "");

  if (j.contains("user_ids") && j["user_ids"].is_array()) {
    reaction.userIds = j["user_ids"].get<std::vector<std::string>>();
  }
}

// ==============================================================================
/**
 * Comment - Comment on a feed post
 *
 * Represents a user comment with optional replies and reactions.
 * Comments can be top-level or replies (parentId is non-empty for replies).
 */
struct Comment : public SerializableModel<Comment> {
  // ==============================================================================
  // Core identity
  juce::String id;       // Unique comment ID
  juce::String postId;   // The post this comment belongs to
  juce::String userId;   // Author of the comment
  juce::String username; // Author's username for display

  // ==============================================================================
  // Content
  juce::String content;       // Comment text
  juce::String userAvatarUrl; // Author's avatar

  // ==============================================================================
  // Threading (replies)
  juce::String parentId; // For threaded replies (empty for top-level)

  // ==============================================================================
  // Reactions (emoji -> list of user IDs)
  std::map<std::string, std::vector<std::string>> reactions;

  // ==============================================================================
  // User's relationship to this comment
  bool isLiked = false;      // Whether current user has liked this comment
  bool isOwnComment = false; // Whether current user authored this comment
  bool canEdit = false;      // Within 5-minute edit window

  // ==============================================================================
  // Stats
  int likeCount = 0; // Number of likes this comment has

  // ==============================================================================
  // Timestamps
  juce::Time createdAt;
  juce::String timeAgo; // Human-readable time (e.g., "2h ago")

  // ==============================================================================
  // Typed JSON serialization
  SIDECHAIN_JSON_TYPE(Comment)

  // ==============================================================================
  // Validation
  bool isValid() const {
    return id.isNotEmpty() && postId.isNotEmpty() && userId.isNotEmpty() && content.isNotEmpty();
  }

  juce::String getId() const {
    return id;
  }

  // ==============================================================================
  // Display helpers
  bool isReply() const {
    return parentId.isNotEmpty();
  }

  int getTotalReactionCount() const {
    int count = 0;
    for (const auto &[emoji, users] : reactions) {
      count += static_cast<int>(users.size());
    }
    return count;
  }

  bool hasReaction(const juce::String &emoji) const {
    auto stdEmoji = emoji.toStdString();
    return reactions.find(stdEmoji) != reactions.end();
  }
};

// ==============================================================================
// JSON Serialization Implementation

inline void to_json(nlohmann::json &j, const Comment &comment) {
  j = nlohmann::json{
      {"id", Json::fromJuceString(comment.id)},
      {"post_id", Json::fromJuceString(comment.postId)},
      {"user_id", Json::fromJuceString(comment.userId)},
      {"username", Json::fromJuceString(comment.username)},
      {"content", Json::fromJuceString(comment.content)},
      {"user_avatar_url", Json::fromJuceString(comment.userAvatarUrl)},
      {"parent_id", Json::fromJuceString(comment.parentId)},
      {"is_liked", comment.isLiked},
      {"is_own_comment", comment.isOwnComment},
      {"can_edit", comment.canEdit},
      {"like_count", comment.likeCount},
      {"created_at", comment.createdAt.toISO8601(true).toStdString()},
      {"time_ago", Json::fromJuceString(comment.timeAgo)},
  };

  // Add reactions if present
  if (!comment.reactions.empty()) {
    j["reactions"] = comment.reactions;
  }
}

inline void from_json(const nlohmann::json &j, Comment &comment) {
  // Required fields
  JSON_REQUIRE_STRING(j, "id", comment.id);
  JSON_REQUIRE_STRING(j, "post_id", comment.postId);
  JSON_REQUIRE_STRING(j, "user_id", comment.userId);
  JSON_REQUIRE_STRING(j, "content", comment.content);

  // Optional fields
  JSON_OPTIONAL_STRING(j, "username", comment.username, "");
  JSON_OPTIONAL_STRING(j, "user_avatar_url", comment.userAvatarUrl, "");
  JSON_OPTIONAL_STRING(j, "avatar_url", comment.userAvatarUrl, ""); // Alternative field name
  JSON_OPTIONAL_STRING(j, "parent_id", comment.parentId, "");
  JSON_OPTIONAL_STRING(j, "time_ago", comment.timeAgo, "");

  JSON_OPTIONAL(j, "is_liked", comment.isLiked, false);
  JSON_OPTIONAL(j, "is_own_comment", comment.isOwnComment, false);
  JSON_OPTIONAL(j, "can_edit", comment.canEdit, false);
  JSON_OPTIONAL(j, "like_count", comment.likeCount, 0);

  // Parse timestamp
  if (j.contains("created_at") && !j["created_at"].is_null()) {
    comment.createdAt = juce::Time::fromISO8601(Json::toJuceString(j["created_at"].get<std::string>()));
  }

  // Parse reactions (emoji -> user IDs)
  if (j.contains("reactions") && j["reactions"].is_object()) {
    comment.reactions = j["reactions"].get<std::map<std::string, std::vector<std::string>>>();
  }
}

} // namespace Sidechain
