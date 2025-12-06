#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
 * Story represents a short music clip with MIDI visualization (Phase 7.5)
 *
 * Stories are 5-60 second audio clips captured from the DAW, optionally
 * including MIDI data for piano roll visualization. They expire after 24 hours.
 */
struct Story
{
    juce::String id;
    juce::String userId;
    juce::String audioUrl;
    float audioDuration = 0.0f;      // Duration in seconds (5-60)
    juce::var midiData;              // MIDI events for visualization
    juce::String waveformData;       // SVG waveform
    int bpm = 0;
    juce::String key;
    juce::StringArray genres;
    int viewCount = 0;
    bool viewed = false;             // Has current user viewed this story
    juce::Time createdAt;
    juce::Time expiresAt;

    // Associated user info (from API response)
    juce::String username;
    juce::String userDisplayName;
    juce::String userAvatarUrl;

    //==============================================================================
    // Helper methods

    // Check if story is expired
    bool isExpired() const;

    // Get remaining time until expiration
    juce::String getExpirationText() const;

    // Check if story has MIDI data
    bool hasMIDI() const;

    // Parse from JSON response
    static Story fromJSON(const juce::var& json);

    // Convert to JSON for upload
    juce::var toJSON() const;
};
