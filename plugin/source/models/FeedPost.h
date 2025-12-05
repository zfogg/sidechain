#pragma once

#include <JuceHeader.h>
#include <map>
#include "../util/Time.h"

//==============================================================================
/**
 * FeedPost represents a single post/activity in the feed
 *
 * Maps to the Stream.io Activity structure from the backend
 */
class FeedPost
{
public:
    FeedPost() = default;
    ~FeedPost() = default;

    //==============================================================================
    // Core identifiers
    juce::String id;           // Stream.io activity ID
    juce::String foreignId;    // Our internal ID (e.g., "loop:uuid")
    juce::String actor;        // User reference (e.g., "user:12345")
    juce::String verb;         // Action type (e.g., "posted")
    juce::String object;       // Object reference (e.g., "loop:uuid")

    //==============================================================================
    // Timestamps
    juce::Time timestamp;      // When the post was created
    juce::String timeAgo;      // Human-readable time (e.g., "2h ago")

    //==============================================================================
    // User info (extracted from actor)
    juce::String userId;
    juce::String username;
    juce::String userAvatarUrl;

    //==============================================================================
    // Audio metadata
    juce::String audioUrl;     // URL to the audio file (MP3)
    juce::String waveformSvg;  // SVG waveform data or URL
    float durationSeconds = 0.0f;
    int durationBars = 0;
    int bpm = 0;
    juce::String key;          // Musical key (e.g., "F minor")
    juce::String daw;          // DAW used (e.g., "Ableton Live")

    //==============================================================================
    // Genres/tags
    juce::StringArray genres;

    //==============================================================================
    // Social metrics
    int likeCount = 0;
    int playCount = 0;
    int commentCount = 0;
    bool isLiked = false;      // Whether current user has liked this post
    bool isFollowing = false;  // Whether current user is following this post's author
    bool isOwnPost = false;    // Whether this is the current user's own post

    //==============================================================================
    // Emoji reactions - music-themed emojis
    // Stores counts for each emoji type
    std::map<juce::String, int> reactionCounts;  // emoji -> count
    juce::String userReaction;  // The emoji the current user reacted with (empty if none)

    // Standard reaction emojis for music content
    static inline const juce::StringArray REACTION_EMOJIS = {
        juce::String(juce::CharPointer_UTF8("\xE2\x9D\xA4\xEF\xB8\x8F")),    // heart - love
        juce::String(juce::CharPointer_UTF8("\xF0\x9F\x94\xA5")),            // fire - fire/hot
        juce::String(juce::CharPointer_UTF8("\xF0\x9F\x8E\xB5")),            // music note - music note
        juce::String(juce::CharPointer_UTF8("\xF0\x9F\x92\xAF")),            // 100 - perfect
        juce::String(juce::CharPointer_UTF8("\xF0\x9F\x98\x8D")),            // heart eyes - heart eyes
        juce::String(juce::CharPointer_UTF8("\xF0\x9F\x9A\x80"))             // rocket - rocket/hype
    };

    //==============================================================================
    // Processing status
    enum class Status
    {
        Ready,       // Fully processed, playable
        Processing,  // Still being processed on backend
        Failed,      // Processing failed
        Unknown
    };
    Status status = Status::Unknown;

    //==============================================================================
    // Factory method to create from JSON
    static FeedPost fromJson(const juce::var& json);

    // Convert to JSON (for caching)
    juce::var toJson() const;

    // Extract user ID from actor string (e.g., "user:12345" -> "12345")
    static juce::String extractUserId(const juce::String& actor);

    // Format timestamp as "time ago" string
    // @deprecated Use TimeUtils::formatTimeAgo() instead
    [[deprecated("Use TimeUtils::formatTimeAgo() instead")]]
    static juce::String formatTimeAgo(const juce::Time& time);

    // Check if post is valid (has required fields)
    bool isValid() const;

    //==============================================================================
    JUCE_LEAK_DETECTOR(FeedPost)
};
