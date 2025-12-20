#pragma once

#include "../util/json/JsonValidation.h"
#include <JuceHeader.h>
#include <nlohmann/json.hpp>
#include <vector>

namespace Sidechain {

// ==============================================================================
/**
 * Draft - Unsent post/message draft
 *
 * Represents a draft post or message that user is composing but hasn't sent yet.
 * Drafts are automatically saved locally and can be resumed later.
 */
struct Draft : public SerializableModel<Draft> {
  // ==============================================================================
  // Core identity
  juce::String id;   // Local UUID for the draft
  juce::String type; // "post", "message", "comment", "story"

  // ==============================================================================
  // Context (what this draft is for)
  juce::String contextId; // Conversation ID (for messages), Post ID (for comments)

  // ==============================================================================
  // Content
  juce::String text; // Draft text content

  // ==============================================================================
  // Attachments (local file paths)
  juce::String audioFilePath;
  juce::String midiFilePath;
  juce::String imageFilePath;

  // ==============================================================================
  // Audio metadata (if audio attached)
  float duration = 0.0f;
  int bpm = 0;
  juce::String key;

  // ==============================================================================
  // Post-specific metadata
  juce::StringArray genres;
  juce::String commentAudience = "everyone"; // "everyone", "followers", "off"

  // ==============================================================================
  // Timestamps
  juce::Time createdAt;
  juce::Time updatedAt;

  // ==============================================================================
  // Auto-recovery flag
  bool isAutoRecovery = false; // Draft created from crashed session

  // ==============================================================================
  // Typed JSON serialization
  SIDECHAIN_JSON_TYPE(Draft)

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
  bool hasContent() const {
    return text.isNotEmpty() || audioFilePath.isNotEmpty() || midiFilePath.isNotEmpty() || imageFilePath.isNotEmpty();
  }

  bool hasAudio() const {
    return audioFilePath.isNotEmpty();
  }

  bool hasMidi() const {
    return midiFilePath.isNotEmpty();
  }

  bool hasImage() const {
    return imageFilePath.isNotEmpty();
  }

  juce::String getDisplayTitle() const {
    if (type == "post")
      return "Draft Post";
    if (type == "message")
      return "Draft Message";
    if (type == "comment")
      return "Draft Comment";
    if (type == "story")
      return "Draft Story";
    return "Draft";
  }

  juce::String getPreviewText() const {
    if (text.isNotEmpty()) {
      return text.substring(0, 50) + (text.length() > 50 ? "..." : "");
    }
    if (hasAudio())
      return "Audio attachment";
    return "Empty draft";
  }
};

// ==============================================================================
// JSON Serialization Implementation

inline void to_json(nlohmann::json &j, const Draft &draft) {
  j = nlohmann::json{
      {"id", Json::fromJuceString(draft.id)},
      {"type", Json::fromJuceString(draft.type)},
      {"context_id", Json::fromJuceString(draft.contextId)},
      {"text", Json::fromJuceString(draft.text)},
      {"audio_file_path", Json::fromJuceString(draft.audioFilePath)},
      {"midi_file_path", Json::fromJuceString(draft.midiFilePath)},
      {"image_file_path", Json::fromJuceString(draft.imageFilePath)},
      {"duration", draft.duration},
      {"bpm", draft.bpm},
      {"key", Json::fromJuceString(draft.key)},
      {"comment_audience", Json::fromJuceString(draft.commentAudience)},
      {"is_auto_recovery", draft.isAutoRecovery},
      {"created_at", draft.createdAt.toISO8601(true).toStdString()},
      {"updated_at", draft.updatedAt.toISO8601(true).toStdString()},
  };

  // Add genres array
  if (!draft.genres.isEmpty()) {
    std::vector<std::string> genreVec;
    for (const auto &genre : draft.genres) {
      genreVec.push_back(genre.toStdString());
    }
    j["genres"] = genreVec;
  }
}

inline void from_json(const nlohmann::json &j, Draft &draft) {
  // Required fields
  JSON_REQUIRE_STRING(j, "id", draft.id);
  JSON_REQUIRE_STRING(j, "type", draft.type);

  // Optional fields
  JSON_OPTIONAL_STRING(j, "context_id", draft.contextId, "");
  JSON_OPTIONAL_STRING(j, "text", draft.text, "");
  JSON_OPTIONAL_STRING(j, "audio_file_path", draft.audioFilePath, "");
  JSON_OPTIONAL_STRING(j, "midi_file_path", draft.midiFilePath, "");
  JSON_OPTIONAL_STRING(j, "image_file_path", draft.imageFilePath, "");
  JSON_OPTIONAL_STRING(j, "key", draft.key, "");
  JSON_OPTIONAL_STRING(j, "comment_audience", draft.commentAudience, "everyone");

  JSON_OPTIONAL(j, "duration", draft.duration, 0.0f);
  JSON_OPTIONAL(j, "bpm", draft.bpm, 0);
  JSON_OPTIONAL(j, "is_auto_recovery", draft.isAutoRecovery, false);

  // Parse genres array
  if (j.contains("genres") && j["genres"].is_array()) {
    draft.genres.clear();
    for (const auto &genre : j["genres"]) {
      if (genre.is_string()) {
        draft.genres.add(Json::toJuceString(genre.get<std::string>()));
      }
    }
  }

  // Parse timestamps
  if (j.contains("created_at") && !j["created_at"].is_null()) {
    draft.createdAt = juce::Time::fromISO8601(Json::toJuceString(j["created_at"].get<std::string>()));
  }
  if (j.contains("updated_at") && !j["updated_at"].is_null()) {
    draft.updatedAt = juce::Time::fromISO8601(Json::toJuceString(j["updated_at"].get<std::string>()));
  }
}

} // namespace Sidechain
