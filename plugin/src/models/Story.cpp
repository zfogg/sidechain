#include "Story.h"

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
  Story story;

  story.id = json["id"].toString();
  story.userId = json["user_id"].toString();
  story.audioUrl = json["audio_url"].toString();
  story.audioDuration = static_cast<float>(json["audio_duration"]);
  story.filename = json["filename"].toString();
  story.midiFilename = json["midi_filename"].toString();
  story.midiData = json["midi_data"];
  story.midiPatternId = json["midi_pattern_id"].toString();
  story.waveformData = json["waveform_data"].toString();
  story.waveformUrl = json["waveform_url"].toString();
  story.bpm = static_cast<int>(json["bpm"]);
  story.key = json["key"].toString();
  story.viewCount = static_cast<int>(json["view_count"]);
  story.viewed = static_cast<bool>(json["viewed"]);

  // Parse genres array
  if (json.hasProperty("genres")) {
    auto *genresArray = json["genres"].getArray();
    if (genresArray) {
      for (const auto &genre : *genresArray) {
        story.genres.add(genre.toString());
      }
    }
  }

  // Parse user info if present
  if (json.hasProperty("user")) {
    const auto &user = json["user"];
    story.username = user["username"].toString();
    story.userDisplayName = user["display_name"].toString();
    story.userAvatarUrl = user["avatar_url"].toString();
  }

  // Parse timestamps (ISO 8601 format)
  // Simplified parsing - in production would use proper date parser
  if (json.hasProperty("created_at")) {
    // For now, set to current time
    story.createdAt = juce::Time::getCurrentTime();
  }

  if (json.hasProperty("expires_at")) {
    // Default to 24 hours from creation
    story.expiresAt = story.createdAt + juce::RelativeTime::hours(24);
  }

  return story;
}

/** Convert Story to JSON for upload
 * Serializes story data to JSON format for API upload.
 * Only includes fields needed for story creation.
 * @return JSON var representation of this story
 */
juce::var Story::toJSON() const {
  auto *obj = new juce::DynamicObject();
  juce::var json(obj);

  obj->setProperty("audio_url", audioUrl);
  obj->setProperty("audio_duration", audioDuration);

  if (filename.isNotEmpty())
    obj->setProperty("filename", filename);

  if (midiFilename.isNotEmpty())
    obj->setProperty("midi_filename", midiFilename);

  if (midiData.isObject()) {
    obj->setProperty("midi_data", midiData);
  }

  if (bpm > 0) {
    obj->setProperty("bpm", bpm);
  }

  if (key.isNotEmpty()) {
    obj->setProperty("key", key);
  }

  if (!genres.isEmpty()) {
    juce::var genresArray = juce::var(juce::Array<juce::var>());
    for (const auto &genre : genres) {
      genresArray.getArray()->add(genre);
    }
    obj->setProperty("genres", genresArray);
  }

  return json;
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
        // Handle nested story structure from highlighted_stories join
        if (storyJson.hasProperty("story")) {
          highlight.stories.add(Story::fromJSON(storyJson["story"]));
        } else {
          highlight.stories.add(Story::fromJSON(storyJson));
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
