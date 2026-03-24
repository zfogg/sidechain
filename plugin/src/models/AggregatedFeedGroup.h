#pragma once

#include "FeedPost.h"
#include <JuceHeader.h>
#include <nlohmann/json.hpp>

namespace Sidechain {

// ==============================================================================
/**
 * AggregatedFeedGroup represents a group of activities from an aggregated feed
 * Used for displaying grouped notifications like "X and 3 others posted today"
 *
 * getstream.io groups activities based on aggregation_format configured in
 * dashboard:
 * - {{ actor }}_{{ verb }}_{{ time.strftime('%Y-%m-%d') }} groups by
 * user+action+day
 * - {{ verb }}_{{ time.strftime('%Y-%m-%d') }} groups by action+day (across
 * users)
 */
struct AggregatedFeedGroup {
  juce::String id;                  // Unique group ID
  juce::String groupKey;            // The aggregation key (e.g., "user123_posted_2024-01-15")
  juce::String verb;                // Common verb for the group (e.g., "posted", "liked")
  int activityCount = 0;            // Number of activities in this group
  int actorCount = 0;               // Number of unique actors
  juce::Array<FeedPost> activities; // The grouped activities
  juce::Time createdAt;
  juce::Time updatedAt;

  // Factory method to create from JSON (nlohmann::json)
  static AggregatedFeedGroup fromJson(const nlohmann::json &json) {
    AggregatedFeedGroup group;

    if (!json.is_object()) {
      return group;
    }

    group.id =
        json.contains("id") && json["id"].is_string() ? juce::String(json["id"].get<std::string>()) : juce::String();
    group.groupKey = json.contains("group") && json["group"].is_string()
                         ? juce::String(json["group"].get<std::string>())
                         : juce::String();
    group.verb = json.contains("verb") && json["verb"].is_string() ? juce::String(json["verb"].get<std::string>())
                                                                   : juce::String();
    group.activityCount = json.value("activity_count", 0);
    group.actorCount = json.value("actor_count", 0);

    // Parse activities array
    if (json.contains("activities") && json["activities"].is_array()) {
      for (const auto &activityJson : json["activities"]) {
        try {
          auto result = Sidechain::SerializableModel<FeedPost>::createFromJson(activityJson);
          if (result.isOk()) {
            auto post = *result.getValue();
            if (post.isValid())
              group.activities.add(post);
          }
        } catch (const std::exception &) {
          // Skip invalid activities
        }
      }
    }

    // Parse timestamps
    if (json.contains("created_at") && json["created_at"].is_string()) {
      juce::String createdStr(json["created_at"].get<std::string>());
      if (createdStr.isNotEmpty())
        group.createdAt = juce::Time::fromISO8601(createdStr);
    }

    if (json.contains("updated_at") && json["updated_at"].is_string()) {
      juce::String updatedStr(json["updated_at"].get<std::string>());
      if (updatedStr.isNotEmpty())
        group.updatedAt = juce::Time::fromISO8601(updatedStr);
    }

    return group;
  }

  // Generate a human-readable summary like "X and 3 others posted today"
  juce::String getSummary() const {
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

  bool isValid() const {
    return id.isNotEmpty() && !activities.isEmpty();
  }
};

} // namespace Sidechain
