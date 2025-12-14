#include "ErrorTracking.h"
#include <iomanip>
#include <sstream>

namespace Sidechain {
namespace Util {
namespace Error {

// Static instance definition
template <>
ErrorTracker* juce::SingletonHolder<ErrorTracker>::instance = nullptr;

ErrorTracker::ErrorTracker()
{
}

ErrorTracker::~ErrorTracker()
{
}

void ErrorTracker::recordError(ErrorSource source,
                               const juce::String& message,
                               ErrorSeverity severity,
                               const std::map<juce::String, juce::String>& context)
{
    std::lock_guard<std::mutex> lock(mutex_);

    ErrorInfo info;
    info.source = source;
    info.message = message;
    info.severity = severity;
    info.context = context;
    info.timestamp = std::chrono::system_clock::now();
    info.id = juce::Uuid().toString();
    info.hash = computeHash(info);

    // Check for duplicate error
    auto duplicateIndex = findDuplicateError(info);
    if (duplicateIndex.has_value())
    {
        errors_[duplicateIndex.value()].occurrenceCount++;
        errors_[duplicateIndex.value()].timestamp = info.timestamp;
    }
    else
    {
        // Add new error
        if (errors_.size() >= MAX_ERRORS)
        {
            // Remove oldest error
            errors_.erase(errors_.begin());
        }
        errors_.push_back(info);
    }

    // Call callbacks
    if (onError_)
        onError_(info);

    if (severity == ErrorSeverity::Critical && onCriticalError_)
        onCriticalError_(info);
}

void ErrorTracker::recordException(ErrorSource source,
                                   const std::exception& exception,
                                   const std::map<juce::String, juce::String>& context)
{
    auto contextCopy = context;
    contextCopy["exception_type"] = typeid(exception).name();
    recordError(source, exception.what(), ErrorSeverity::Error, contextCopy);
}

std::vector<ErrorInfo> ErrorTracker::getAllErrors() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return errors_;
}

std::vector<ErrorInfo> ErrorTracker::getErrorsBySeverity(ErrorSeverity severity) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<ErrorInfo> filtered;
    for (const auto& error : errors_)
    {
        if (error.severity == severity)
            filtered.push_back(error);
    }
    return filtered;
}

std::vector<ErrorInfo> ErrorTracker::getErrorsBySource(ErrorSource source) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<ErrorInfo> filtered;
    for (const auto& error : errors_)
    {
        if (error.source == source)
            filtered.push_back(error);
    }
    return filtered;
}

std::vector<ErrorInfo> ErrorTracker::getRecentErrors(int minutesBack) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<ErrorInfo> filtered;

    auto cutoffTime = std::chrono::system_clock::now() - std::chrono::minutes(minutesBack);

    for (const auto& error : errors_)
    {
        if (error.timestamp >= cutoffTime)
            filtered.push_back(error);
    }
    return filtered;
}

std::optional<ErrorInfo> ErrorTracker::getError(const juce::String& errorId) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& error : errors_)
    {
        if (error.id == errorId)
            return error;
    }
    return std::nullopt;
}

void ErrorTracker::clear()
{
    std::lock_guard<std::mutex> lock(mutex_);
    errors_.clear();
}

void ErrorTracker::clearOlderThan(int minutesBack)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto cutoffTime = std::chrono::system_clock::now() - std::chrono::minutes(minutesBack);

    auto it = errors_.begin();
    while (it != errors_.end())
    {
        if (it->timestamp < cutoffTime)
            it = errors_.erase(it);
        else
            ++it;
    }
}

ErrorTracker::ErrorStats ErrorTracker::getStatistics() const
{
    std::lock_guard<std::mutex> lock(mutex_);

    ErrorStats stats;
    stats.totalErrors = static_cast<int>(errors_.size());

    std::map<juce::String, int> errorCounts;

    for (const auto& error : errors_)
    {
        switch (error.severity)
        {
            case ErrorSeverity::Critical:
                stats.criticalCount++;
                break;
            case ErrorSeverity::Error:
                stats.errorCount++;
                break;
            case ErrorSeverity::Warning:
                stats.warningCount++;
                break;
            case ErrorSeverity::Info:
                stats.infoCount++;
                break;
        }

        stats.bySource[error.source]++;
        errorCounts[error.message] += error.occurrenceCount;
    }

    // Get top 10 most frequent errors
    std::vector<std::pair<juce::String, int>> sortedErrors(errorCounts.begin(), errorCounts.end());
    std::sort(sortedErrors.begin(), sortedErrors.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    for (int i = 0; i < std::min(10, static_cast<int>(sortedErrors.size())); ++i)
    {
        stats.topErrors[sortedErrors[i].first] = sortedErrors[i].second;
    }

    return stats;
}

juce::var ErrorTracker::exportAsJson() const
{
    std::lock_guard<std::mutex> lock(mutex_);

    juce::Array<juce::var> errorsArray;

    for (const auto& error : errors_)
    {
        juce::DynamicObject::Ptr obj(new juce::DynamicObject());
        obj->setProperty("id", error.id);
        obj->setProperty("source", ErrorInfo::sourceToString(error.source));
        obj->setProperty("message", error.message);
        obj->setProperty("severity", ErrorInfo::severityToString(error.severity));
        obj->setProperty("occurrenceCount", error.occurrenceCount);

        // Convert timestamp to ISO 8601 string
        auto timestamp = error.timestamp.time_since_epoch().count();
        auto timeT = std::chrono::system_clock::to_time_t(error.timestamp);
        std::ostringstream oss;
        oss << std::put_time(std::gmtime(&timeT), "%Y-%m-%dT%H:%M:%SZ");
        obj->setProperty("timestamp", oss.str());

        // Add context
        juce::DynamicObject::Ptr contextObj(new juce::DynamicObject());
        for (const auto& pair : error.context)
        {
            contextObj->setProperty(pair.first, pair.second);
        }
        obj->setProperty("context", contextObj.release());

        errorsArray.add(juce::var(obj.release()));
    }

    return juce::var(errorsArray);
}

juce::String ErrorTracker::exportAsCsv() const
{
    std::lock_guard<std::mutex> lock(mutex_);

    juce::String csv;
    csv << "ID,Source,Severity,Message,Occurrences,Timestamp\n";

    for (const auto& error : errors_)
    {
        auto timeT = std::chrono::system_clock::to_time_t(error.timestamp);
        std::ostringstream oss;
        oss << std::put_time(std::gmtime(&timeT), "%Y-%m-%d %H:%M:%S");

        csv << error.id << ","
            << ErrorInfo::sourceToString(error.source) << ","
            << ErrorInfo::severityToString(error.severity) << ","
            << "\"" << error.message.replace("\"", "\"\"") << "\","
            << error.occurrenceCount << ","
            << oss.str() << "\n";
    }

    return csv;
}

size_t ErrorTracker::getErrorCount() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return errors_.size();
}

bool ErrorTracker::hasCriticalErrors() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& error : errors_)
    {
        if (error.severity == ErrorSeverity::Critical)
            return true;
    }
    return false;
}

void ErrorTracker::acknowledgeAll()
{
    std::lock_guard<std::mutex> lock(mutex_);
    // In a real implementation, this would mark errors as acknowledged
}

size_t ErrorTracker::computeHash(const ErrorInfo& error) const
{
    std::hash<std::string> hasher;
    size_t h1 = hasher(error.message.toStdString());
    size_t h2 = hasher(std::to_string(static_cast<int>(error.source)));
    return h1 ^ (h2 << 1);
}

std::optional<size_t> ErrorTracker::findDuplicateError(const ErrorInfo& error) const
{
    for (size_t i = 0; i < errors_.size(); ++i)
    {
        if (errors_[i].hash == error.hash &&
            errors_[i].message == error.message &&
            errors_[i].source == error.source)
        {
            return i;
        }
    }
    return std::nullopt;
}

}  // namespace Error
}  // namespace Util
}  // namespace Sidechain
