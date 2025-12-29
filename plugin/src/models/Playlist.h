#pragma once

#include "../util/json/JsonValidation.h"
#include "../util/Json.h"
#include "../util/Result.h"
#include "../util/SerializableModel.h"
#include <JuceHeader.h>
#include <nlohmann/json.hpp>
#include <memory>

namespace Sidechain {

// ==============================================================================
/**
 * Playlist represents a collection of audio posts
 */
struct Playlist : public SerializableModel<Playlist> {
  // Equality comparison (by ID) - required for RxCpp observables
  bool operator==(const Playlist &other) const {
    return id == other.id;
  }
  bool operator!=(const Playlist &other) const {
    return id != other.id;
  }

  juce::String id;
  juce::String name;
  juce::String description;
  juce::String ownerId;
  juce::String ownerUsername;
  juce::String ownerAvatarUrl;
  bool isCollaborative = false;
  bool isPublic = true;
  int entryCount = 0;
  juce::Time createdAt;

  // User's role in this playlist (if applicable)
  juce::String userRole; // "owner", "editor", "viewer", or empty

  // Parse from JSON response
  static Playlist fromJSON(const juce::var &json);

  // Convert to JSON for upload
  juce::var toJSON() const;

  // Check if user can edit this playlist
  bool canEdit() const {
    return userRole == "owner" || userRole == "editor";
  }

  // Check if user is owner
  bool isOwner() const {
    return userRole == "owner";
  }

  // SerializableModel required method
  bool isValid() const {
    return !id.isEmpty() && !name.isEmpty();
  }
};

// ==============================================================================
// JSON Serialization (for nlohmann::json / SerializableModel<Playlist>)
// These are declared here for ADL to find them

inline void to_json(nlohmann::json &j, const Playlist &playlist);
inline void from_json(const nlohmann::json &j, Playlist &playlist);

// ==============================================================================
// nlohmann::json implementations (inline in header for proper linking)

inline void to_json(nlohmann::json &j, const Playlist &playlist) {
  j = nlohmann::json{
      {"id", Json::fromJuceString(playlist.id)},
      {"name", Json::fromJuceString(playlist.name)},
      {"description", Json::fromJuceString(playlist.description)},
      {"owner_id", Json::fromJuceString(playlist.ownerId)},
      {"owner_username", Json::fromJuceString(playlist.ownerUsername)},
      {"owner_avatar_url", Json::fromJuceString(playlist.ownerAvatarUrl)},
      {"is_collaborative", playlist.isCollaborative},
      {"is_public", playlist.isPublic},
      {"entry_count", playlist.entryCount},
      {"user_role", Json::fromJuceString(playlist.userRole)},
      {"created_at", playlist.createdAt.toISO8601(true).toStdString()},
  };
}

inline void from_json(const nlohmann::json &j, Playlist &playlist) {
  JSON_OPTIONAL_STRING(j, "id", playlist.id, "");
  JSON_OPTIONAL_STRING(j, "name", playlist.name, "");
  JSON_OPTIONAL_STRING(j, "description", playlist.description, "");
  JSON_OPTIONAL_STRING(j, "owner_id", playlist.ownerId, "");
  JSON_OPTIONAL_STRING(j, "owner_username", playlist.ownerUsername, "");
  JSON_OPTIONAL_STRING(j, "owner_avatar_url", playlist.ownerAvatarUrl, "");
  JSON_OPTIONAL(j, "is_collaborative", playlist.isCollaborative, false);
  JSON_OPTIONAL(j, "is_public", playlist.isPublic, true);
  JSON_OPTIONAL(j, "entry_count", playlist.entryCount, 0);
  JSON_OPTIONAL_STRING(j, "user_role", playlist.userRole, "");

  // Parse timestamp
  if (j.contains("created_at") && !j["created_at"].is_null()) {
    try {
      playlist.createdAt = juce::Time::fromISO8601(Json::toJuceString(j["created_at"].get<std::string>()));
    } catch (...) {
      // Invalid timestamp format
    }
  }
}

// ==============================================================================
/**
 * PlaylistEntry represents a post in a playlist
 */
struct PlaylistEntry {
  juce::String id;
  juce::String playlistId;
  juce::String postId;
  juce::String addedByUserId;
  juce::String addedByUsername;
  int position = 0;
  juce::Time addedAt;

  // Post data (loaded when viewing playlist)
  juce::String postAudioUrl;
  juce::String postUsername;
  int postBpm = 0;
  juce::String postKey;
  juce::StringArray postGenres;

  // Parse from JSON response
  static PlaylistEntry fromJSON(const juce::var &json);
};

// ==============================================================================
/**
 * PlaylistCollaborator represents a user who can collaborate on a playlist
 */
struct PlaylistCollaborator {
  juce::String id;
  juce::String playlistId;
  juce::String userId;
  juce::String username;
  juce::String userAvatarUrl;
  juce::String role; // "owner", "editor", "viewer"
  juce::Time addedAt;

  // Parse from JSON response
  static PlaylistCollaborator fromJSON(const juce::var &json);
};

} // namespace Sidechain
