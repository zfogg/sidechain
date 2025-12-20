#pragma once

#include "../util/SerializableModel.h"
#include "../util/json/JsonValidation.h"
#include <JuceHeader.h>
#include <nlohmann/json.hpp>

namespace Sidechain {

// ==============================================================================
/**
 * MIDIChallengeConstraints represents the constraints for a MIDI challenge
 */
struct MIDIChallengeConstraints {
  int bpmMin = 0;           // 0 means no constraint
  int bpmMax = 0;           // 0 means no constraint
  juce::String key;         // Empty means no constraint
  juce::String scale;       // Empty means no constraint
  int noteCountMin = 0;     // 0 means no constraint
  int noteCountMax = 0;     // 0 means no constraint
  double durationMin = 0.0; // 0 means no constraint
  double durationMax = 0.0; // 0 means no constraint

  // Parse from JSON
  static MIDIChallengeConstraints fromJSON(const juce::var &json);
};

// ==============================================================================
/**
 * MIDIChallenge represents a MIDI challenge
 */
struct MIDIChallenge : public SerializableModel<MIDIChallenge> {
  juce::String id;
  juce::String title;
  juce::String description;
  MIDIChallengeConstraints constraints;
  juce::Time startDate;
  juce::Time endDate;
  juce::Time votingEndDate;
  juce::String status; // "upcoming", "active", "voting", "ended"
  int entryCount = 0;
  juce::Time createdAt;

  // Validation
  bool isValid() const {
    return id.isNotEmpty() && title.isNotEmpty();
  }

  // Parse from JSON response
  static MIDIChallenge fromJSON(const juce::var &json);

  // Check if challenge is currently accepting submissions
  bool isAcceptingSubmissions() const {
    auto now = juce::Time::getCurrentTime();
    return now >= startDate && now <= endDate;
  }

  // Check if challenge is in voting phase
  bool isVoting() const {
    auto now = juce::Time::getCurrentTime();
    return now > endDate && now <= votingEndDate;
  }

  // Check if challenge has ended
  bool hasEnded() const {
    auto now = juce::Time::getCurrentTime();
    return now > votingEndDate;
  }
};

// ==============================================================================
/**
 * MIDIChallengeEntry represents a user's submission to a MIDI challenge
 */
struct MIDIChallengeEntry : public SerializableModel<MIDIChallengeEntry> {
  juce::String id;
  juce::String challengeId;
  juce::String userId;
  juce::String username;
  juce::String userAvatarUrl;
  juce::String audioUrl;
  juce::String postId;        // Optional link to AudioPost
  juce::String midiPatternId; // Optional link to MIDIPattern
  int voteCount = 0;
  bool hasVoted = false; // Whether current user has voted for this entry
  juce::Time submittedAt;

  // Validation
  bool isValid() const {
    return id.isNotEmpty() && challengeId.isNotEmpty() && userId.isNotEmpty();
  }

  // Parse from JSON response
  static MIDIChallengeEntry fromJSON(const juce::var &json);
};

// ==============================================================================
// nlohmann::json serialization

inline void to_json(nlohmann::json &j, const MIDIChallengeConstraints &constraints) {
  j = nlohmann::json{
      {"bpm_min", constraints.bpmMin},
      {"bpm_max", constraints.bpmMax},
      {"key", Json::fromJuceString(constraints.key)},
      {"scale", Json::fromJuceString(constraints.scale)},
      {"note_count_min", constraints.noteCountMin},
      {"note_count_max", constraints.noteCountMax},
      {"duration_min", constraints.durationMin},
      {"duration_max", constraints.durationMax},
  };
}

inline void from_json(const nlohmann::json &j, MIDIChallengeConstraints &constraints) {
  JSON_OPTIONAL(j, "bpm_min", constraints.bpmMin, 0);
  JSON_OPTIONAL(j, "bpm_max", constraints.bpmMax, 0);
  JSON_OPTIONAL_STRING(j, "key", constraints.key, "");
  JSON_OPTIONAL_STRING(j, "scale", constraints.scale, "");
  JSON_OPTIONAL(j, "note_count_min", constraints.noteCountMin, 0);
  JSON_OPTIONAL(j, "note_count_max", constraints.noteCountMax, 0);
  JSON_OPTIONAL(j, "duration_min", constraints.durationMin, 0.0);
  JSON_OPTIONAL(j, "duration_max", constraints.durationMax, 0.0);
}

inline void to_json(nlohmann::json &j, const MIDIChallenge &challenge) {
  j = nlohmann::json{
      {"id", Json::fromJuceString(challenge.id)},
      {"title", Json::fromJuceString(challenge.title)},
      {"description", Json::fromJuceString(challenge.description)},
      {"status", Json::fromJuceString(challenge.status)},
      {"start_date", challenge.startDate.toISO8601(true).toStdString()},
      {"end_date", challenge.endDate.toISO8601(true).toStdString()},
      {"voting_end_date", challenge.votingEndDate.toISO8601(true).toStdString()},
      {"entry_count", challenge.entryCount},
      {"created_at", challenge.createdAt.toISO8601(true).toStdString()},
      {"constraints", challenge.constraints},
  };
}

inline void from_json(const nlohmann::json &j, MIDIChallenge &challenge) {
  JSON_OPTIONAL_STRING(j, "id", challenge.id, "");
  JSON_OPTIONAL_STRING(j, "title", challenge.title, "");
  JSON_OPTIONAL_STRING(j, "description", challenge.description, "");
  JSON_OPTIONAL_STRING(j, "status", challenge.status, "");
  JSON_OPTIONAL(j, "entry_count", challenge.entryCount, 0);

  // Parse dates
  if (j.contains("start_date") && !j["start_date"].is_null()) {
    try {
      challenge.startDate = juce::Time::fromISO8601(Json::toJuceString(j["start_date"].get<std::string>()));
    } catch (...) {
    }
  }

  if (j.contains("end_date") && !j["end_date"].is_null()) {
    try {
      challenge.endDate = juce::Time::fromISO8601(Json::toJuceString(j["end_date"].get<std::string>()));
    } catch (...) {
    }
  }

  if (j.contains("voting_end_date") && !j["voting_end_date"].is_null()) {
    try {
      challenge.votingEndDate = juce::Time::fromISO8601(Json::toJuceString(j["voting_end_date"].get<std::string>()));
    } catch (...) {
    }
  }

  if (j.contains("created_at") && !j["created_at"].is_null()) {
    try {
      challenge.createdAt = juce::Time::fromISO8601(Json::toJuceString(j["created_at"].get<std::string>()));
    } catch (...) {
    }
  }

  // Parse constraints
  if (j.contains("constraints") && j["constraints"].is_object()) {
    from_json(j["constraints"], challenge.constraints);
  }
}

inline void to_json(nlohmann::json &j, const MIDIChallengeEntry &entry) {
  j = nlohmann::json{
      {"id", Json::fromJuceString(entry.id)},
      {"challenge_id", Json::fromJuceString(entry.challengeId)},
      {"user_id", Json::fromJuceString(entry.userId)},
      {"username", Json::fromJuceString(entry.username)},
      {"user_avatar_url", Json::fromJuceString(entry.userAvatarUrl)},
      {"audio_url", Json::fromJuceString(entry.audioUrl)},
      {"post_id", Json::fromJuceString(entry.postId)},
      {"midi_pattern_id", Json::fromJuceString(entry.midiPatternId)},
      {"vote_count", entry.voteCount},
      {"has_voted", entry.hasVoted},
      {"submitted_at", entry.submittedAt.toISO8601(true).toStdString()},
  };
}

inline void from_json(const nlohmann::json &j, MIDIChallengeEntry &entry) {
  JSON_OPTIONAL_STRING(j, "id", entry.id, "");
  JSON_OPTIONAL_STRING(j, "challenge_id", entry.challengeId, "");
  JSON_OPTIONAL_STRING(j, "user_id", entry.userId, "");
  JSON_OPTIONAL_STRING(j, "username", entry.username, "");
  JSON_OPTIONAL_STRING(j, "user_avatar_url", entry.userAvatarUrl, "");
  JSON_OPTIONAL_STRING(j, "audio_url", entry.audioUrl, "");
  JSON_OPTIONAL_STRING(j, "post_id", entry.postId, "");
  JSON_OPTIONAL_STRING(j, "midi_pattern_id", entry.midiPatternId, "");
  JSON_OPTIONAL(j, "vote_count", entry.voteCount, 0);
  JSON_OPTIONAL(j, "has_voted", entry.hasVoted, false);

  // Parse timestamp
  if (j.contains("submitted_at") && !j["submitted_at"].is_null()) {
    try {
      entry.submittedAt = juce::Time::fromISO8601(Json::toJuceString(j["submitted_at"].get<std::string>()));
    } catch (...) {
    }
  }
}

} // namespace Sidechain
