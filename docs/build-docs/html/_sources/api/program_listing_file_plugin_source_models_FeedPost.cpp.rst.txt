
.. _program_listing_file_plugin_source_models_FeedPost.cpp:

Program Listing for File FeedPost.cpp
=====================================

|exhale_lsh| :ref:`Return to documentation for file <file_plugin_source_models_FeedPost.cpp>` (``plugin/source/models/FeedPost.cpp``)

.. |exhale_lsh| unicode:: U+021B0 .. UPWARDS ARROW WITH TIP LEFTWARDS

.. code-block:: cpp

   #include "FeedPost.h"
   #include "../util/Json.h"
   #include "../util/Log.h"
   
   //==============================================================================
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
           post.timeAgo = formatTimeAgo(post.timestamp);
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
       post.isFollowing = Json::getBool(json, "is_following");
       post.isOwnPost = Json::getBool(json, "is_own_post");
   
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
       // Delegate to TimeUtils
       return TimeUtils::formatTimeAgo(time);
   }
   
   //==============================================================================
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
   bool FeedPost::isValid() const
   {
       // A post must have at least an ID and an audio URL to be playable
       return id.isNotEmpty() && audioUrl.isNotEmpty();
   }
