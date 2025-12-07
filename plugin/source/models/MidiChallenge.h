#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
 * MIDIChallengeConstraints represents the constraints for a MIDI challenge
 */
struct MIDIChallengeConstraints
{
    int bpmMin = 0;  // 0 means no constraint
    int bpmMax = 0;  // 0 means no constraint
    juce::String key;  // Empty means no constraint
    juce::String scale;  // Empty means no constraint
    int noteCountMin = 0;  // 0 means no constraint
    int noteCountMax = 0;  // 0 means no constraint
    double durationMin = 0.0;  // 0 means no constraint
    double durationMax = 0.0;  // 0 means no constraint

    // Parse from JSON
    static MIDIChallengeConstraints fromJSON(const juce::var& json);
};

//==============================================================================
/**
 * MIDIChallenge represents a MIDI challenge (R.2.2.1.1)
 */
struct MIDIChallenge
{
    juce::String id;
    juce::String title;
    juce::String description;
    MIDIChallengeConstraints constraints;
    juce::Time startDate;
    juce::Time endDate;
    juce::Time votingEndDate;
    juce::String status;  // "upcoming", "active", "voting", "ended"
    int entryCount = 0;
    juce::Time createdAt;

    // Parse from JSON response
    static MIDIChallenge fromJSON(const juce::var& json);

    // Check if challenge is currently accepting submissions
    bool isAcceptingSubmissions() const
    {
        auto now = juce::Time::getCurrentTime();
        return now >= startDate && now <= endDate;
    }

    // Check if challenge is in voting phase
    bool isVoting() const
    {
        auto now = juce::Time::getCurrentTime();
        return now > endDate && now <= votingEndDate;
    }

    // Check if challenge has ended
    bool hasEnded() const
    {
        auto now = juce::Time::getCurrentTime();
        return now > votingEndDate;
    }
};

//==============================================================================
/**
 * MIDIChallengeEntry represents a user's submission to a MIDI challenge
 */
struct MIDIChallengeEntry
{
    juce::String id;
    juce::String challengeId;
    juce::String userId;
    juce::String username;
    juce::String userAvatarUrl;
    juce::String audioUrl;
    juce::String postId;  // Optional link to AudioPost
    juce::String midiPatternId;  // Optional link to MIDIPattern
    int voteCount = 0;
    bool hasVoted = false;  // Whether current user has voted for this entry
    juce::Time submittedAt;

    // Parse from JSON response
    static MIDIChallengeEntry fromJSON(const juce::var& json);
};
