
.. _program_listing_file_plugin_source_models_AggregatedFeedGroup.h:

Program Listing for File AggregatedFeedGroup.h
==============================================

|exhale_lsh| :ref:`Return to documentation for file <file_plugin_source_models_AggregatedFeedGroup.h>` (``plugin/source/models/AggregatedFeedGroup.h``)

.. |exhale_lsh| unicode:: U+021B0 .. UPWARDS ARROW WITH TIP LEFTWARDS

.. code-block:: cpp

   #pragma once
   
   #include <JuceHeader.h>
   #include "FeedPost.h"
   
   //==============================================================================
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
