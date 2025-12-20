#include "AggregatedFeedGroup.h"

namespace Sidechain {

// ==============================================================================
/**
 * Parse AggregatedFeedGroup from JSON
 * @param json JSON object containing feed group data
 * @return AggregatedFeedGroup with parsed fields
 */
AggregatedFeedGroup AggregatedFeedGroup::fromJSON(const nlohmann::json &json) {
  AggregatedFeedGroup group;

  if (json.contains("id") && json["id"].is_string())
    group.id = juce::String(json["id"].get<std::string>());
  if (json.contains("group") && json["group"].is_string())
    group.groupKey = juce::String(json["group"].get<std::string>());
  if (json.contains("verb") && json["verb"].is_string())
    group.verb = juce::String(json["verb"].get<std::string>());
  if (json.contains("activity_count") && json["activity_count"].is_number_integer())
    group.activityCount = json["activity_count"].get<int>();
  if (json.contains("actor_count") && json["actor_count"].is_number_integer())
    group.actorCount = json["actor_count"].get<int>();

  // Parse activities array
  if (json.contains("activities") && json["activities"].is_array()) {
    for (const auto &activityJson : json["activities"]) {
      auto result = Sidechain::SerializableModel<FeedPost>::createFromJson(activityJson);
      if (result.isOk()) {
        auto post = *result.getValue();
        if (post.isValid())
          group.activities.add(post);
      }
    }
  }

  // Parse timestamps
  if (json.contains("created_at") && json["created_at"].is_string()) {
    juce::String createdStr = juce::String(json["created_at"].get<std::string>());
    if (createdStr.isNotEmpty())
      group.createdAt = juce::Time::fromISO8601(createdStr);
  }

  if (json.contains("updated_at") && json["updated_at"].is_string()) {
    juce::String updatedStr = juce::String(json["updated_at"].get<std::string>());
    if (updatedStr.isNotEmpty())
      group.updatedAt = juce::Time::fromISO8601(updatedStr);
  }

  return group;
}

} // namespace Sidechain
