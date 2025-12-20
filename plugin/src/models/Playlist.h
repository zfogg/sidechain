#pragma once

#include "../util/json/JsonValidation.h"
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
};

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
