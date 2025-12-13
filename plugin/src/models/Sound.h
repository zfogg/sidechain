#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
 * Sound represents a unique audio signature detected across posts (Feature #15)
 *
 * When the same sample/sound is used in multiple posts, they are linked
 * to the same Sound, enabling "See X other posts with this sound" discovery.
 */
struct Sound
{
    juce::String id;
    juce::String name;
    juce::String description;
    double duration = 0.0;          // Duration in seconds

    // Creator info
    juce::String creatorId;
    juce::String creatorUsername;
    juce::String creatorDisplayName;
    juce::String creatorAvatarUrl;

    // Original post that first used this sound
    juce::String originalPostId;

    // Usage statistics
    int usageCount = 0;             // Number of posts using this sound
    bool isTrending = false;
    int trendingRank = 0;

    bool isPublic = true;
    juce::Time createdAt;

    //==========================================================================
    // Parsing from JSON

    static Sound fromJson(const juce::var& json)
    {
        Sound sound;

        sound.id = json.getProperty("id", "").toString();
        sound.name = json.getProperty("name", "").toString();
        sound.description = json.getProperty("description", "").toString();
        sound.duration = static_cast<double>(json.getProperty("duration", 0.0));
        sound.creatorId = json.getProperty("creator_id", "").toString();
        sound.originalPostId = json.getProperty("original_post_id", "").toString();
        sound.usageCount = static_cast<int>(json.getProperty("usage_count", 0));
        sound.isTrending = static_cast<bool>(json.getProperty("is_trending", false));
        sound.trendingRank = static_cast<int>(json.getProperty("trending_rank", 0));
        sound.isPublic = static_cast<bool>(json.getProperty("is_public", true));

        // Parse creator object if present
        auto creator = json.getProperty("creator", juce::var());
        if (creator.isObject())
        {
            sound.creatorUsername = creator.getProperty("username", "").toString();
            sound.creatorDisplayName = creator.getProperty("display_name", "").toString();
            sound.creatorAvatarUrl = creator.getProperty("avatar_url", "").toString();
        }

        // Parse created_at timestamp
        auto createdAtStr = json.getProperty("created_at", "").toString();
        if (createdAtStr.isNotEmpty())
        {
            // ISO 8601 format: 2024-01-15T10:30:00Z
            sound.createdAt = juce::Time::fromISO8601(createdAtStr);
        }

        return sound;
    }

    //==========================================================================
    // Display helpers

    /** Get formatted usage count string (e.g., "1.2K posts") */
    juce::String getUsageCountString() const
    {
        if (usageCount >= 1000000)
            return juce::String(usageCount / 1000000.0, 1) + "M posts";
        if (usageCount >= 1000)
            return juce::String(usageCount / 1000.0, 1) + "K posts";
        if (usageCount == 1)
            return "1 post";
        return juce::String(usageCount) + " posts";
    }

    /** Get formatted duration string (e.g., "0:15") */
    juce::String getDurationString() const
    {
        int totalSeconds = static_cast<int>(duration);
        int minutes = totalSeconds / 60;
        int seconds = totalSeconds % 60;
        return juce::String(minutes) + ":" + juce::String(seconds).paddedLeft('0', 2);
    }

    /** Get creator display name or username */
    juce::String getCreatorName() const
    {
        if (creatorDisplayName.isNotEmpty())
            return creatorDisplayName;
        return creatorUsername;
    }
};

//==============================================================================
/**
 * SoundPost represents a post that uses a specific sound
 */
struct SoundPost
{
    juce::String id;
    juce::String audioUrl;
    double duration = 0.0;
    int bpm = 0;
    juce::String key;
    juce::String waveformSvg;
    int likeCount = 0;
    int playCount = 0;
    juce::Time createdAt;

    // User info
    juce::String userId;
    juce::String username;
    juce::String displayName;
    juce::String avatarUrl;

    static SoundPost fromJson(const juce::var& json)
    {
        SoundPost post;

        post.id = json.getProperty("id", "").toString();
        post.audioUrl = json.getProperty("audio_url", "").toString();
        post.duration = static_cast<double>(json.getProperty("duration", 0.0));
        post.bpm = static_cast<int>(json.getProperty("bpm", 0));
        post.key = json.getProperty("key", "").toString();
        post.waveformSvg = json.getProperty("waveform_svg", "").toString();
        post.likeCount = static_cast<int>(json.getProperty("like_count", 0));
        post.playCount = static_cast<int>(json.getProperty("play_count", 0));

        // Parse user object
        auto user = json.getProperty("user", juce::var());
        if (user.isObject())
        {
            post.userId = user.getProperty("id", "").toString();
            post.username = user.getProperty("username", "").toString();
            post.displayName = user.getProperty("display_name", "").toString();
            post.avatarUrl = user.getProperty("avatar_url", "").toString();
        }

        // Parse created_at
        auto createdAtStr = json.getProperty("created_at", "").toString();
        if (createdAtStr.isNotEmpty())
        {
            post.createdAt = juce::Time::fromISO8601(createdAtStr);
        }

        return post;
    }

    juce::String getUserDisplayName() const
    {
        if (displayName.isNotEmpty())
            return displayName;
        return username;
    }
};
