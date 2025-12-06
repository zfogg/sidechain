#include "StringFormatter.h"
#include <cmath>

namespace StringFormatter
{

//==============================================================================
// Count Formatting

juce::String formatCount(int value)
{
    return formatCount(value, 1);
}

juce::String formatCount(int value, int decimals)
{
    if (value < 0)
        return "-" + formatCount(-value, decimals);

    if (value < 1000)
        return juce::String(value);

    if (value < 1000000)
    {
        double k = value / 1000.0;
        if (k >= 100.0)
            return juce::String(static_cast<int>(k)) + "K";
        return juce::String(k, decimals) + "K";
    }

    if (value < 1000000000)
    {
        double m = value / 1000000.0;
        if (m >= 100.0)
            return juce::String(static_cast<int>(m)) + "M";
        return juce::String(m, decimals) + "M";
    }

    double b = value / 1000000000.0;
    return juce::String(b, decimals) + "B";
}

juce::String formatLargeNumber(int64_t value)
{
    if (value < 0)
        return "-" + formatLargeNumber(-value);

    if (value < 1000)
        return juce::String(value);

    if (value < 1000000)
        return juce::String(value / 1000.0, 1) + "K";

    if (value < 1000000000)
        return juce::String(value / 1000000.0, 1) + "M";

    return juce::String(value / 1000000000.0, 1) + "B";
}

//==============================================================================
// Duration Formatting

juce::String formatDuration(double seconds)
{
    if (seconds < 0)
        seconds = 0;

    int totalSeconds = static_cast<int>(std::round(seconds));
    int hours = totalSeconds / 3600;
    int minutes = (totalSeconds % 3600) / 60;
    int secs = totalSeconds % 60;

    if (hours > 0)
    {
        return juce::String(hours) + ":" +
               juce::String(minutes).paddedLeft('0', 2) + ":" +
               juce::String(secs).paddedLeft('0', 2);
    }

    return juce::String(minutes) + ":" +
           juce::String(secs).paddedLeft('0', 2);
}

juce::String formatDurationMMSS(double seconds)
{
    if (seconds < 0)
        seconds = 0;

    int totalSeconds = static_cast<int>(std::round(seconds));
    int minutes = totalSeconds / 60;
    int secs = totalSeconds % 60;

    return juce::String(minutes) + ":" +
           juce::String(secs).paddedLeft('0', 2);
}

juce::String formatDurationHMS(double seconds)
{
    if (seconds < 0)
        seconds = 0;

    int totalSeconds = static_cast<int>(std::round(seconds));
    int hours = totalSeconds / 3600;
    int minutes = (totalSeconds % 3600) / 60;
    int secs = totalSeconds % 60;

    return juce::String(hours) + ":" +
           juce::String(minutes).paddedLeft('0', 2) + ":" +
           juce::String(secs).paddedLeft('0', 2);
}

juce::String formatMilliseconds(int ms)
{
    if (ms < 1000)
        return juce::String(ms) + "ms";

    return juce::String(ms / 1000.0, 1) + "s";
}

//==============================================================================
// Percentage Formatting

juce::String formatPercentage(float value)
{
    return formatPercentage(value, 0);
}

juce::String formatPercentage(float value, int decimals)
{
    float percentage = value * 100.0f;

    if (decimals <= 0)
        return juce::String(static_cast<int>(std::round(percentage))) + "%";

    return juce::String(percentage, decimals) + "%";
}

//==============================================================================
// Music-specific Formatting

juce::String formatBPM(double bpm)
{
    return formatBPMValue(bpm) + " BPM";
}

juce::String formatBPMValue(double bpm)
{
    // Show decimal only if not a whole number
    if (std::abs(bpm - std::round(bpm)) < 0.05)
        return juce::String(static_cast<int>(std::round(bpm)));

    return juce::String(bpm, 1);
}

juce::String formatConfidence(float confidence)
{
    return formatPercentage(confidence, 0);
}

juce::String formatKeyLong(const juce::String& key)
{
    if (key.isEmpty())
        return "Unknown";

    // Check if minor key (ends with 'm')
    bool isMinor = key.endsWithChar('m');
    juce::String root = isMinor ? key.dropLastCharacters(1) : key;

    return root + (isMinor ? " minor" : " major");
}

//==============================================================================
// Social/Engagement Formatting

juce::String formatFollowers(int count)
{
    if (count == 1)
        return "1 follower";

    return formatCount(count) + " followers";
}

juce::String formatLikes(int count)
{
    if (count == 1)
        return "1 like";

    return formatCount(count) + " likes";
}

juce::String formatComments(int count)
{
    if (count == 1)
        return "1 comment";

    return formatCount(count) + " comments";
}

juce::String formatPlays(int count)
{
    if (count == 1)
        return "1 play";

    return formatCount(count) + " plays";
}

//==============================================================================
// Time Ago Formatting

juce::String formatTimeAgo(const juce::Time& timestamp)
{
    auto now = juce::Time::getCurrentTime();
    auto diff = now - timestamp;

    int64_t seconds = static_cast<int64_t>(diff.inSeconds());

    if (seconds < 0)
        return "just now";

    if (seconds < 60)
        return "just now";

    int64_t minutes = seconds / 60;
    if (minutes < 60)
        return juce::String(minutes) + "m ago";

    int64_t hours = minutes / 60;
    if (hours < 24)
        return juce::String(hours) + "h ago";

    int64_t days = hours / 24;
    if (days < 7)
        return juce::String(days) + "d ago";

    int64_t weeks = days / 7;
    if (weeks < 4)
        return juce::String(weeks) + "w ago";

    int64_t months = days / 30;
    if (months < 12)
        return juce::String(months) + "mo ago";

    int64_t years = days / 365;
    return juce::String(years) + "y ago";
}

juce::String formatTimeAgo(const juce::String& isoTimestamp)
{
    if (isoTimestamp.isEmpty())
        return "";

    // Parse ISO 8601 timestamp (e.g., "2024-01-15T10:30:00Z")
    auto timestamp = juce::Time::fromISO8601(isoTimestamp);
    return formatTimeAgo(timestamp);
}

}  // namespace StringFormatter
