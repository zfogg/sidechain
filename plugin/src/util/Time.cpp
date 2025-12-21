#include "Time.h"

namespace Sidechain {
namespace TimeUtils {

juce::String formatTimeAgo(const juce::Time &time) {
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

  if (minutes < 60) {
    if (minutes == 1)
      return "1 min ago";
    return juce::String(minutes) + " mins ago";
  }

  if (hours < 24) {
    if (hours == 1)
      return "1 hour ago";
    return juce::String(hours) + " hours ago";
  }

  if (days < 7) {
    if (days == 1)
      return "1 day ago";
    return juce::String(days) + " days ago";
  }

  if (days < 30) {
    int weeks = static_cast<int>(days / 7);
    if (weeks == 1)
      return "1 week ago";
    return juce::String(weeks) + " weeks ago";
  }

  if (days < 365) {
    int months = static_cast<int>(days / 30);
    if (months == 1)
      return "1 month ago";
    return juce::String(months) + " months ago";
  }

  int years = static_cast<int>(days / 365);
  if (years == 1)
    return "1 year ago";
  return juce::String(years) + " years ago";
}

juce::String formatTimeAgoShort(const juce::Time &time) {
  auto now = juce::Time::getCurrentTime();
  auto diff = now - time;

  auto seconds = static_cast<juce::int64>(std::round(diff.inSeconds()));
  auto minutes = static_cast<juce::int64>(std::round(diff.inMinutes()));
  auto hours = static_cast<juce::int64>(std::round(diff.inHours()));
  auto days = static_cast<juce::int64>(std::round(diff.inDays()));

  if (seconds < 0)
    return "now";

  if (seconds < 60)
    return "now";

  if (minutes < 60)
    return juce::String(minutes) + "m";

  if (hours < 24)
    return juce::String(hours) + "h";

  if (days < 7)
    return juce::String(days) + "d";

  if (days < 28) {
    int weeks = static_cast<int>(days / 7);
    return juce::String(weeks) + "w";
  }

  // For older dates, show the actual date
  return time.formatted("%b %d");
}

} // namespace TimeUtils
} // namespace Sidechain
