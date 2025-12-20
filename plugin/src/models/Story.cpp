#include "Story.h"
#include "../util/Log.h"
#include "../util/Json.h"

namespace Sidechain {

// ==============================================================================
/** Check if story is expired (past expiration time)
 * Stories expire 24 hours after creation.
 * @return true if story has expired, false otherwise
 */
bool Story::isExpired() const {
  return juce::Time::getCurrentTime() > expiresAt;
}

/** Get remaining time until expiration as human-readable string
 * @return Formatted string (e.g., "5h left", "30m left", "< 1m left", or
 * "Expired")
 */
juce::String Story::getExpirationText() const {
  auto now = juce::Time::getCurrentTime();

  if (expiresAt < now)
    return "Expired";

  auto diff = expiresAt - now;
  int hours = static_cast<int>(diff.inHours());

  if (hours < 1) {
    int minutes = static_cast<int>(diff.inMinutes());
    if (minutes < 1)
      return "< 1m left";
    return juce::String(minutes) + "m left";
  }

  return juce::String(hours) + "h left";
}

/** Check if story has MIDI data for visualization
 * @return true if MIDI data is present and contains events, false otherwise
 */
bool Story::hasMIDI() const {
  if (!midiData.isObject())
    return false;

  // Check if there are any events
  if (midiData.hasProperty("events")) {
    auto *events = midiData["events"].getArray();
    return events && !events->isEmpty();
  }

  return false;
}

/** Parse Story from JSON response
 * Creates a Story instance from backend API JSON data.
 * @param json JSON var containing story data from API
 * @return Story instance parsed from JSON
 */
Story Story::fromJSON(const juce::var &json) {
  try {
    // Convert juce::var to nlohmann::json
    auto jsonStr = juce::JSON::toString(json);
    auto jsonObj = nlohmann::json::parse(jsonStr.toStdString());

    // Use new SerializableModel API
    auto result = Sidechain::SerializableModel<Story>::createFromJson(jsonObj);
    if (result.isOk()) {
      return *result.getValue();
    } else {
      // Return empty story on error
      Log::debug("Story::fromJSON: " + result.getError());
      return Story();
    }
  } catch (const std::exception &e) {
    Log::debug("Story::fromJSON exception: " + juce::String(e.what()));
    return Story();
  }
}

/** Convert Story to JSON for upload
 * Serializes story data to JSON format for API upload.
 * Only includes fields needed for story creation.
 * @return JSON var representation of this story
 */
juce::var Story::toJSON() const {
  try {
    // Use new SerializableModel API
    auto result = Sidechain::SerializableModel<Story>::toJson(std::make_shared<Story>(*this));
    if (result.isOk()) {
      // Convert nlohmann::json back to juce::var
      auto jsonStr = result.getValue().dump();
      return juce::JSON::parse(jsonStr);
    } else {
      Log::debug("Story::toJSON: " + result.getError());
      return juce::var();
    }
  } catch (const std::exception &e) {
    Log::debug("Story::toJSON exception: " + juce::String(e.what()));
    return juce::var();
  }
}

// ==============================================================================
// StoryHighlight implementation

/** Get the cover image URL, or first story's waveform as fallback
 * @return URL for cover image display
 */
juce::String StoryHighlight::getCoverUrl() const {
  if (coverImageUrl.isNotEmpty())
    return coverImageUrl;

  // Use first story's audio URL as placeholder (UI can show waveform)
  if (!stories.isEmpty())
    return stories[0].audioUrl;

  return {};
}

/** Parse StoryHighlight from JSON response
 * Creates a StoryHighlight instance from backend API JSON data.
 * @param json JSON var containing highlight data from API
 * @return StoryHighlight instance parsed from JSON
 */
StoryHighlight StoryHighlight::fromJSON(const juce::var &json) {
  StoryHighlight highlight;

  highlight.id = json["id"].toString();
  highlight.userId = json["user_id"].toString();
  highlight.name = json["name"].toString();
  highlight.coverImageUrl = json["cover_image"].toString();
  highlight.description = json["description"].toString();
  highlight.sortOrder = static_cast<int>(json["sort_order"]);
  highlight.storyCount = static_cast<int>(json["story_count"]);

  // Parse stories array if present (when fetching single highlight)
  if (json.hasProperty("stories")) {
    auto *storiesArray = json["stories"].getArray();
    if (storiesArray) {
      for (const auto &storyJson : *storiesArray) {
        try {
          // Convert juce::var to nlohmann::json for new API
          auto jsonStr = juce::JSON::toString(storyJson.hasProperty("story") ? storyJson["story"] : storyJson);
          auto jsonObj = nlohmann::json::parse(jsonStr.toStdString());

          // Use new SerializableModel API
          auto storyResult = Sidechain::SerializableModel<Sidechain::Story>::createFromJson(jsonObj);
          if (storyResult.isOk()) {
            highlight.stories.add(*storyResult.getValue());
          } else {
            Log::debug("Story: Failed to parse story in highlight: " + storyResult.getError());
          }
        } catch (const std::exception &e) {
          Log::debug("Story: Exception parsing story in highlight: " + juce::String(e.what()));
        }
      }
    }
  }

  // Parse timestamps
  highlight.createdAt = juce::Time::getCurrentTime();
  highlight.updatedAt = juce::Time::getCurrentTime();

  return highlight;
}

/** Convert StoryHighlight to JSON for creation/update
 * Serializes highlight data to JSON format for API requests.
 * @return JSON var representation of this highlight
 */
juce::var StoryHighlight::toJSON() const {
  auto *obj = new juce::DynamicObject();
  juce::var json(obj);

  obj->setProperty("name", name);

  if (description.isNotEmpty())
    obj->setProperty("description", description);

  if (coverImageUrl.isNotEmpty())
    obj->setProperty("cover_image", coverImageUrl);

  if (sortOrder > 0)
    obj->setProperty("sort_order", sortOrder);

  return json;
}

} // namespace Sidechain

namespace Sidechain {

// ==============================================================================
// nlohmann::json serialization for SerializableModel<Story>

void to_json(nlohmann::json &j, const Story &story) {
  j = nlohmann::json{
      {"id", Json::fromJuceString(story.id)},
      {"user_id", Json::fromJuceString(story.userId)},
      {"audio_url", Json::fromJuceString(story.audioUrl)},
      {"audio_duration", story.audioDuration},
      {"filename", Json::fromJuceString(story.filename)},
      {"midi_filename", Json::fromJuceString(story.midiFilename)},
      {"midi_pattern_id", Json::fromJuceString(story.midiPatternId)},
      {"waveform_data", Json::fromJuceString(story.waveformData)},
      {"waveform_url", Json::fromJuceString(story.waveformUrl)},
      {"bpm", story.bpm},
      {"key", Json::fromJuceString(story.key)},
      {"view_count", story.viewCount},
      {"viewed", story.viewed},
      {"created_at", story.createdAt.toISO8601(true).toStdString()},
      {"expires_at", story.expiresAt.toISO8601(true).toStdString()},
      {"username", Json::fromJuceString(story.username)},
      {"user_display_name", Json::fromJuceString(story.userDisplayName)},
      {"user_avatar_url", Json::fromJuceString(story.userAvatarUrl)},
  };

  // Add genres array
  std::vector<std::string> genresVec;
  for (const auto &genre : story.genres) {
    genresVec.push_back(Json::fromJuceString(genre));
  }
  j["genres"] = genresVec;
}

void from_json(const nlohmann::json &j, Story &story) {
  JSON_OPTIONAL_STRING(j, "id", story.id, "");
  JSON_OPTIONAL_STRING(j, "user_id", story.userId, "");
  JSON_OPTIONAL_STRING(j, "audio_url", story.audioUrl, "");
  JSON_OPTIONAL(j, "audio_duration", story.audioDuration, 0.0f);
  JSON_OPTIONAL_STRING(j, "filename", story.filename, "");
  JSON_OPTIONAL_STRING(j, "midi_filename", story.midiFilename, "");
  JSON_OPTIONAL_STRING(j, "midi_pattern_id", story.midiPatternId, "");
  JSON_OPTIONAL_STRING(j, "waveform_data", story.waveformData, "");
  JSON_OPTIONAL_STRING(j, "waveform_url", story.waveformUrl, "");
  JSON_OPTIONAL(j, "bpm", story.bpm, 0);
  JSON_OPTIONAL_STRING(j, "key", story.key, "");
  JSON_OPTIONAL(j, "view_count", story.viewCount, 0);
  JSON_OPTIONAL(j, "viewed", story.viewed, false);

  // Parse genres array
  if (j.contains("genres") && j["genres"].is_array()) {
    for (const auto &genreJson : j["genres"]) {
      if (genreJson.is_string()) {
        story.genres.add(Json::toJuceString(genreJson.get<std::string>()));
      }
    }
  }

  // Parse user info
  JSON_OPTIONAL_STRING(j, "username", story.username, "");
  JSON_OPTIONAL_STRING(j, "user_display_name", story.userDisplayName, "");
  JSON_OPTIONAL_STRING(j, "user_avatar_url", story.userAvatarUrl, "");

  // Parse timestamps
  if (j.contains("created_at") && !j["created_at"].is_null()) {
    try {
      story.createdAt = juce::Time::fromISO8601(Json::toJuceString(j["created_at"].get<std::string>()));
    } catch (...) {
      story.createdAt = juce::Time::getCurrentTime();
    }
  }

  if (j.contains("expires_at") && !j["expires_at"].is_null()) {
    try {
      story.expiresAt = juce::Time::fromISO8601(Json::toJuceString(j["expires_at"].get<std::string>()));
    } catch (...) {
      story.expiresAt = story.createdAt + juce::RelativeTime::hours(24);
    }
  } else {
    story.expiresAt = story.createdAt + juce::RelativeTime::hours(24);
  }
}

} // namespace Sidechain
