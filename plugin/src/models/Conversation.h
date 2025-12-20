#pragma once

#include "../util/json/JsonValidation.h"
#include <JuceHeader.h>
#include <nlohmann/json.hpp>
#include <vector>

namespace Sidechain {

// ==============================================================================
/**
 * Conversation - Chat conversation entity
 *
 * Represents a direct message conversation or group chat.
 * Contains metadata about the conversation and its participants.
 */
struct Conversation {
  // ==============================================================================
  // Core identity
  juce::String id;
  juce::String type; // "direct" or "group"
  juce::String name; // Display name (for group chats)

  // ==============================================================================
  // Participants
  std::vector<std::string> memberIds; // User IDs of all members
  int memberCount = 0;

  // ==============================================================================
  // Last message preview
  juce::String lastMessageId;
  juce::String lastMessageText;
  juce::String lastMessageSenderId;
  juce::Time lastMessageAt;

  // ==============================================================================
  // Status
  int unreadCount = 0;
  bool isMuted = false;
  bool isPinned = false;
  bool isArchived = false;

  // ==============================================================================
  // Timestamps
  juce::Time createdAt;
  juce::Time updatedAt;

  // ==============================================================================
  // Metadata (custom data like avatar, description for group chats)
  std::unordered_map<std::string, std::string> metadata;

  // ==============================================================================
  // Typed JSON serialization
  SIDECHAIN_JSON_TYPE(Conversation)

  // ==============================================================================
  // Validation
  bool isValid() const {
    return id.isNotEmpty() && type.isNotEmpty();
  }

  juce::String getId() const {
    return id;
  }

  // ==============================================================================
  // Display helpers
  bool isDirect() const {
    return type == "direct";
  }

  bool isGroup() const {
    return type == "group";
  }

  juce::String getDisplayName() const {
    return name.isNotEmpty() ? name : juce::String("Conversation");
  }
};

// ==============================================================================
// JSON Serialization Implementation

inline void to_json(nlohmann::json &j, const Conversation &conv) {
  j = nlohmann::json{
      {"id", Json::fromJuceString(conv.id)},
      {"type", Json::fromJuceString(conv.type)},
      {"name", Json::fromJuceString(conv.name)},
      {"member_ids", conv.memberIds},
      {"member_count", conv.memberCount},
      {"last_message_id", Json::fromJuceString(conv.lastMessageId)},
      {"last_message_text", Json::fromJuceString(conv.lastMessageText)},
      {"last_message_sender_id", Json::fromJuceString(conv.lastMessageSenderId)},
      {"last_message_at", conv.lastMessageAt.toISO8601(true).toStdString()},
      {"unread_count", conv.unreadCount},
      {"is_muted", conv.isMuted},
      {"is_pinned", conv.isPinned},
      {"is_archived", conv.isArchived},
      {"created_at", conv.createdAt.toISO8601(true).toStdString()},
      {"updated_at", conv.updatedAt.toISO8601(true).toStdString()},
  };

  // Add metadata if present
  if (!conv.metadata.empty()) {
    j["metadata"] = conv.metadata;
  }
}

inline void from_json(const nlohmann::json &j, Conversation &conv) {
  // Required fields
  JSON_REQUIRE_STRING(j, "id", conv.id);
  JSON_REQUIRE_STRING(j, "type", conv.type);

  // Optional fields
  JSON_OPTIONAL_STRING(j, "name", conv.name, "");
  JSON_OPTIONAL_STRING(j, "last_message_id", conv.lastMessageId, "");
  JSON_OPTIONAL_STRING(j, "last_message_text", conv.lastMessageText, "");
  JSON_OPTIONAL_STRING(j, "last_message_sender_id", conv.lastMessageSenderId, "");

  JSON_OPTIONAL(j, "member_count", conv.memberCount, 0);
  JSON_OPTIONAL(j, "unread_count", conv.unreadCount, 0);
  JSON_OPTIONAL(j, "is_muted", conv.isMuted, false);
  JSON_OPTIONAL(j, "is_pinned", conv.isPinned, false);
  JSON_OPTIONAL(j, "is_archived", conv.isArchived, false);

  // Parse member IDs
  if (j.contains("member_ids") && j["member_ids"].is_array()) {
    conv.memberIds = j["member_ids"].get<std::vector<std::string>>();
  }

  // Parse timestamps
  if (j.contains("created_at") && !j["created_at"].is_null()) {
    conv.createdAt = juce::Time::fromISO8601(Json::toJuceString(j["created_at"].get<std::string>()));
  }
  if (j.contains("updated_at") && !j["updated_at"].is_null()) {
    conv.updatedAt = juce::Time::fromISO8601(Json::toJuceString(j["updated_at"].get<std::string>()));
  }
  if (j.contains("last_message_at") && !j["last_message_at"].is_null()) {
    conv.lastMessageAt = juce::Time::fromISO8601(Json::toJuceString(j["last_message_at"].get<std::string>()));
  }

  // Parse metadata
  if (j.contains("metadata") && j["metadata"].is_object()) {
    conv.metadata = j["metadata"].get<std::unordered_map<std::string, std::string>>();
  }
}

} // namespace Sidechain
