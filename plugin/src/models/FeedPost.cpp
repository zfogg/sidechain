#include "FeedPost.h"
#include "../util/Json.h"
#include "../util/Log.h"
#include "../util/Time.h"

//==============================================================================
/** Create a FeedPost from JSON data
 * Parses getstream.io activity JSON into a FeedPost object.
 * Missing fields will be left as default values.
 * @param json JSON var containing post data from getstream.io
 * @return FeedPost instance (may have empty fields if JSON is incomplete)
 */
FeedPost FeedPost::fromJson(const juce::var& json)
{
    FeedPost post;

    // Core identifiers
    post.id = Json::getString(json, "id");
    post.foreignId = Json::getString(json, "foreign_id");
    post.actor = Json::getString(json, "actor");
    post.verb = Json::getString(json, "verb");
    post.object = Json::getString(json, "object");

    // Extract user ID from actor string
    post.userId = extractUserId(post.actor);

    // Parse timestamp
    juce::String timeStr = Json::getString(json, "time");
    if (timeStr.isNotEmpty())
    {
        // getstream.io uses ISO 8601 format: "2024-01-15T10:30:00.000Z"
        post.timestamp = juce::Time::fromISO8601(timeStr);
        post.timeAgo = TimeUtils::formatTimeAgo(post.timestamp);
    }

    // User info (may be nested in actor_data or user object)
    if (Json::hasKey(json, "actor_data"))
    {
        auto actorData = Json::getObject(json, "actor_data");
        post.username = Json::getString(actorData, "username");
        post.userAvatarUrl = Json::getString(actorData, "avatar_url");
    }
    else if (Json::hasKey(json, "user"))
    {
        auto userData = Json::getObject(json, "user");
        post.username = Json::getString(userData, "username");
        post.userAvatarUrl = Json::getString(userData, "avatar_url");
    }

    // Audio metadata
    post.audioUrl = Json::getString(json, "audio_url");
    post.waveformSvg = Json::getString(json, "waveform");
    post.durationSeconds = Json::getFloat(json, "duration_seconds");
    post.durationBars = Json::getInt(json, "duration_bars");
    post.bpm = Json::getInt(json, "bpm");
    post.key = Json::getString(json, "key");
    post.daw = Json::getString(json, "daw");

    // MIDI metadata (R.3.3 Cross-DAW MIDI Collaboration)
    post.hasMidi = Json::getBool(json, "has_midi");
    post.midiId = Json::getString(json, "midi_pattern_id");
    // Also check for midi_id as alternative field name
    if (post.midiId.isEmpty())
        post.midiId = Json::getString(json, "midi_id");
    // If we have a midi_id, we have MIDI
    if (post.midiId.isNotEmpty())
        post.hasMidi = true;

    // Project file metadata (R.3.4 Project File Exchange)
    post.hasProjectFile = Json::getBool(json, "has_project_file");
    post.projectFileId = Json::getString(json, "project_file_id");
    post.projectFileDaw = Json::getString(json, "project_file_daw");
    // If we have a project_file_id, we have a project file
    if (post.projectFileId.isNotEmpty())
        post.hasProjectFile = true;

    // Remix metadata (R.3.2 Remix Chains)
    post.remixOfPostId = Json::getString(json, "remix_of_post_id");
    post.remixOfStoryId = Json::getString(json, "remix_of_story_id");
    post.remixType = Json::getString(json, "remix_type");
    post.remixChainDepth = Json::getInt(json, "remix_chain_depth");
    post.remixCount = Json::getInt(json, "remix_count");
    // Post is a remix if it has a remix source
    post.isRemix = post.remixOfPostId.isNotEmpty() || post.remixOfStoryId.isNotEmpty();
    // Also check extra data for remix info (from getstream.io activity extra field)
    if (!post.isRemix && Json::hasKey(json, "extra"))
    {
        auto extra = Json::getObject(json, "extra");
        if (Json::hasKey(extra, "remix_of_post_id"))
        {
            post.remixOfPostId = Json::getString(extra, "remix_of_post_id");
            post.isRemix = true;
        }
        if (Json::hasKey(extra, "remix_of_story_id"))
        {
            post.remixOfStoryId = Json::getString(extra, "remix_of_story_id");
            post.isRemix = true;
        }
        if (Json::hasKey(extra, "remix_type"))
            post.remixType = Json::getString(extra, "remix_type");
        if (Json::hasKey(extra, "remix_chain_depth"))
            post.remixChainDepth = Json::getInt(extra, "remix_chain_depth");
    }

    // Parse genres array
    auto genreVar = Json::getArray(json, "genre");
    if (Json::isArray(genreVar))
    {
        for (int i = 0; i < Json::arraySize(genreVar); ++i)
            post.genres.add(Json::getStringAt(genreVar, i));
    }
    else if (Json::hasKey(json, "genre"))
    {
        // Single genre as string
        post.genres.add(Json::getString(json, "genre"));
    }

    // Social metrics - first try enriched data from getstream.io
    // Enriched endpoints return reaction_counts: {"like": 5, "🔥": 3, "❤️": 2}
    auto reactionCounts = Json::getObject(json, "reaction_counts");
    if (Json::isObject(reactionCounts))
    {
        auto* dynObj = reactionCounts.getDynamicObject();
        if (dynObj != nullptr)
        {
            for (const auto& prop : dynObj->getProperties())
            {
                juce::String key = prop.name.toString();
                int count = static_cast<int>(prop.value);
                post.reactionCounts[key] = count;

                // Sum up "like" reactions for backwards compatibility
                if (key == "like")
                    post.likeCount += count;
            }
        }
    }
    else
    {
        // Fallback to traditional like_count field
        post.likeCount = Json::getInt(json, "like_count");
    }

    // Check own_reactions to determine if current user has liked/reacted
    // Format: {"like": ["reaction_id1"], "🔥": ["reaction_id2"]}
    auto ownReactions = Json::getObject(json, "own_reactions");
    if (Json::isObject(ownReactions))
    {
        auto* dynObj = ownReactions.getDynamicObject();
        if (dynObj != nullptr)
        {
            for (const auto& prop : dynObj->getProperties())
            {
                juce::String kind = prop.name.toString();
                if (prop.value.isArray() && prop.value.size() > 0)
                {
                    // User has reacted with this type
                    if (kind == "like")
                        post.isLiked = true;
                    else
                        post.userReaction = kind;  // Store the emoji reaction
                }
            }
        }
    }
    else
    {
        // Fallback to traditional is_liked field
        post.isLiked = Json::getBool(json, "is_liked");
    }

    post.playCount = Json::getInt(json, "play_count");
    post.commentCount = Json::getInt(json, "comment_count");
    post.saveCount = Json::getInt(json, "save_count");
    post.repostCount = Json::getInt(json, "repost_count");
    post.isSaved = Json::getBool(json, "is_saved");
    post.isReposted = Json::getBool(json, "is_reposted");
    post.isFollowing = Json::getBool(json, "is_following");
    post.isOwnPost = Json::getBool(json, "is_own_post");

    // Pin metadata
    post.isPinned = Json::getBool(json, "is_pinned");
    post.pinOrder = Json::getInt(json, "pin_order");

    // Comment audience setting
    post.commentAudience = Json::getString(json, "comment_audience");
    if (post.commentAudience.isEmpty())
        post.commentAudience = "everyone";  // Default

    // Repost metadata (if this is a repost of another post)
    // Check extra data from getstream.io activity
    if (Json::hasKey(json, "extra"))
    {
        auto extra = Json::getObject(json, "extra");
        if (Json::getBool(extra, "is_repost"))
        {
            post.isARepost = true;
            post.originalPostId = Json::getString(extra, "original_post_id");
            post.originalUserId = Json::getString(extra, "original_user_id");
            post.originalUsername = Json::getString(extra, "original_username");
            post.originalAvatarUrl = Json::getString(extra, "original_avatar");
            post.repostQuote = Json::getString(extra, "quote");
        }
    }
    // Also check top-level fields for repost info (alternative format)
    if (!post.isARepost)
    {
        post.isARepost = Json::getBool(json, "is_a_repost");
        if (post.isARepost || Json::hasKey(json, "original_post_id"))
        {
            post.isARepost = true;
            post.originalPostId = Json::getString(json, "original_post_id");
            post.originalUserId = Json::getString(json, "original_user_id");
            post.originalUsername = Json::getString(json, "original_username");
            post.originalAvatarUrl = Json::getString(json, "original_avatar_url");
            post.repostQuote = Json::getString(json, "repost_quote");
        }
    }

    // Recommendation reason (for unified timeline feed)
    post.recommendationReason = Json::getString(json, "recommendation_reason");
    post.source = Json::getString(json, "source");
    post.score = Json::getFloat(json, "score");
    post.isRecommended = Json::getBool(json, "is_recommended");

    // Processing status
    juce::String statusStr = Json::getString(json, "status").toLowerCase();
    if (statusStr == "ready")
        post.status = Status::Ready;
    else if (statusStr == "processing")
        post.status = Status::Processing;
    else if (statusStr == "failed")
        post.status = Status::Failed;
    else
        post.status = Status::Unknown;

    return post;
}

//==============================================================================
/** Convert FeedPost to JSON for caching
 * Serializes the post data to JSON format compatible with fromJson().
 * @return JSON var representation of this post
 */
juce::var FeedPost::toJson() const
{
    auto obj = new juce::DynamicObject();

    // Core identifiers
    obj->setProperty("id", id);
    obj->setProperty("foreign_id", foreignId);
    obj->setProperty("actor", actor);
    obj->setProperty("verb", verb);
    obj->setProperty("object", object);

    // Timestamp
    obj->setProperty("time", timestamp.toISO8601(true));
    obj->setProperty("time_ago", timeAgo);

    // User info
    auto userObj = new juce::DynamicObject();
    userObj->setProperty("id", userId);
    userObj->setProperty("username", username);
    userObj->setProperty("avatar_url", userAvatarUrl);
    obj->setProperty("user", juce::var(userObj));

    // Audio metadata
    obj->setProperty("audio_url", audioUrl);
    obj->setProperty("waveform", waveformSvg);
    obj->setProperty("duration_seconds", durationSeconds);
    obj->setProperty("duration_bars", durationBars);
    obj->setProperty("bpm", bpm);
    obj->setProperty("key", key);
    obj->setProperty("daw", daw);

    // MIDI metadata
    obj->setProperty("has_midi", hasMidi);
    if (midiId.isNotEmpty())
        obj->setProperty("midi_pattern_id", midiId);

    // Project file metadata (R.3.4)
    obj->setProperty("has_project_file", hasProjectFile);
    if (projectFileId.isNotEmpty())
        obj->setProperty("project_file_id", projectFileId);
    if (projectFileDaw.isNotEmpty())
        obj->setProperty("project_file_daw", projectFileDaw);

    // Remix metadata (R.3.2)
    if (isRemix || remixCount > 0)
    {
        obj->setProperty("is_remix", isRemix);
        if (remixOfPostId.isNotEmpty())
            obj->setProperty("remix_of_post_id", remixOfPostId);
        if (remixOfStoryId.isNotEmpty())
            obj->setProperty("remix_of_story_id", remixOfStoryId);
        if (remixType.isNotEmpty())
            obj->setProperty("remix_type", remixType);
        obj->setProperty("remix_chain_depth", remixChainDepth);
        obj->setProperty("remix_count", remixCount);
    }

    // Genres
    juce::Array<juce::var> genreArray;
    for (const auto& genre : genres)
        genreArray.add(genre);
    obj->setProperty("genre", genreArray);

    // Social metrics
    obj->setProperty("like_count", likeCount);
    obj->setProperty("play_count", playCount);
    obj->setProperty("comment_count", commentCount);
    obj->setProperty("save_count", saveCount);
    obj->setProperty("repost_count", repostCount);
    obj->setProperty("is_liked", isLiked);
    obj->setProperty("is_saved", isSaved);
    obj->setProperty("is_reposted", isReposted);
    obj->setProperty("is_following", isFollowing);
    obj->setProperty("is_own_post", isOwnPost);
    obj->setProperty("comment_audience", commentAudience);

    // Pin metadata
    obj->setProperty("is_pinned", isPinned);
    obj->setProperty("pin_order", pinOrder);

    // Repost metadata
    if (isARepost)
    {
        obj->setProperty("is_a_repost", isARepost);
        if (originalPostId.isNotEmpty())
            obj->setProperty("original_post_id", originalPostId);
        if (originalUserId.isNotEmpty())
            obj->setProperty("original_user_id", originalUserId);
        if (originalUsername.isNotEmpty())
            obj->setProperty("original_username", originalUsername);
        if (originalAvatarUrl.isNotEmpty())
            obj->setProperty("original_avatar_url", originalAvatarUrl);
        if (repostQuote.isNotEmpty())
            obj->setProperty("repost_quote", repostQuote);
    }

    // Serialize reaction counts for caching enriched data
    if (!reactionCounts.empty())
    {
        auto reactionCountsObj = new juce::DynamicObject();
        for (const auto& [key, count] : reactionCounts)
            reactionCountsObj->setProperty(key, count);
        obj->setProperty("reaction_counts", juce::var(reactionCountsObj));
    }

    // Serialize user's own reaction
    if (userReaction.isNotEmpty())
    {
        auto ownReactionsObj = new juce::DynamicObject();
        juce::Array<juce::var> reactionIds;
        reactionIds.add("cached_reaction");
        ownReactionsObj->setProperty(userReaction, reactionIds);
        obj->setProperty("own_reactions", juce::var(ownReactionsObj));
    }

    // Recommendation metadata
    if (recommendationReason.isNotEmpty())
        obj->setProperty("recommendation_reason", recommendationReason);
    if (source.isNotEmpty())
        obj->setProperty("source", source);
    if (score > 0.0f)
        obj->setProperty("score", score);
    if (isRecommended)
        obj->setProperty("is_recommended", isRecommended);

    // Status
    juce::String statusStr;
    switch (status)
    {
        case Status::Ready:      statusStr = "ready"; break;
        case Status::Processing: statusStr = "processing"; break;
        case Status::Failed:     statusStr = "failed"; break;
        default:                 statusStr = "unknown"; break;
    }
    obj->setProperty("status", statusStr);

    return juce::var(obj);
}

//==============================================================================
/** Extract user ID from actor string
 * Parses getstream.io actor format (e.g., "user:12345" or "SU:user:12345")
 * @param actorString Actor string in format "user:ID" or "SU:user:ID"
 * @return Extracted user ID, or empty string if format is invalid
 */
juce::String FeedPost::extractUserId(const juce::String& actorString)
{
    // Actor format: "user:12345" or "SU:user:12345" (Stream User prefix)
    if (actorString.isEmpty())
        return {};

    // Handle "SU:user:id" format
    if (actorString.startsWith("SU:"))
    {
        auto withoutPrefix = actorString.substring(3);
        if (withoutPrefix.startsWith("user:"))
            return withoutPrefix.substring(5);
        return withoutPrefix;
    }

    // Handle "user:id" format
    if (actorString.startsWith("user:"))
        return actorString.substring(5);

    // If no prefix, assume it's just the ID
    return actorString;
}

//==============================================================================
/** Format timestamp as "time ago" string
 * @deprecated Use TimeUtils::formatTimeAgo() instead
 * @param time Timestamp to format
 * @return Human-readable time string (e.g., "2h ago")
 */
juce::String FeedPost::formatTimeAgo(const juce::Time& time)
{
    // Delegate to TimeUtils
    return TimeUtils::formatTimeAgo(time);
}

//==============================================================================
/** Type-safe parsing with validation
 * Parses JSON and validates required fields. Returns error if validation fails.
 * @param json JSON var containing post data
 * @return Outcome with FeedPost if valid, or error message if invalid
 */
Outcome<FeedPost> FeedPost::tryFromJson(const juce::var& json)
{
    // Validate input
    if (!Json::isObject(json))
        return Outcome<FeedPost>::error("Invalid JSON: expected object");

    // Parse using existing method
    FeedPost post = fromJson(json);

    // Validate required fields
    if (post.id.isEmpty())
        return Outcome<FeedPost>::error("Missing required field: id");

    if (post.audioUrl.isEmpty())
        return Outcome<FeedPost>::error("Missing required field: audio_url");

    if (post.actor.isEmpty())
        return Outcome<FeedPost>::error("Missing required field: actor");

    // Log successful parse at debug level
    Log::debug("Parsed FeedPost: " + post.id + " by " + post.username);

    return Outcome<FeedPost>::ok(std::move(post));
}

//==============================================================================
/** Check if post is valid (has required fields)
 * A valid post must have at least an ID and audio URL to be playable.
 * @return true if post has all required fields, false otherwise
 */
bool FeedPost::isValid() const
{
    // A post must have at least an ID and an audio URL to be playable
    return id.isNotEmpty() && audioUrl.isNotEmpty();
}
