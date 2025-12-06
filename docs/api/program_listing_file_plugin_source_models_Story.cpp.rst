
.. _program_listing_file_plugin_source_models_Story.cpp:

Program Listing for File Story.cpp
==================================

|exhale_lsh| :ref:`Return to documentation for file <file_plugin_source_models_Story.cpp>` (``plugin/source/models/Story.cpp``)

.. |exhale_lsh| unicode:: U+021B0 .. UPWARDS ARROW WITH TIP LEFTWARDS

.. code-block:: cpp

   #include "Story.h"
   
   //==============================================================================
   bool Story::isExpired() const
   {
       return juce::Time::getCurrentTime() > expiresAt;
   }
   
   juce::String Story::getExpirationText() const
   {
       auto now = juce::Time::getCurrentTime();
   
       if (expiresAt < now)
           return "Expired";
   
       auto diff = expiresAt - now;
       int hours = static_cast<int>(diff.inHours());
   
       if (hours < 1)
       {
           int minutes = static_cast<int>(diff.inMinutes());
           if (minutes < 1)
               return "< 1m left";
           return juce::String(minutes) + "m left";
       }
   
       return juce::String(hours) + "h left";
   }
   
   bool Story::hasMIDI() const
   {
       if (!midiData.isObject())
           return false;
   
       // Check if there are any events
       if (midiData.hasProperty("events"))
       {
           auto* events = midiData["events"].getArray();
           return events && !events->isEmpty();
       }
   
       return false;
   }
   
   Story Story::fromJSON(const juce::var& json)
   {
       Story story;
   
       story.id = json["id"].toString();
       story.userId = json["user_id"].toString();
       story.audioUrl = json["audio_url"].toString();
       story.audioDuration = static_cast<float>(json["audio_duration"]);
       story.midiData = json["midi_data"];
       story.waveformData = json["waveform_data"].toString();
       story.bpm = static_cast<int>(json["bpm"]);
       story.key = json["key"].toString();
       story.viewCount = static_cast<int>(json["view_count"]);
       story.viewed = static_cast<bool>(json["viewed"]);
   
       // Parse genres array
       if (json.hasProperty("genres"))
       {
           auto* genresArray = json["genres"].getArray();
           if (genresArray)
           {
               for (const auto& genre : *genresArray)
               {
                   story.genres.add(genre.toString());
               }
           }
       }
   
       // Parse user info if present
       if (json.hasProperty("user"))
       {
           const auto& user = json["user"];
           story.username = user["username"].toString();
           story.userDisplayName = user["display_name"].toString();
           story.userAvatarUrl = user["avatar_url"].toString();
       }
   
       // Parse timestamps (ISO 8601 format)
       // Simplified parsing - in production would use proper date parser
       if (json.hasProperty("created_at"))
       {
           // For now, set to current time
           story.createdAt = juce::Time::getCurrentTime();
       }
   
       if (json.hasProperty("expires_at"))
       {
           // Default to 24 hours from creation
           story.expiresAt = story.createdAt + juce::RelativeTime::hours(24);
       }
   
       return story;
   }
   
   juce::var Story::toJSON() const
   {
       auto* obj = new juce::DynamicObject();
       juce::var json(obj);
   
       obj->setProperty("audio_url", audioUrl);
       obj->setProperty("audio_duration", audioDuration);
   
       if (midiData.isObject())
       {
           obj->setProperty("midi_data", midiData);
       }
   
       if (bpm > 0)
       {
           obj->setProperty("bpm", bpm);
       }
   
       if (key.isNotEmpty())
       {
           obj->setProperty("key", key);
       }
   
       if (!genres.isEmpty())
       {
           juce::var genresArray = juce::var(juce::Array<juce::var>());
           for (const auto& genre : genres)
           {
               genresArray.getArray()->add(genre);
           }
           obj->setProperty("genres", genresArray);
       }
   
       return json;
   }
