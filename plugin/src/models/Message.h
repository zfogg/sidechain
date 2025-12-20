#pragma once

#include "../util/json/JsonValidation.h"
#include <JuceHeader.h>
#include <nlohmann/json.hpp>
#include <unordered_map>
#include <vector>

namespace Sidechain {

// ==============================================================================
/**
 * MessageAttachment - Attached media/files in a message
 */
struct MessageAttachment {
  juce::String type;     // "image", "audio", "video", "file", "loop", "midi"
  juce::String url;      // URL to the attached file
  juce::String filename; // Original filename
  int64_t size = 0;      // File size in bytes
  juce::String mimeType; // MIME type

  // For audio/video
  float duration = 0.0f; // Duration in seconds

  // For images
  int width = 0;
  int height = 0;
  juce::String thumbnailUrl;
};

// JSON serialization for MessageAttachment
inline void to_json(nlohmann::json &j, const MessageAttachment &att) {
  j = nlohmann::json{
      {"type", Json::fromJuceString(att.type)},
      {"url", Json::fromJuceString(att.url)},
      {"filename", Json::fromJuceString(att.filename)},
      {"size", att.size},
      {"mime_type", Json::fromJuceString(att.mimeType)},
      {"duration", att.duration},
      {"width", att.width},
      {"height", att.height},
      {"thumbnail_url", Json::fromJuceString(att.thumbnailUrl)},
  };
}

inline void from_json(const nlohmann::json &j, MessageAttachment &att) {
  JSON_OPTIONAL_STRING(j, "type", att.type, "");
  JSON_OPTIONAL_STRING(j, "url", att.url, "");
  JSON_OPTIONAL_STRING(j, "filename", att.filename, "");
  JSON_OPTIONAL_STRING(j, "mime_type", att.mimeType, "");
  JSON_OPTIONAL_STRING(j, "thumbnail_url", att.thumbnailUrl, "");

  JSON_OPTIONAL(j, "size", att.size, 0);
  JSON_OPTIONAL(j, "duration", att.duration, 0.0f);
  JSON_OPTIONAL(j, "width", att.width, 0);
  JSON_OPTIONAL(j, "height", att.height, 0);
}

// ==============================================================================
/**
 * Message - Chat message entity
 *
 * Represents a message in a conversation with text, attachments, and metadata.
 */
struct Message : public SerializableModel<Message> {
  // ==============================================================================
  // Core identity
  juce::String id;
  juce::String conversationId;
  juce::String senderId;
  juce::String senderUsername;
  juce::String senderAvatarUrl;

  // ==============================================================================
  // Content
  juce::String text;
  std::vector<MessageAttachment> attachments;

  // ==============================================================================
  // Reply/Thread
  juce::String replyToId;       // ID of message this is replying to
  juce::String replyToText;     // Preview of replied message
  juce::String replyToSenderId; // Sender of replied message

  // ==============================================================================
  // Reactions (emoji -> list of user IDs)
  std::unordered_map<std::string, std::vector<std::string>> reactions;

  // ==============================================================================
  // Status
  bool isEdited = false;
  bool isDeleted = false;
  bool isSilent = false; // Silent message (no notification)
  bool isPinned = false;

  // ==============================================================================
  // Read receipts
  std::vector<std::string> readBy; // User IDs who have read this message

  // ==============================================================================
  // Timestamps
  juce::Time createdAt;
  juce::Time updatedAt;
  juce::Time deletedAt;

  // ==============================================================================
  // Metadata (custom data like post references, etc.)
  std::unordered_map<std::string, std::string> metadata;

  // ==============================================================================
  // Typed JSON serialization
  SIDECHAIN_JSON_TYPE(Message)

  // ==============================================================================
  // Validation
  bool isValid() const {
    return id.isNotEmpty() && conversationId.isNotEmpty() && senderId.isNotEmpty();
  }

  juce::String getId() const {
    return id;
  }

  // ==============================================================================
  // Display helpers
  bool hasAttachments() const {
    return !attachments.empty();
  }

  bool isReply() const {
    return replyToId.isNotEmpty();
  }

  int getReactionCount() const {
    int count = 0;
    for (const auto &[emoji, users] : reactions) {
      count += static_cast<int>(users.size());
    }
    return count;
  }

  bool isReadBy(const juce::String &userId) const {
    auto stdUserId = userId.toStdString();
    return std::find(readBy.begin(), readBy.end(), stdUserId) != readBy.end();
  }
};

// ==============================================================================
// JSON Serialization Implementation

inline void to_json(nlohmann::json &j, const Message &msg) {
  j = nlohmann::json{
      {"id", Json::fromJuceString(msg.id)},
      {"conversation_id", Json::fromJuceString(msg.conversationId)},
      {"sender_id", Json::fromJuceString(msg.senderId)},
      {"sender_username", Json::fromJuceString(msg.senderUsername)},
      {"sender_avatar_url", Json::fromJuceString(msg.senderAvatarUrl)},
      {"text", Json::fromJuceString(msg.text)},
      {"reply_to_id", Json::fromJuceString(msg.replyToId)},
      {"reply_to_text", Json::fromJuceString(msg.replyToText)},
      {"reply_to_sender_id", Json::fromJuceString(msg.replyToSenderId)},
      {"is_edited", msg.isEdited},
      {"is_deleted", msg.isDeleted},
      {"is_silent", msg.isSilent},
      {"is_pinned", msg.isPinned},
      {"created_at", msg.createdAt.toISO8601(true).toStdString()},
      {"updated_at", msg.updatedAt.toISO8601(true).toStdString()},
  };

  // Add attachments
  if (!msg.attachments.empty()) {
    j["attachments"] = msg.attachments;
  }

  // Add reactions
  if (!msg.reactions.empty()) {
    j["reactions"] = msg.reactions;
  }

  // Add read receipts
  if (!msg.readBy.empty()) {
    j["read_by"] = msg.readBy;
  }

  // Add metadata
  if (!msg.metadata.empty()) {
    j["metadata"] = msg.metadata;
  }

  // Add deleted timestamp if deleted
  if (msg.isDeleted && msg.deletedAt != juce::Time()) {
    j["deleted_at"] = msg.deletedAt.toISO8601(true).toStdString();
  }
}

inline void from_json(const nlohmann::json &j, Message &msg) {
  // Required fields
  JSON_REQUIRE_STRING(j, "id", msg.id);
  JSON_REQUIRE_STRING(j, "conversation_id", msg.conversationId);
  JSON_REQUIRE_STRING(j, "sender_id", msg.senderId);

  // Optional fields
  JSON_OPTIONAL_STRING(j, "sender_username", msg.senderUsername, "");
  JSON_OPTIONAL_STRING(j, "sender_avatar_url", msg.senderAvatarUrl, "");
  JSON_OPTIONAL_STRING(j, "text", msg.text, "");
  JSON_OPTIONAL_STRING(j, "reply_to_id", msg.replyToId, "");
  JSON_OPTIONAL_STRING(j, "reply_to_text", msg.replyToText, "");
  JSON_OPTIONAL_STRING(j, "reply_to_sender_id", msg.replyToSenderId, "");

  JSON_OPTIONAL(j, "is_edited", msg.isEdited, false);
  JSON_OPTIONAL(j, "is_deleted", msg.isDeleted, false);
  JSON_OPTIONAL(j, "is_silent", msg.isSilent, false);
  JSON_OPTIONAL(j, "is_pinned", msg.isPinned, false);

  // Parse attachments
  if (j.contains("attachments") && j["attachments"].is_array()) {
    msg.attachments = j["attachments"].get<std::vector<MessageAttachment>>();
  }

  // Parse reactions
  if (j.contains("reactions") && j["reactions"].is_object()) {
    msg.reactions = j["reactions"].get<std::unordered_map<std::string, std::vector<std::string>>>();
  }

  // Parse read receipts
  if (j.contains("read_by") && j["read_by"].is_array()) {
    msg.readBy = j["read_by"].get<std::vector<std::string>>();
  }

  // Parse timestamps
  if (j.contains("created_at") && !j["created_at"].is_null()) {
    msg.createdAt = juce::Time::fromISO8601(Json::toJuceString(j["created_at"].get<std::string>()));
  }
  if (j.contains("updated_at") && !j["updated_at"].is_null()) {
    msg.updatedAt = juce::Time::fromISO8601(Json::toJuceString(j["updated_at"].get<std::string>()));
  }
  if (j.contains("deleted_at") && !j["deleted_at"].is_null()) {
    msg.deletedAt = juce::Time::fromISO8601(Json::toJuceString(j["deleted_at"].get<std::string>()));
  }

  // Parse metadata
  if (j.contains("metadata") && j["metadata"].is_object()) {
    msg.metadata = j["metadata"].get<std::unordered_map<std::string, std::string>>();
  }
}

} // namespace Sidechain
