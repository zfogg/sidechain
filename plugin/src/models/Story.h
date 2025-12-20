#pragma once

#include "../util/SerializableModel.h"
#include "../util/json/JsonValidation.h"
#include <JuceHeader.h>
#include <nlohmann/json.hpp>

namespace Sidechain {

// ==============================================================================
/**
 * Story represents a short music clip with MIDI visualization
 *
 * Stories are 5-60 second audio clips captured from the DAW, optionally
 * including MIDI data for piano roll visualization. They expire after 24 hours.
 */
struct Story : public SerializableModel<Story> {
  juce::String id;
  juce::String userId;
  juce::String audioUrl;
  float audioDuration = 0.0f; // Duration in seconds (5-60)
  juce::String filename;      // Display filename for audio
  juce::String midiFilename;  // Display filename for MIDI
  juce::var midiData;         // MIDI events for visualization (embedded)
  juce::String midiPatternId; // ID of standalone MIDI pattern (for download)
  juce::String waveformData;  // SVG waveform (legacy, deprecated)
  juce::String waveformUrl;   // CDN URL to waveform PNG image
  int bpm = 0;
  juce::String key;
  juce::StringArray genres;
  int viewCount = 0;
  bool viewed = false; // Has current user viewed this story
  juce::Time createdAt;
  juce::Time expiresAt;

  // Associated user info (from API response)
  juce::String username;
  juce::String userDisplayName;
  juce::String userAvatarUrl;

  // ==============================================================================
  // Helper methods

  /** Check if story is expired (past expiration time)
   *  @return true if story has expired, false otherwise
   */
  bool isExpired() const;

  /** Get remaining time until expiration as human-readable string
   *  @return Formatted string (e.g., "5h remaining" or "Expired")
   */
  juce::String getExpirationText() const;

  /** Check if story has MIDI data for visualization
   *  @return true if MIDI data is present, false otherwise
   */
  bool hasMIDI() const;

  /** Check if story has downloadable MIDI pattern
   *  @return true if MIDI pattern ID is present, false otherwise
   */
  bool hasDownloadableMIDI() const {
    return midiPatternId.isNotEmpty();
  }

  /** Validation
   *  @return true if story has required fields (id and audioUrl)
   */
  bool isValid() const {
    return id.isNotEmpty() && audioUrl.isNotEmpty();
  }

  /** Parse from JSON response
   *  @deprecated Use SerializableModel<Story>::createFromJson() with nlohmann::json instead
   *  @param json JSON var containing story data
   *  @return Story instance parsed from JSON
   */
  [[deprecated("Use SerializableModel<Story>::createFromJson() with nlohmann::json instead")]]
  static Story fromJSON(const juce::var &json);

  /** Convert to JSON for upload
   *  @deprecated Use SerializableModel<Story>::toJson() with nlohmann::json instead
   *  @return JSON var representation of this story
   */
  [[deprecated("Use SerializableModel<Story>::toJson() with nlohmann::json instead")]]
  juce::var toJSON() const;
};

// ==============================================================================
/**
 * StoryHighlight represents a collection of saved stories that persist beyond
 * 24 hours
 *
 * Like Instagram Highlights, these allow users to save and organize their
 * best stories for permanent display on their profile.
 */
struct StoryHighlight {
  juce::String id;
  juce::String userId;
  juce::String name;          // Display name (e.g., "Jams", "Experiments")
  juce::String coverImageUrl; // Optional custom cover image
  juce::String description;   // Optional description
  int sortOrder = 0;          // Order on profile
  int storyCount = 0;         // Number of stories in this highlight
  juce::Time createdAt;
  juce::Time updatedAt;

  // Stories in this highlight (populated when fetching single highlight)
  juce::Array<Story> stories;

  // ==============================================================================
  // Helper methods

  /** Check if highlight has a custom cover image
   *  @return true if cover image URL is set
   */
  bool hasCoverImage() const {
    return coverImageUrl.isNotEmpty();
  }

  /** Get the cover image URL, or first story's waveform as fallback
   *  @return URL for cover image display
   */
  juce::String getCoverUrl() const;

  /** Parse from JSON response
   *  @param json JSON var containing highlight data
   *  @return StoryHighlight instance parsed from JSON
   */
  static StoryHighlight fromJSON(const juce::var &json);

  /** Convert to JSON for creation/update
   *  @return JSON var representation of this highlight
   */
  juce::var toJSON() const;
};

} // namespace Sidechain
