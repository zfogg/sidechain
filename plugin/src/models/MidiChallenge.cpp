#include "MidiChallenge.h"
#include "../util/Json.h"

// ==============================================================================
MIDIChallengeConstraints MIDIChallengeConstraints::fromJSON(const juce::var &json) {
  MIDIChallengeConstraints constraints;

  if (json.hasProperty("bpm_min"))
    constraints.bpmMin = ::Json::getInt(json, "bpm_min");
  if (json.hasProperty("bpm_max"))
    constraints.bpmMax = ::Json::getInt(json, "bpm_max");
  constraints.key = ::Json::getString(json, "key");
  constraints.scale = ::Json::getString(json, "scale");
  if (json.hasProperty("note_count_min"))
    constraints.noteCountMin = ::Json::getInt(json, "note_count_min");
  if (json.hasProperty("note_count_max"))
    constraints.noteCountMax = ::Json::getInt(json, "note_count_max");
  if (json.hasProperty("duration_min"))
    constraints.durationMin = Json::getDouble(json, "duration_min");
  if (json.hasProperty("duration_max"))
    constraints.durationMax = Json::getDouble(json, "duration_max");

  return constraints;
}

// ==============================================================================
MIDIChallenge MIDIChallenge::fromJSON(const juce::var &json) {
  MIDIChallenge challenge;

  challenge.id = ::Json::getString(json, "id");
  challenge.title = ::Json::getString(json, "title");
  challenge.description = ::Json::getString(json, "description");
  challenge.status = ::Json::getString(json, "status");

  // Parse constraints
  if (json.hasProperty("constraints")) {
    challenge.constraints = MIDIChallengeConstraints::fromJSON(json["constraints"]);
  }

  // Parse dates
  juce::String startDateStr = ::Json::getString(json, "start_date");
  if (startDateStr.isNotEmpty())
    challenge.startDate = juce::Time::fromISO8601(startDateStr);

  juce::String endDateStr = ::Json::getString(json, "end_date");
  if (endDateStr.isNotEmpty())
    challenge.endDate = juce::Time::fromISO8601(endDateStr);

  juce::String votingEndDateStr = ::Json::getString(json, "voting_end_date");
  if (votingEndDateStr.isNotEmpty())
    challenge.votingEndDate = juce::Time::fromISO8601(votingEndDateStr);

  // Parse entry count
  if (json.hasProperty("entries")) {
    auto entries = json["entries"];
    if (entries.isArray())
      challenge.entryCount = entries.size();
  } else if (json.hasProperty("entry_count")) {
    challenge.entryCount = ::Json::getInt(json, "entry_count");
  }

  // Parse timestamp
  juce::String timeStr = ::Json::getString(json, "created_at");
  if (timeStr.isNotEmpty()) {
    challenge.createdAt = juce::Time::fromISO8601(timeStr);
  }

  return challenge;
}

// ==============================================================================
MIDIChallengeEntry MIDIChallengeEntry::fromJSON(const juce::var &json) {
  MIDIChallengeEntry entry;

  entry.id = ::Json::getString(json, "id");
  entry.challengeId = ::Json::getString(json, "challenge_id");
  entry.userId = ::Json::getString(json, "user_id");
  entry.audioUrl = ::Json::getString(json, "audio_url");
  entry.postId = ::Json::getString(json, "post_id");
  entry.midiPatternId = ::Json::getString(json, "midi_pattern_id");
  entry.voteCount = ::Json::getInt(json, "vote_count");
  entry.hasVoted = ::Json::getBool(json, "has_voted", false);

  // Parse user info if present
  if (json.hasProperty("user")) {
    auto user = json["user"];
    entry.username = ::Json::getString(user, "username");
    entry.userAvatarUrl = ::Json::getString(user, "avatar_url");
  }

  // Parse timestamp
  juce::String timeStr = ::Json::getString(json, "submitted_at");
  if (timeStr.isNotEmpty()) {
    entry.submittedAt = juce::Time::fromISO8601(timeStr);
  }

  return entry;
}
