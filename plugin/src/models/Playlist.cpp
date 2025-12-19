#include "Playlist.h"
#include "../util/Json.h"

// ==============================================================================
Playlist Playlist::fromJSON(const juce::var &json) {
  Playlist playlist;

  playlist.id = Json::getString(json, "id");
  playlist.name = Json::getString(json, "name");
  playlist.description = Json::getString(json, "description");
  playlist.ownerId = Json::getString(json, "owner_id");
  playlist.isCollaborative = Json::getBool(json, "is_collaborative");
  playlist.isPublic = Json::getBool(json, "is_public");

  // Parse owner info if present
  if (json.hasProperty("owner")) {
    auto owner = json["owner"];
    playlist.ownerUsername = Json::getString(owner, "username");
    playlist.ownerAvatarUrl = Json::getString(owner, "avatar_url");
  }

  // Parse entry count
  if (json.hasProperty("entries")) {
    auto entries = json["entries"];
    if (entries.isArray())
      playlist.entryCount = entries.size();
  } else {
    playlist.entryCount = Json::getInt(json, "entry_count");
  }

  // Parse user role if present
  playlist.userRole = Json::getString(json, "user_role");

  // Parse timestamp
  juce::String timeStr = Json::getString(json, "created_at");
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

  entry.id = Json::getString(json, "id");
  entry.playlistId = Json::getString(json, "playlist_id");
  entry.postId = Json::getString(json, "post_id");
  entry.addedByUserId = Json::getString(json, "added_by_user_id");
  entry.position = Json::getInt(json, "position");

  // Parse added by user info
  if (json.hasProperty("added_by_user")) {
    auto user = json["added_by_user"];
    entry.addedByUsername = Json::getString(user, "username");
  }

  // Parse post data if present
  if (json.hasProperty("post")) {
    auto post = json["post"];
    entry.postAudioUrl = Json::getString(post, "audio_url");
    entry.postBpm = Json::getInt(post, "bpm");
    entry.postKey = Json::getString(post, "key");

    // Parse genres
    auto genres = Json::getArray(post, "genre");
    if (Json::isArray(genres)) {
      for (int i = 0; i < Json::arraySize(genres); ++i)
        entry.postGenres.add(Json::getStringAt(genres, i));
    }

    // Parse post user
    if (post.hasProperty("user")) {
      entry.postUsername = Json::getString(post["user"], "username");
    }
  }

  // Parse timestamp
  juce::String timeStr = Json::getString(json, "added_at");
  if (timeStr.isNotEmpty()) {
    entry.addedAt = juce::Time::fromISO8601(timeStr);
  }

  return entry;
}

// ==============================================================================
PlaylistCollaborator PlaylistCollaborator::fromJSON(const juce::var &json) {
  PlaylistCollaborator collab;

  collab.id = Json::getString(json, "id");
  collab.playlistId = Json::getString(json, "playlist_id");
  collab.userId = Json::getString(json, "user_id");
  collab.role = Json::getString(json, "role");

  // Parse user info if present
  if (json.hasProperty("user")) {
    auto user = json["user"];
    collab.username = Json::getString(user, "username");
    collab.userAvatarUrl = Json::getString(user, "avatar_url");
  }

  // Parse timestamp
  juce::String timeStr = Json::getString(json, "added_at");
  if (timeStr.isNotEmpty()) {
    collab.addedAt = juce::Time::fromISO8601(timeStr);
  }

  return collab;
}
