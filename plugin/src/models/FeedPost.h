#pragma once

#include <JuceHeader.h>
#include <map>
#include "../util/Time.h"
#include "../util/Result.h"

//==============================================================================
/**
 * FeedPost represents a single post/activity in the feed
 *
 * Maps to the getstream.io Activity structure from the backend
 */
class FeedPost
{
public:
    FeedPost() = default;
    ~FeedPost() = default;

    //==============================================================================
    // Core identifiers
    juce::String id;           // getstream.io activity ID
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
    // MIDI metadata (R.3.3 Cross-DAW MIDI Collaboration)
    bool hasMidi = false;      // Whether this post has associated MIDI data
    juce::String midiId;       // UUID of the MIDI pattern (for download)

    //==============================================================================
    // Project file metadata (R.3.4 Project File Exchange)
    bool hasProjectFile = false;     // Whether this post has an associated project file
    juce::String projectFileId;      // UUID of the project file (for download)
    juce::String projectFileDaw;     // DAW type (e.g., "ableton", "fl_studio")

    //==============================================================================
    // Remix metadata (R.3.2 Remix Chains)
    bool isRemix = false;            // Whether this post is a remix of another post/story
    juce::String remixOfPostId;      // ID of the original post (if remix of post)
    juce::String remixOfStoryId;     // ID of the original story (if remix of story)
    juce::String remixType;          // "audio", "midi", or "both"
    int remixChainDepth = 0;         // 0=original, 1=remix, 2=remix of remix, etc.
    int remixCount = 0;              // Number of remixes this post has

    //==============================================================================
    // Genres/tags
    juce::StringArray genres;

    //==============================================================================
    // Social metrics
    int likeCount = 0;
    int playCount = 0;
    int commentCount = 0;
    int saveCount = 0;         // Number of users who saved/bookmarked this post
    int repostCount = 0;       // Number of times this post has been reposted
    bool isLiked = false;      // Whether current user has liked this post
    bool isSaved = false;      // Whether current user has saved/bookmarked this post
    bool isReposted = false;   // Whether current user has reposted this post
    bool isFollowing = false;  // Whether current user is following this post's author
    bool isOwnPost = false;    // Whether this is the current user's own post

    //==============================================================================
    // Pinned post metadata
    bool isPinned = false;     // Whether this post is pinned to user's profile
    int pinOrder = 0;          // Order among pinned posts (1-3), 0 if not pinned

    //==============================================================================
    // Comment controls
    juce::String commentAudience = "everyone";  // "everyone", "followers", "off"
    bool commentsDisabled() const { return commentAudience == "off"; }
    bool commentsFollowersOnly() const { return commentAudience == "followers"; }

    //==============================================================================
    // Repost metadata (when this post is a repost of another post)
    bool isARepost = false;             // Whether this post is actually a repost
    juce::String originalPostId;        // ID of the original post (if this is a repost)
    juce::String originalUserId;        // User ID of original poster
    juce::String originalUsername;      // Username of original poster
    juce::String originalAvatarUrl;     // Avatar URL of original poster
    juce::String repostQuote;           // Optional quote/comment from reposter

    //==============================================================================
    // Online status (presence)
    bool isOnline = false;     // Whether post author is currently online
    bool isInStudio = false;   // Whether post author is "in studio" (custom status)

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
    // Recommendation metadata (for unified timeline feed)
    juce::String recommendationReason;  // Why this post was recommended (e.g., "matches your genre preferences")
    juce::String source;                // Where this post came from: "following", "gorse", "trending", "recent"
    float score = 0.0f;                 // Ranking score from timeline service
    bool isRecommended = false;         // Whether this is a recommendation (vs from followed users)

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
    // Factory methods to create from JSON

    /** Create a FeedPost from JSON data
     *  @param json JSON var containing post data
     *  @return FeedPost instance (may be invalid if JSON is malformed)
     */
    static FeedPost fromJson(const juce::var& json);

    /** Type-safe parsing with validation - returns error if required fields are missing
     *  @param json JSON var containing post data
     *  @return Outcome with FeedPost if valid, or error message if invalid
     */
    static Outcome<FeedPost> tryFromJson(const juce::var& json);

    /** Convert to JSON (for caching)
     *  @return JSON var representation of this post
     */
    juce::var toJson() const;

    /** Extract user ID from actor string (e.g., "user:12345" -> "12345")
     *  @param actor Actor string in format "user:ID"
     *  @return Extracted user ID, or empty string if format is invalid
     */
    static juce::String extractUserId(const juce::String& actor);

    /** Format timestamp as "time ago" string
     *  @deprecated Use TimeUtils::formatTimeAgo() instead
     *  @param time Timestamp to format
     *  @return Human-readable time string (e.g., "2h ago")
     */
    [[deprecated("Use TimeUtils::formatTimeAgo() instead")]]
    static juce::String formatTimeAgo(const juce::Time& time);

    /** Check if post is valid (has required fields)
     *  @return true if post has all required fields, false otherwise
     */
    bool isValid() const;

    //==============================================================================
    JUCE_LEAK_DETECTOR(FeedPost)
};
