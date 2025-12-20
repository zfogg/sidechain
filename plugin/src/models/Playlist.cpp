#include "Playlist.h"
#include "../util/Json.h"

namespace Sidechain {

// ==============================================================================
Playlist Playlist::fromJSON(const juce::var &json) {
  Playlist playlist;

  playlist.id = ::Json::getString(json, "id");
  playlist.name = ::Json::getString(json, "name");
  playlist.description = ::Json::getString(json, "description");
  playlist.ownerId = ::Json::getString(json, "owner_id");
  playlist.isCollaborative = ::Json::getBool(json, "is_collaborative");
  playlist.isPublic = ::Json::getBool(json, "is_public");

  // Parse owner info if present
  if (json.hasProperty("owner")) {
    auto owner = json["owner"];
    playlist.ownerUsername = ::Json::getString(owner, "username");
    playlist.ownerAvatarUrl = ::Json::getString(owner, "avatar_url");
  }

  // Parse entry count
  if (json.hasProperty("entries")) {
    auto entries = json["entries"];
    if (entries.isArray())
      playlist.entryCount = entries.size();
  } else {
    playlist.entryCount = ::Json::getInt(json, "entry_count");
  }

  // Parse user role if present
  playlist.userRole = ::Json::getString(json, "user_role");

  // Parse timestamp
  juce::String timeStr = ::Json::getString(json, "created_at");
  if (timeStr.isNotEmpty()) {
    playlist.createdAt = juce::Time::fromISO8601(timeStr);
  }

  return playlist;
}

juce::var Playlist::toJSON() const {
  auto *obj = new juce::DynamicObject();
  obj->setProperty("name", name);
  obj->setProperty("description", description);
  obj->setProperty("is_collaborative", isCollaborative);
  obj->setProperty("is_public", isPublic);
  return juce::var(obj);
}

// ==============================================================================
PlaylistEntry PlaylistEntry::fromJSON(const juce::var &json) {
  PlaylistEntry entry;

  entry.id = ::Json::getString(json, "id");
  entry.playlistId = ::Json::getString(json, "playlist_id");
  entry.postId = ::Json::getString(json, "post_id");
  entry.addedByUserId = ::Json::getString(json, "added_by_user_id");
  entry.position = ::Json::getInt(json, "position");

  // Parse added by user info
  if (json.hasProperty("added_by_user")) {
    auto user = json["added_by_user"];
    entry.addedByUsername = ::Json::getString(user, "username");
  }

  // Parse post data if present
  if (json.hasProperty("post")) {
    auto post = json["post"];
    entry.postAudioUrl = ::Json::getString(post, "audio_url");
    entry.postBpm = ::Json::getInt(post, "bpm");
    entry.postKey = ::Json::getString(post, "key");

    // Parse genres
    auto genres = ::Json::getArray(post, "genre");
    if (::Json::isArray(genres)) {
      for (int i = 0; i < ::Json::arraySize(genres); ++i)
        entry.postGenres.add(::Json::getStringAt(genres, i));
    }

    // Parse post user
    if (post.hasProperty("user")) {
      entry.postUsername = ::Json::getString(post["user"], "username");
    }
  }

  // Parse timestamp
  juce::String timeStr = ::Json::getString(json, "added_at");
  if (timeStr.isNotEmpty()) {
    entry.addedAt = juce::Time::fromISO8601(timeStr);
  }

  return entry;
}

// ==============================================================================
PlaylistCollaborator PlaylistCollaborator::fromJSON(const juce::var &json) {
  PlaylistCollaborator collab;

  collab.id = ::Json::getString(json, "id");
  collab.playlistId = ::Json::getString(json, "playlist_id");
  collab.userId = ::Json::getString(json, "user_id");
  collab.role = ::Json::getString(json, "role");

  // Parse user info if present
  if (json.hasProperty("user")) {
    auto user = json["user"];
    collab.username = ::Json::getString(user, "username");
    collab.userAvatarUrl = ::Json::getString(user, "avatar_url");
  }

  // Parse timestamp
  juce::String timeStr = ::Json::getString(json, "added_at");
  if (timeStr.isNotEmpty()) {
    collab.addedAt = juce::Time::fromISO8601(timeStr);
  }

  return collab;
}

// ==============================================================================
// PlaylistEntry and PlaylistCollaborator inline implementations remain in CPP

inline void to_json(nlohmann::json &j, const PlaylistEntry &entry) {
  j = nlohmann::json{
      {"id", Json::fromJuceString(entry.id)},
      {"playlist_id", Json::fromJuceString(entry.playlistId)},
      {"post_id", Json::fromJuceString(entry.postId)},
      {"added_by_user_id", Json::fromJuceString(entry.addedByUserId)},
      {"added_by_username", Json::fromJuceString(entry.addedByUsername)},
      {"position", entry.position},
      {"post_audio_url", Json::fromJuceString(entry.postAudioUrl)},
      {"post_username", Json::fromJuceString(entry.postUsername)},
      {"post_bpm", entry.postBpm},
      {"post_key", Json::fromJuceString(entry.postKey)},
      {"added_at", entry.addedAt.toISO8601(true).toStdString()},
  };

  // Add genres array
  std::vector<std::string> genresVec;
  for (const auto &genre : entry.postGenres) {
    genresVec.push_back(Json::fromJuceString(genre));
  }
  j["post_genres"] = genresVec;
}

inline void from_json(const nlohmann::json &j, PlaylistEntry &entry) {
  JSON_OPTIONAL_STRING(j, "id", entry.id, "");
  JSON_OPTIONAL_STRING(j, "playlist_id", entry.playlistId, "");
  JSON_OPTIONAL_STRING(j, "post_id", entry.postId, "");
  JSON_OPTIONAL_STRING(j, "added_by_user_id", entry.addedByUserId, "");
  JSON_OPTIONAL_STRING(j, "added_by_username", entry.addedByUsername, "");
  JSON_OPTIONAL(j, "position", entry.position, 0);
  JSON_OPTIONAL_STRING(j, "post_audio_url", entry.postAudioUrl, "");
  JSON_OPTIONAL_STRING(j, "post_username", entry.postUsername, "");
  JSON_OPTIONAL(j, "post_bpm", entry.postBpm, 0);
  JSON_OPTIONAL_STRING(j, "post_key", entry.postKey, "");

  // Parse genres array
  if (j.contains("post_genres") && j["post_genres"].is_array()) {
    for (const auto &genreJson : j["post_genres"]) {
      if (genreJson.is_string()) {
        entry.postGenres.add(Json::toJuceString(genreJson.get<std::string>()));
      }
    }
  }

  // Parse timestamp
  if (j.contains("added_at") && !j["added_at"].is_null()) {
    try {
      entry.addedAt = juce::Time::fromISO8601(Json::toJuceString(j["added_at"].get<std::string>()));
    } catch (...) {
      // Invalid timestamp format
    }
  }
}

// ==============================================================================
// nlohmann::json serialization for PlaylistCollaborator

inline void to_json(nlohmann::json &j, const PlaylistCollaborator &collab) {
  j = nlohmann::json{
      {"id", Json::fromJuceString(collab.id)},
      {"playlist_id", Json::fromJuceString(collab.playlistId)},
      {"user_id", Json::fromJuceString(collab.userId)},
      {"username", Json::fromJuceString(collab.username)},
      {"user_avatar_url", Json::fromJuceString(collab.userAvatarUrl)},
      {"role", Json::fromJuceString(collab.role)},
      {"added_at", collab.addedAt.toISO8601(true).toStdString()},
  };
}

inline void from_json(const nlohmann::json &j, PlaylistCollaborator &collab) {
  JSON_OPTIONAL_STRING(j, "id", collab.id, "");
  JSON_OPTIONAL_STRING(j, "playlist_id", collab.playlistId, "");
  JSON_OPTIONAL_STRING(j, "user_id", collab.userId, "");
  JSON_OPTIONAL_STRING(j, "username", collab.username, "");
  JSON_OPTIONAL_STRING(j, "user_avatar_url", collab.userAvatarUrl, "");
  JSON_OPTIONAL_STRING(j, "role", collab.role, "");

  // Parse timestamp
  if (j.contains("added_at") && !j["added_at"].is_null()) {
    try {
      collab.addedAt = juce::Time::fromISO8601(Json::toJuceString(j["added_at"].get<std::string>()));
    } catch (...) {
      // Invalid timestamp format
    }
  }
}

} // namespace Sidechain
