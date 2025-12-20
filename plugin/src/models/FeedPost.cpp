#include "FeedPost.h"
#include "../util/Json.h"
#include "../util/Log.h"
#include "../util/Time.h"

namespace Sidechain {

// ==============================================================================
/** Create a FeedPost from JSON data
 * Parses getstream.io activity JSON into a FeedPost object.
 * Missing fields will be left as default values.
 * @param json JSON var containing post data from getstream.io
 * @return FeedPost instance (may have empty fields if JSON is incomplete)
 */
FeedPost FeedPost::fromJson(const juce::var &json) {
  try {
    // Convert juce::var to nlohmann::json
    auto jsonStr = juce::JSON::toString(json);
    auto jsonObj = nlohmann::json::parse(jsonStr.toStdString());

    // Use new SerializableModel API
    auto result = Sidechain::SerializableModel<FeedPost>::createFromJson(jsonObj);
    if (result.isOk()) {
      return *result.getValue();
    } else {
      // Return empty post on error
      Log::debug("FeedPost::fromJson: " + result.getError());
      return FeedPost();
    }
  } catch (const std::exception &e) {
    Log::debug("FeedPost::fromJson exception: " + juce::String(e.what()));
    return FeedPost();
  }
}

// ==============================================================================
/** Convert FeedPost to JSON for caching
 * Serializes the post data to JSON format compatible with fromJson().
 * @return JSON var representation of this post
 */
juce::var FeedPost::toJson() const {
  try {
    // Use new SerializableModel API
    auto result = Sidechain::SerializableModel<FeedPost>::toJson(std::make_shared<FeedPost>(*this));
    if (result.isOk()) {
      // Convert nlohmann::json back to juce::var
      auto jsonStr = result.getValue().dump();
      return juce::JSON::parse(jsonStr);
    } else {
      Log::debug("FeedPost::toJson: " + result.getError());
      return juce::var();
    }
  } catch (const std::exception &e) {
    Log::debug("FeedPost::toJson exception: " + juce::String(e.what()));
    return juce::var();
  }
}

// ==============================================================================
/** Extract user ID from actor string
 * Parses getstream.io actor format (e.g., "user:12345" or "SU:user:12345")
 * @param actorString Actor string in format "user:ID" or "SU:user:ID"
 * @return Extracted user ID, or empty string if format is invalid
 */
juce::String FeedPost::extractUserId(const juce::String &actorString) {
  // Actor format: "user:12345" or "SU:user:12345" (Stream User prefix)
  if (actorString.isEmpty())
    return {};

  // Handle "SU:user:id" format
  if (actorString.startsWith("SU:")) {
    auto withoutPrefix = actorString.substring(3);
    if (withoutPrefix.startsWith("user:"))
      return withoutPrefix.substring(5);
    return withoutPrefix;
  }

  // Handle "user:id" format
  if (actorString.startsWith("user:"))
    return actorString.substring(5);

  // If no prefix, assume it's just the ID
  return actorString;
}

// ==============================================================================
/** Format timestamp as "time ago" string
 * @deprecated Use TimeUtils::formatTimeAgo() instead
 * @param time Timestamp to format
 * @return Human-readable time string (e.g., "2h ago")
 */
juce::String FeedPost::formatTimeAgo(const juce::Time &time) {
  // Delegate to TimeUtils
  return TimeUtils::formatTimeAgo(time);
}

// ==============================================================================
/** Type-safe parsing with validation
 * Parses JSON and validates required fields. Returns error if validation fails.
 * @param json JSON var containing post data
 * @return Outcome with FeedPost if valid, or error message if invalid
 */
Outcome<FeedPost> FeedPost::tryFromJson(const juce::var &json) {
  // Validate input
  if (!::Json::isObject(json))
    return Outcome<FeedPost>::error("Invalid JSON: expected object");

  // Parse using existing method
  FeedPost post = fromJson(json);

  // Validate required fields
  if (post.id.isEmpty())
    return Outcome<FeedPost>::error("Missing required field: id");

  if (post.audioUrl.isEmpty())
    return Outcome<FeedPost>::error("Missing required field: audio_url");

  if (post.actor.isEmpty())
    return Outcome<FeedPost>::error("Missing required field: actor");

  // Log successful parse at debug level
  Log::debug("Parsed FeedPost: " + post.id + " by " + post.username);

  return Outcome<FeedPost>::ok(std::move(post));
}

// ==============================================================================
/** Check if post is valid (has required fields)
 * A valid post must have at least an ID and audio URL to be playable.
 * @return true if post has all required fields, false otherwise
 */
bool FeedPost::isValid() const {
  // A post must have at least an ID and an audio URL to be playable
  return id.isNotEmpty() && audioUrl.isNotEmpty();
}

// ==============================================================================
// nlohmann::json serialization for use with SerializableModel<FeedPost>

void to_json(nlohmann::json &j, const FeedPost &post) {
  j = nlohmann::json{
      {"id", Json::fromJuceString(post.id)},
      {"foreign_id", Json::fromJuceString(post.foreignId)},
      {"actor", Json::fromJuceString(post.actor)},
      {"verb", Json::fromJuceString(post.verb)},
      {"object", Json::fromJuceString(post.object)},
      {"time", post.timestamp.toISO8601(true).toStdString()},
      {"audio_url", Json::fromJuceString(post.audioUrl)},
      {"waveform_url", Json::fromJuceString(post.waveformUrl)},
      {"filename", Json::fromJuceString(post.filename)},
      {"duration_seconds", post.durationSeconds},
      {"duration_bars", post.durationBars},
      {"bpm", post.bpm},
      {"key", Json::fromJuceString(post.key)},
      {"daw", Json::fromJuceString(post.daw)},
      {"has_midi", post.hasMidi},
      {"midi_pattern_id", Json::fromJuceString(post.midiId)},
      {"midi_filename", Json::fromJuceString(post.midiFilename)},
      {"has_project_file", post.hasProjectFile},
      {"project_file_id", Json::fromJuceString(post.projectFileId)},
      {"project_file_daw", Json::fromJuceString(post.projectFileDaw)},
      {"is_remix", post.isRemix},
      {"remix_of_post_id", Json::fromJuceString(post.remixOfPostId)},
      {"remix_of_story_id", Json::fromJuceString(post.remixOfStoryId)},
      {"remix_type", Json::fromJuceString(post.remixType)},
      {"remix_chain_depth", post.remixChainDepth},
      {"remix_count", post.remixCount},
      {"sound_id", Json::fromJuceString(post.soundId)},
      {"sound_name", Json::fromJuceString(post.soundName)},
      {"sound_usage_count", post.soundUsageCount},
      {"like_count", post.likeCount},
      {"play_count", post.playCount},
      {"comment_count", post.commentCount},
      {"save_count", post.saveCount},
      {"repost_count", post.repostCount},
      {"download_count", post.downloadCount},
      {"is_liked", post.isLiked},
      {"is_saved", post.isSaved},
      {"is_reposted", post.isReposted},
      {"is_following", post.isFollowing},
      {"is_own_post", post.isOwnPost},
      {"is_pinned", post.isPinned},
      {"pin_order", post.pinOrder},
      {"comment_audience", Json::fromJuceString(post.commentAudience)},
      {"is_a_repost", post.isARepost},
      {"original_post_id", Json::fromJuceString(post.originalPostId)},
      {"original_user_id", Json::fromJuceString(post.originalUserId)},
      {"original_username", Json::fromJuceString(post.originalUsername)},
      {"original_avatar_url", Json::fromJuceString(post.originalAvatarUrl)},
      {"original_filename", Json::fromJuceString(post.originalFilename)},
      {"repost_quote", Json::fromJuceString(post.repostQuote)},
      {"is_online", post.isOnline},
      {"is_in_studio", post.isInStudio},
      {"user_reaction", Json::fromJuceString(post.userReaction)},
      {"recommendation_reason", Json::fromJuceString(post.recommendationReason)},
      {"source", Json::fromJuceString(post.source)},
      {"score", post.score},
      {"is_recommended", post.isRecommended},
  };

  // Add userId and username
  j["user_id"] = Json::fromJuceString(post.userId);
  j["username"] = Json::fromJuceString(post.username);
  j["user_avatar_url"] = Json::fromJuceString(post.userAvatarUrl);

  // Add genres array
  std::vector<std::string> genresVec;
  for (const auto &genre : post.genres) {
    genresVec.push_back(Json::fromJuceString(genre));
  }
  j["genres"] = genresVec;

  // Add reaction counts
  nlohmann::json reactionCountsObj;
  for (const auto &[emoji, count] : post.reactionCounts) {
    reactionCountsObj[Json::fromJuceString(emoji)] = count;
  }
  j["reaction_counts"] = reactionCountsObj;

  // Add processing status
  std::string statusStr;
  switch (post.status) {
  case FeedPost::Status::Ready:
    statusStr = "ready";
    break;
  case FeedPost::Status::Processing:
    statusStr = "processing";
    break;
  case FeedPost::Status::Failed:
    statusStr = "failed";
    break;
  case FeedPost::Status::Unknown:
    statusStr = "unknown";
    break;
  }
  j["status"] = statusStr;
}

void from_json(const nlohmann::json &j, FeedPost &post) {
  // Core identifiers
  JSON_OPTIONAL_STRING(j, "id", post.id, "");
  JSON_OPTIONAL_STRING(j, "foreign_id", post.foreignId, "");
  JSON_OPTIONAL_STRING(j, "actor", post.actor, "");
  JSON_OPTIONAL_STRING(j, "verb", post.verb, "");
  JSON_OPTIONAL_STRING(j, "object", post.object, "");

  // Extract user ID from actor
  post.userId = FeedPost::extractUserId(post.actor);

  // Timestamps
  if (j.contains("time") && !j["time"].is_null()) {
    try {
      post.timestamp = juce::Time::fromISO8601(Json::toJuceString(j["time"].get<std::string>()));
      post.timeAgo = TimeUtils::formatTimeAgo(post.timestamp);
    } catch (...) {
      // Invalid timestamp format
    }
  }

  // User info
  JSON_OPTIONAL_STRING(j, "user_id", post.userId, "");
  JSON_OPTIONAL_STRING(j, "username", post.username, "");
  JSON_OPTIONAL_STRING(j, "user_avatar_url", post.userAvatarUrl, "");

  // Audio metadata
  JSON_OPTIONAL_STRING(j, "audio_url", post.audioUrl, "");
  JSON_OPTIONAL_STRING(j, "waveform", post.waveformSvg, "");
  JSON_OPTIONAL_STRING(j, "waveform_url", post.waveformUrl, "");
  JSON_OPTIONAL_STRING(j, "filename", post.filename, "");
  JSON_OPTIONAL(j, "duration_seconds", post.durationSeconds, 0.0f);
  JSON_OPTIONAL(j, "duration_bars", post.durationBars, 0);
  JSON_OPTIONAL(j, "bpm", post.bpm, 0);
  JSON_OPTIONAL_STRING(j, "key", post.key, "");
  JSON_OPTIONAL_STRING(j, "daw", post.daw, "");

  // MIDI metadata
  JSON_OPTIONAL(j, "has_midi", post.hasMidi, false);
  JSON_OPTIONAL_STRING(j, "midi_pattern_id", post.midiId, "");
  JSON_OPTIONAL_STRING(j, "midi_id", post.midiId, "");
  JSON_OPTIONAL_STRING(j, "midi_filename", post.midiFilename, "");
  if (post.midiId.isNotEmpty())
    post.hasMidi = true;

  // Project file metadata
  JSON_OPTIONAL(j, "has_project_file", post.hasProjectFile, false);
  JSON_OPTIONAL_STRING(j, "project_file_id", post.projectFileId, "");
  JSON_OPTIONAL_STRING(j, "project_file_daw", post.projectFileDaw, "");
  if (post.projectFileId.isNotEmpty())
    post.hasProjectFile = true;

  // Remix metadata
  JSON_OPTIONAL_STRING(j, "remix_of_post_id", post.remixOfPostId, "");
  JSON_OPTIONAL_STRING(j, "remix_of_story_id", post.remixOfStoryId, "");
  JSON_OPTIONAL_STRING(j, "remix_type", post.remixType, "");
  JSON_OPTIONAL(j, "remix_chain_depth", post.remixChainDepth, 0);
  JSON_OPTIONAL(j, "remix_count", post.remixCount, 0);
  post.isRemix = post.remixOfPostId.isNotEmpty() || post.remixOfStoryId.isNotEmpty();

  // Sound/Sample metadata
  JSON_OPTIONAL_STRING(j, "sound_id", post.soundId, "");
  JSON_OPTIONAL_STRING(j, "sound_name", post.soundName, "");
  JSON_OPTIONAL(j, "sound_usage_count", post.soundUsageCount, 0);

  // Genres array
  if (j.contains("genres") && j["genres"].is_array()) {
    for (const auto &genreJson : j["genres"]) {
      if (genreJson.is_string()) {
        post.genres.add(Json::toJuceString(genreJson.get<std::string>()));
      }
    }
  } else if (j.contains("genre")) {
    // Single genre as string
    JSON_OPTIONAL_STRING(j, "genre", post.genres, "");
    if (post.genres.size() == 0 && j["genre"].is_string()) {
      post.genres.add(Json::toJuceString(j["genre"].get<std::string>()));
    }
  }

  // Social metrics
  JSON_OPTIONAL(j, "like_count", post.likeCount, 0);
  JSON_OPTIONAL(j, "play_count", post.playCount, 0);
  JSON_OPTIONAL(j, "comment_count", post.commentCount, 0);
  JSON_OPTIONAL(j, "save_count", post.saveCount, 0);
  JSON_OPTIONAL(j, "repost_count", post.repostCount, 0);
  JSON_OPTIONAL(j, "download_count", post.downloadCount, 0);
  JSON_OPTIONAL(j, "is_liked", post.isLiked, false);
  JSON_OPTIONAL(j, "is_saved", post.isSaved, false);
  JSON_OPTIONAL(j, "is_reposted", post.isReposted, false);
  JSON_OPTIONAL(j, "is_following", post.isFollowing, false);
  JSON_OPTIONAL(j, "is_own_post", post.isOwnPost, false);

  // Pin metadata
  JSON_OPTIONAL(j, "is_pinned", post.isPinned, false);
  JSON_OPTIONAL(j, "pin_order", post.pinOrder, 0);

  // Comment audience
  JSON_OPTIONAL_STRING(j, "comment_audience", post.commentAudience, "everyone");

  // Repost metadata
  JSON_OPTIONAL(j, "is_a_repost", post.isARepost, false);
  JSON_OPTIONAL_STRING(j, "original_post_id", post.originalPostId, "");
  JSON_OPTIONAL_STRING(j, "original_user_id", post.originalUserId, "");
  JSON_OPTIONAL_STRING(j, "original_username", post.originalUsername, "");
  JSON_OPTIONAL_STRING(j, "original_avatar_url", post.originalAvatarUrl, "");
  JSON_OPTIONAL_STRING(j, "original_filename", post.originalFilename, "");
  JSON_OPTIONAL_STRING(j, "repost_quote", post.repostQuote, "");

  // Online status
  JSON_OPTIONAL(j, "is_online", post.isOnline, false);
  JSON_OPTIONAL(j, "is_in_studio", post.isInStudio, false);

  // Reaction counts
  if (j.contains("reaction_counts") && j["reaction_counts"].is_object()) {
    for (auto it = j["reaction_counts"].begin(); it != j["reaction_counts"].end(); ++it) {
      if (it.value().is_number_integer()) {
        post.reactionCounts[Json::toJuceString(it.key())] = it.value().get<int>();
      }
    }
  }

  // User reaction
  JSON_OPTIONAL_STRING(j, "user_reaction", post.userReaction, "");

  // Recommendation metadata
  JSON_OPTIONAL_STRING(j, "recommendation_reason", post.recommendationReason, "");
  JSON_OPTIONAL_STRING(j, "source", post.source, "");
  JSON_OPTIONAL(j, "score", post.score, 0.0f);
  JSON_OPTIONAL(j, "is_recommended", post.isRecommended, false);

  // Processing status
  if (j.contains("status") && j["status"].is_string()) {
    juce::String statusStr = Json::toJuceString(j["status"].get<std::string>()).toLowerCase();
    if (statusStr == "ready")
      post.status = FeedPost::Status::Ready;
    else if (statusStr == "processing")
      post.status = FeedPost::Status::Processing;
    else if (statusStr == "failed")
      post.status = FeedPost::Status::Failed;
    else
      post.status = FeedPost::Status::Unknown;
  }
}

} // namespace Sidechain
