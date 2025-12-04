#pragma once

#include <JuceHeader.h>

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
    static juce::String formatTimeAgo(const juce::Time& time);

    // Check if post is valid (has required fields)
    bool isValid() const;

    //==============================================================================
    JUCE_LEAK_DETECTOR(FeedPost)
};

//==============================================================================
/**
 * FeedResponse represents a paginated response from the feed API
 */
struct FeedResponse
{
    juce::Array<FeedPost> posts;
    int limit = 20;
    int offset = 0;
    int total = 0;
    bool hasMore = false;
    juce::String error;        // Non-empty if there was an error
};
