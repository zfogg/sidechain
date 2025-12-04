#include "FeedPost.h"

//==============================================================================
FeedPost FeedPost::fromJson(const juce::var& json)
{
    FeedPost post;

    // Core identifiers
    post.id = json.getProperty("id", "").toString();
    post.foreignId = json.getProperty("foreign_id", "").toString();
    post.actor = json.getProperty("actor", "").toString();
    post.verb = json.getProperty("verb", "").toString();
    post.object = json.getProperty("object", "").toString();

    // Extract user ID from actor string
    post.userId = extractUserId(post.actor);

    // Parse timestamp
    juce::String timeStr = json.getProperty("time", "").toString();
    if (timeStr.isNotEmpty())
    {
        // Stream.io uses ISO 8601 format: "2024-01-15T10:30:00.000Z"
        post.timestamp = juce::Time::fromISO8601(timeStr);
        post.timeAgo = formatTimeAgo(post.timestamp);
    }

    // User info (may be nested in actor_data or user object)
    if (json.hasProperty("actor_data"))
    {
        auto actorData = json.getProperty("actor_data", juce::var());
        post.username = actorData.getProperty("username", "").toString();
        post.userAvatarUrl = actorData.getProperty("avatar_url", "").toString();
    }
    else if (json.hasProperty("user"))
    {
        auto userData = json.getProperty("user", juce::var());
        post.username = userData.getProperty("username", "").toString();
        post.userAvatarUrl = userData.getProperty("avatar_url", "").toString();
    }

    // Audio metadata
    post.audioUrl = json.getProperty("audio_url", "").toString();
    post.waveformSvg = json.getProperty("waveform", "").toString();
    post.durationSeconds = static_cast<float>(json.getProperty("duration_seconds", 0.0));
    post.durationBars = static_cast<int>(json.getProperty("duration_bars", 0));
    post.bpm = static_cast<int>(json.getProperty("bpm", 0));
    post.key = json.getProperty("key", "").toString();
    post.daw = json.getProperty("daw", "").toString();

    // Parse genres array
    if (json.hasProperty("genre"))
    {
        auto genreVar = json.getProperty("genre", juce::var());
        if (genreVar.isArray())
        {
            for (int i = 0; i < genreVar.size(); ++i)
                post.genres.add(genreVar[i].toString());
        }
        else if (genreVar.isString())
        {
            post.genres.add(genreVar.toString());
        }
    }

    // Social metrics
    post.likeCount = static_cast<int>(json.getProperty("like_count", 0));
    post.playCount = static_cast<int>(json.getProperty("play_count", 0));
    post.commentCount = static_cast<int>(json.getProperty("comment_count", 0));
    post.isLiked = static_cast<bool>(json.getProperty("is_liked", false));
    post.isFollowing = static_cast<bool>(json.getProperty("is_following", false));
    post.isOwnPost = static_cast<bool>(json.getProperty("is_own_post", false));

    // Processing status
    juce::String statusStr = json.getProperty("status", "").toString().toLowerCase();
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

    // Genres
    juce::Array<juce::var> genreArray;
    for (const auto& genre : genres)
        genreArray.add(genre);
    obj->setProperty("genre", genreArray);

    // Social metrics
    obj->setProperty("like_count", likeCount);
    obj->setProperty("play_count", playCount);
    obj->setProperty("comment_count", commentCount);
    obj->setProperty("is_liked", isLiked);
    obj->setProperty("is_following", isFollowing);
    obj->setProperty("is_own_post", isOwnPost);

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
juce::String FeedPost::formatTimeAgo(const juce::Time& time)
{
    auto now = juce::Time::getCurrentTime();
    auto diff = now - time;

    // Use std::round for proper rounding instead of truncation
    auto seconds = static_cast<juce::int64>(std::round(diff.inSeconds()));
    auto minutes = static_cast<juce::int64>(std::round(diff.inMinutes()));
    auto hours = static_cast<juce::int64>(std::round(diff.inHours()));
    auto days = static_cast<juce::int64>(std::round(diff.inDays()));

    if (seconds < 0)
        return "just now"; // Future time, probably clock skew

    if (seconds < 60)
        return "just now";

    if (minutes < 60)
    {
        if (minutes == 1)
            return "1 min ago";
        return juce::String(minutes) + " mins ago";
    }

    if (hours < 24)
    {
        if (hours == 1)
            return "1 hour ago";
        return juce::String(hours) + " hours ago";
    }

    if (days < 7)
    {
        if (days == 1)
            return "1 day ago";
        return juce::String(days) + " days ago";
    }

    if (days < 30)
    {
        int weeks = days / 7;
        if (weeks == 1)
            return "1 week ago";
        return juce::String(weeks) + " weeks ago";
    }

    if (days < 365)
    {
        int months = days / 30;
        if (months == 1)
            return "1 month ago";
        return juce::String(months) + " months ago";
    }

    int years = days / 365;
    if (years == 1)
        return "1 year ago";
    return juce::String(years) + " years ago";
}

//==============================================================================
bool FeedPost::isValid() const
{
    // A post must have at least an ID and an audio URL to be playable
    return id.isNotEmpty() && audioUrl.isNotEmpty();
}
