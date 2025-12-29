#pragma once

#include <JuceHeader.h>
#include "../util/SerializableModel.h"
#include "../util/json/JsonValidation.h"
#include <nlohmann/json.hpp>

namespace Sidechain {

// ==============================================================================
/**
 * Sound represents a unique audio signature detected across posts
 *
 * When the same sample/sound is used in multiple posts, they are linked
 * to the same Sound, enabling "See X other posts with this sound" discovery.
 */
struct Sound : public SerializableModel<Sound> {
  // Equality comparison (by ID) - required for RxCpp observables
  bool operator==(const Sound &other) const {
    return id == other.id;
  }
  bool operator!=(const Sound &other) const {
    return id != other.id;
  }

  juce::String id;
  juce::String name;
  juce::String description;
  double duration = 0.0; // Duration in seconds

  // Creator info
  juce::String creatorId;
  juce::String creatorUsername;
  juce::String creatorDisplayName;
  juce::String creatorAvatarUrl;

  // Original post that first used this sound
  juce::String originalPostId;

  // Usage statistics
  int usageCount = 0; // Number of posts using this sound
  bool isTrending = false;
  int trendingRank = 0;

  bool isPublic = true;
  juce::Time createdAt;

  // Validation
  bool isValid() const {
    return id.isNotEmpty() && name.isNotEmpty();
  }

  // ==========================================================================
  // Display helpers

  /** Get formatted usage count string (e.g., "1.2K posts") */
  juce::String getUsageCountString() const {
    if (usageCount >= 1000000)
      return juce::String(usageCount / 1000000.0, 1) + "M posts";
    if (usageCount >= 1000)
      return juce::String(usageCount / 1000.0, 1) + "K posts";
    if (usageCount == 1)
      return "1 post";
    return juce::String(usageCount) + " posts";
  }

  /** Get formatted duration string (e.g., "0:15") */
  juce::String getDurationString() const {
    int totalSeconds = static_cast<int>(duration);
    int minutes = totalSeconds / 60;
    int seconds = totalSeconds % 60;
    return juce::String(minutes) + ":" + juce::String(seconds).paddedLeft('0', 2);
  }

  /** Get creator display name or username */
  juce::String getCreatorName() const {
    if (creatorDisplayName.isNotEmpty())
      return creatorDisplayName;
    return creatorUsername;
  }
};

// ==============================================================================
// JSON Serialization

inline void to_json(nlohmann::json &j, const Sound &sound) {
  j = nlohmann::json{
      {"id", Json::fromJuceString(sound.id)},
      {"name", Json::fromJuceString(sound.name)},
      {"description", Json::fromJuceString(sound.description)},
      {"duration", sound.duration},
      {"creator_id", Json::fromJuceString(sound.creatorId)},
      {"creator_username", Json::fromJuceString(sound.creatorUsername)},
      {"creator_display_name", Json::fromJuceString(sound.creatorDisplayName)},
      {"creator_avatar_url", Json::fromJuceString(sound.creatorAvatarUrl)},
      {"original_post_id", Json::fromJuceString(sound.originalPostId)},
      {"usage_count", sound.usageCount},
      {"is_trending", sound.isTrending},
      {"trending_rank", sound.trendingRank},
      {"is_public", sound.isPublic},
      {"created_at", sound.createdAt.toISO8601(true).toStdString()},
  };
}

inline void from_json(const nlohmann::json &j, Sound &sound) {
  JSON_OPTIONAL_STRING(j, "id", sound.id, "");
  JSON_OPTIONAL_STRING(j, "name", sound.name, "");
  JSON_OPTIONAL_STRING(j, "description", sound.description, "");
  JSON_OPTIONAL(j, "duration", sound.duration, 0.0);
  JSON_OPTIONAL_STRING(j, "creator_id", sound.creatorId, "");
  JSON_OPTIONAL_STRING(j, "creator_username", sound.creatorUsername, "");
  JSON_OPTIONAL_STRING(j, "creator_display_name", sound.creatorDisplayName, "");
  JSON_OPTIONAL_STRING(j, "creator_avatar_url", sound.creatorAvatarUrl, "");
  JSON_OPTIONAL_STRING(j, "original_post_id", sound.originalPostId, "");
  JSON_OPTIONAL(j, "usage_count", sound.usageCount, 0);
  JSON_OPTIONAL(j, "is_trending", sound.isTrending, false);
  JSON_OPTIONAL(j, "trending_rank", sound.trendingRank, 0);
  JSON_OPTIONAL(j, "is_public", sound.isPublic, true);

  // Parse timestamp
  if (j.contains("created_at") && !j["created_at"].is_null()) {
    try {
      sound.createdAt = juce::Time::fromISO8601(Json::toJuceString(j["created_at"].get<std::string>()));
    } catch (...) {
      // Invalid timestamp format
    }
  }
}

// ==============================================================================
/**
 * SoundPost represents a post that uses a specific sound
 */
struct SoundPost : public SerializableModel<SoundPost> {
  // Equality comparison (by ID) - required for RxCpp observables
  bool operator==(const SoundPost &other) const {
    return id == other.id;
  }
  bool operator!=(const SoundPost &other) const {
    return id != other.id;
  }

  juce::String id;
  juce::String audioUrl;
  double duration = 0.0;
  int bpm = 0;
  juce::String key;
  juce::String waveformSvg;
  int likeCount = 0;
  int playCount = 0;
  juce::Time createdAt;

  // User info
  juce::String userId;
  juce::String username;
  juce::String displayName;
  juce::String avatarUrl;

  bool isValid() const {
    return id.isNotEmpty() && audioUrl.isNotEmpty();
  }

  juce::String getUserDisplayName() const {
    if (displayName.isNotEmpty())
      return displayName;
    return username;
  }
};

// ==============================================================================
// JSON Serialization for SoundPost

inline void to_json(nlohmann::json &j, const SoundPost &post) {
  j = nlohmann::json{
      {"id", Json::fromJuceString(post.id)},
      {"audio_url", Json::fromJuceString(post.audioUrl)},
      {"duration", post.duration},
      {"bpm", post.bpm},
      {"key", Json::fromJuceString(post.key)},
      {"waveform_svg", Json::fromJuceString(post.waveformSvg)},
      {"like_count", post.likeCount},
      {"play_count", post.playCount},
      {"created_at", post.createdAt.toISO8601(true).toStdString()},
      {"user_id", Json::fromJuceString(post.userId)},
      {"username", Json::fromJuceString(post.username)},
      {"display_name", Json::fromJuceString(post.displayName)},
      {"avatar_url", Json::fromJuceString(post.avatarUrl)},
  };
}

inline void from_json(const nlohmann::json &j, SoundPost &post) {
  JSON_OPTIONAL_STRING(j, "id", post.id, "");
  JSON_OPTIONAL_STRING(j, "audio_url", post.audioUrl, "");
  JSON_OPTIONAL(j, "duration", post.duration, 0.0);
  JSON_OPTIONAL(j, "bpm", post.bpm, 0);
  JSON_OPTIONAL_STRING(j, "key", post.key, "");
  JSON_OPTIONAL_STRING(j, "waveform_svg", post.waveformSvg, "");
  JSON_OPTIONAL(j, "like_count", post.likeCount, 0);
  JSON_OPTIONAL(j, "play_count", post.playCount, 0);

  JSON_OPTIONAL_STRING(j, "user_id", post.userId, "");
  JSON_OPTIONAL_STRING(j, "username", post.username, "");
  JSON_OPTIONAL_STRING(j, "display_name", post.displayName, "");
  JSON_OPTIONAL_STRING(j, "avatar_url", post.avatarUrl, "");

  // Parse timestamp
  if (j.contains("created_at") && !j["created_at"].is_null()) {
    try {
      post.createdAt = juce::Time::fromISO8601(Json::toJuceString(j["created_at"].get<std::string>()));
    } catch (...) {
      // Invalid timestamp format
    }
  }
}

} // namespace Sidechain
