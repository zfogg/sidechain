#pragma once

#include <JuceHeader.h>
#include <map>
#include "util/Time.h"

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
        juce::String(juce::CharPointer_UTF8("\xE2\x9D\xA4\xEF\xB8\x8F")),    // ❤️ - love
        juce::String(juce::CharPointer_UTF8("\xF0\x9F\x94\xA5")),            // 🔥 - fire/hot
        juce::String(juce::CharPointer_UTF8("\xF0\x9F\x8E\xB5")),            // 🎵 - music note
        juce::String(juce::CharPointer_UTF8("\xF0\x9F\x92\xAF")),            // 💯 - perfect
        juce::String(juce::CharPointer_UTF8("\xF0\x9F\x98\x8D")),            // 😍 - heart eyes
        juce::String(juce::CharPointer_UTF8("\xF0\x9F\x9A\x80"))             // 🚀 - rocket/hype
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

//==============================================================================
/**
 * AggregatedFeedGroup represents a group of activities from an aggregated feed
 * Used for displaying grouped notifications like "X and 3 others posted today"
 *
 * Stream.io groups activities based on aggregation_format configured in dashboard:
 * - {{ actor }}_{{ verb }}_{{ time.strftime('%Y-%m-%d') }} groups by user+action+day
 * - {{ verb }}_{{ time.strftime('%Y-%m-%d') }} groups by action+day (across users)
 */
struct AggregatedFeedGroup
{
    juce::String id;                    // Unique group ID
    juce::String groupKey;              // The aggregation key (e.g., "user123_posted_2024-01-15")
    juce::String verb;                  // Common verb for the group (e.g., "posted", "liked")
    int activityCount = 0;              // Number of activities in this group
    int actorCount = 0;                 // Number of unique actors
    juce::Array<FeedPost> activities;   // The grouped activities
    juce::Time createdAt;
    juce::Time updatedAt;

    // Factory method to create from JSON
    static AggregatedFeedGroup fromJson(const juce::var& json)
    {
        AggregatedFeedGroup group;
        group.id = json.getProperty("id", "").toString();
        group.groupKey = json.getProperty("group", "").toString();
        group.verb = json.getProperty("verb", "").toString();
        group.activityCount = static_cast<int>(json.getProperty("activity_count", 0));
        group.actorCount = static_cast<int>(json.getProperty("actor_count", 0));

        // Parse activities array
        auto activitiesVar = json.getProperty("activities", juce::var());
        if (activitiesVar.isArray())
        {
            for (int i = 0; i < activitiesVar.size(); ++i)
            {
                auto post = FeedPost::fromJson(activitiesVar[i]);
                if (post.isValid())
                    group.activities.add(post);
            }
        }

        // Parse timestamps
        juce::String createdStr = json.getProperty("created_at", "").toString();
        if (createdStr.isNotEmpty())
            group.createdAt = juce::Time::fromISO8601(createdStr);

        juce::String updatedStr = json.getProperty("updated_at", "").toString();
        if (updatedStr.isNotEmpty())
            group.updatedAt = juce::Time::fromISO8601(updatedStr);

        return group;
    }

    // Generate a human-readable summary like "X and 3 others posted today"
    juce::String getSummary() const
    {
        if (activities.isEmpty())
            return "";

        juce::String firstActor = activities[0].username;
        if (actorCount == 1)
            return firstActor + " " + verb;
        else if (actorCount == 2)
            return firstActor + " and 1 other " + verb;
        else
            return firstActor + " and " + juce::String(actorCount - 1) + " others " + verb;
    }

    bool isValid() const { return id.isNotEmpty() && !activities.isEmpty(); }
};

//==============================================================================
/**
 * AggregatedFeedResponse represents a paginated response from an aggregated feed API
 */
struct AggregatedFeedResponse
{
    juce::Array<AggregatedFeedGroup> groups;
    int limit = 20;
    int offset = 0;
    int total = 0;
    bool hasMore = false;
    juce::String error;
};
