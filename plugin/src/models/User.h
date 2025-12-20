#pragma once

#include "../util/json/JsonValidation.h"
#include <JuceHeader.h>
#include <nlohmann/json.hpp>
#include <unordered_map>

namespace Sidechain {

// ==============================================================================
/**
 * SocialLinks - Typed structure for social media links
 */
struct SocialLinks {
  juce::String instagram;
  juce::String twitter;
  juce::String youtube;
  juce::String soundcloud;
  juce::String spotify;
  juce::String bandcamp;

  // Allow custom links beyond the standard platforms
  std::unordered_map<std::string, std::string> custom;

  bool isEmpty() const {
    return instagram.isEmpty() && twitter.isEmpty() && youtube.isEmpty() && soundcloud.isEmpty() && spotify.isEmpty() &&
           bandcamp.isEmpty() && custom.empty();
  }
};

// JSON serialization for SocialLinks
inline void to_json(nlohmann::json &j, const SocialLinks &links) {
  j = nlohmann::json::object();

  if (links.instagram.isNotEmpty())
    j["instagram"] = Json::fromJuceString(links.instagram);
  if (links.twitter.isNotEmpty())
    j["twitter"] = Json::fromJuceString(links.twitter);
  if (links.youtube.isNotEmpty())
    j["youtube"] = Json::fromJuceString(links.youtube);
  if (links.soundcloud.isNotEmpty())
    j["soundcloud"] = Json::fromJuceString(links.soundcloud);
  if (links.spotify.isNotEmpty())
    j["spotify"] = Json::fromJuceString(links.spotify);
  if (links.bandcamp.isNotEmpty())
    j["bandcamp"] = Json::fromJuceString(links.bandcamp);

  // Add custom links
  for (const auto &[key, value] : links.custom) {
    j[key] = value;
  }
}

inline void from_json(const nlohmann::json &j, SocialLinks &links) {
  if (!j.is_object())
    return;

  if (j.contains("instagram"))
    links.instagram = Json::toJuceString(j["instagram"].get<std::string>());
  if (j.contains("twitter"))
    links.twitter = Json::toJuceString(j["twitter"].get<std::string>());
  if (j.contains("youtube"))
    links.youtube = Json::toJuceString(j["youtube"].get<std::string>());
  if (j.contains("soundcloud"))
    links.soundcloud = Json::toJuceString(j["soundcloud"].get<std::string>());
  if (j.contains("spotify"))
    links.spotify = Json::toJuceString(j["spotify"].get<std::string>());
  if (j.contains("bandcamp"))
    links.bandcamp = Json::toJuceString(j["bandcamp"].get<std::string>());

  // Parse any additional custom links
  for (auto it = j.begin(); it != j.end(); ++it) {
    const auto &key = it.key();
    if (key != "instagram" && key != "twitter" && key != "youtube" && key != "soundcloud" && key != "spotify" &&
        key != "bandcamp") {
      if (it.value().is_string()) {
        links.custom[key] = it.value().get<std::string>();
      }
    }
  }
}

// ==============================================================================
/**
 * User - User profile entity
 *
 * Represents a user account with profile information, stats, and relationships.
 * Used for user profiles, search results, followers lists, etc.
 *
 * Features typed JSON with validation for safe parsing from API responses.
 */
struct User : public SerializableModel<User> {
  // ==============================================================================
  // Core identity
  juce::String id;
  juce::String username;
  juce::String displayName;
  juce::String bio;

  // ==============================================================================
  // Profile media
  juce::String avatarUrl;
  juce::String bannerUrl;

  // ==============================================================================
  // Profile metadata
  juce::String location;
  juce::String genre;      // Primary genre
  juce::String daw;        // DAW preference (e.g., "Ableton Live", "FL Studio")
  juce::String website;    // Personal website URL
  SocialLinks socialLinks; // Social media links (Instagram, Twitter, etc.)

  // ==============================================================================
  // Stats
  int followerCount = 0;
  int followingCount = 0;
  int postCount = 0;
  int likeCount = 0; // Total likes received across all posts

  // ==============================================================================
  // Account status
  bool isPrivate = false;  // Private account (requires approval to follow)
  bool isVerified = false; // Verified artist/producer badge
  bool isOnline = false;   // Currently online
  bool isInStudio = false; // Custom status: "in studio"

  // ==============================================================================
  // Relationships (current user's relationship with this user)
  bool isFollowing = false; // Current user follows this user
  bool followsYou = false;  // This user follows current user
  bool isBlocked = false;   // Current user has blocked this user
  bool isMuted = false;     // Current user has muted this user

  // ==============================================================================
  // Timestamps
  juce::Time createdAt;
  juce::Time lastActive;

  // ==============================================================================
  // Typed JSON serialization
  SIDECHAIN_JSON_TYPE(User)

  // ==============================================================================
  // Validation
  bool isValid() const {
    return id.isNotEmpty() && username.isNotEmpty();
  }

  juce::String getId() const {
    return id;
  }

  // ==============================================================================
  // Display helpers
  juce::String getDisplayName() const {
    return displayName.isNotEmpty() ? displayName : username;
  }

  juce::String getFormattedFollowerCount() const {
    if (followerCount >= 1000000)
      return juce::String(followerCount / 1000000.0, 1) + "M";
    if (followerCount >= 1000)
      return juce::String(followerCount / 1000.0, 1) + "K";
    return juce::String(followerCount);
  }

  juce::String getFormattedFollowingCount() const {
    if (followingCount >= 1000000)
      return juce::String(followingCount / 1000000.0, 1) + "M";
    if (followingCount >= 1000)
      return juce::String(followingCount / 1000.0, 1) + "K";
    return juce::String(followingCount);
  }
};

// ==============================================================================
// JSON Serialization Implementation

inline void to_json(nlohmann::json &j, const User &user) {
  j = nlohmann::json{
      {"id", Json::fromJuceString(user.id)},
      {"username", Json::fromJuceString(user.username)},
      {"display_name", Json::fromJuceString(user.displayName)},
      {"bio", Json::fromJuceString(user.bio)},
      {"avatar_url", Json::fromJuceString(user.avatarUrl)},
      {"banner_url", Json::fromJuceString(user.bannerUrl)},
      {"location", Json::fromJuceString(user.location)},
      {"genre", Json::fromJuceString(user.genre)},
      {"daw", Json::fromJuceString(user.daw)},
      {"website", Json::fromJuceString(user.website)},
      {"follower_count", user.followerCount},
      {"following_count", user.followingCount},
      {"post_count", user.postCount},
      {"like_count", user.likeCount},
      {"is_private", user.isPrivate},
      {"is_verified", user.isVerified},
      {"is_online", user.isOnline},
      {"is_in_studio", user.isInStudio},
      {"is_following", user.isFollowing},
      {"follows_you", user.followsYou},
      {"is_blocked", user.isBlocked},
      {"is_muted", user.isMuted},
      {"created_at", user.createdAt.toISO8601(true).toStdString()},
      {"last_active", user.lastActive.toISO8601(true).toStdString()},
  };

  // Add social links if present
  if (!user.socialLinks.isEmpty()) {
    j["social_links"] = user.socialLinks;
  }
}

inline void from_json(const nlohmann::json &j, User &user) {
  // Required fields with validation
  JSON_REQUIRE_STRING(j, "id", user.id);
  JSON_REQUIRE_STRING(j, "username", user.username);

  // Optional fields with defaults
  JSON_OPTIONAL_STRING(j, "display_name", user.displayName, "");
  JSON_OPTIONAL_STRING(j, "bio", user.bio, "");
  JSON_OPTIONAL_STRING(j, "avatar_url", user.avatarUrl, "");
  JSON_OPTIONAL_STRING(j, "banner_url", user.bannerUrl, "");
  JSON_OPTIONAL_STRING(j, "location", user.location, "");
  JSON_OPTIONAL_STRING(j, "genre", user.genre, "");
  JSON_OPTIONAL_STRING(j, "daw", user.daw, "");
  JSON_OPTIONAL_STRING(j, "website", user.website, "");

  JSON_OPTIONAL(j, "follower_count", user.followerCount, 0);
  JSON_OPTIONAL(j, "following_count", user.followingCount, 0);
  JSON_OPTIONAL(j, "post_count", user.postCount, 0);
  JSON_OPTIONAL(j, "like_count", user.likeCount, 0);

  JSON_OPTIONAL(j, "is_private", user.isPrivate, false);
  JSON_OPTIONAL(j, "is_verified", user.isVerified, false);
  JSON_OPTIONAL(j, "is_online", user.isOnline, false);
  JSON_OPTIONAL(j, "is_in_studio", user.isInStudio, false);

  JSON_OPTIONAL(j, "is_following", user.isFollowing, false);
  JSON_OPTIONAL(j, "follows_you", user.followsYou, false);
  JSON_OPTIONAL(j, "is_blocked", user.isBlocked, false);
  JSON_OPTIONAL(j, "is_muted", user.isMuted, false);

  // Parse timestamps
  if (j.contains("created_at") && !j["created_at"].is_null()) {
    user.createdAt = juce::Time::fromISO8601(Json::toJuceString(j["created_at"].get<std::string>()));
  }
  if (j.contains("last_active") && !j["last_active"].is_null()) {
    user.lastActive = juce::Time::fromISO8601(Json::toJuceString(j["last_active"].get<std::string>()));
  }

  // Parse social links
  if (j.contains("social_links") && j["social_links"].is_object()) {
    from_json(j["social_links"], user.socialLinks);
  }
}

} // namespace Sidechain
