//==============================================================================
// SearchClient.cpp - Search operations
// Part of NetworkClient implementation split
//==============================================================================

#include "../NetworkClient.h"
#include "../../util/Log.h"
#include "../../util/Async.h"

//==============================================================================
// Helper to build API endpoint paths consistently
static juce::String buildApiPath(const char* path)
{
    return juce::String("/api/v1") + path;
}

// Helper to convert RequestResult to Outcome<juce::var>
static Outcome<juce::var> requestResultToOutcome(const NetworkClient::RequestResult& result)
{
    if (result.success && result.isSuccess())
    {
        return Outcome<juce::var>::ok(result.data);
    }
    else
    {
        juce::String errorMsg = result.getUserFriendlyError();
        if (errorMsg.isEmpty())
            errorMsg = "Request failed (HTTP " + juce::String(result.httpStatus) + ")";
        return Outcome<juce::var>::error(errorMsg);
    }
}

//==============================================================================
void NetworkClient::searchPosts(const juce::String& query,
                                const juce::String& genre,
                                int bpmMin,
                                int bpmMax,
                                const juce::String& key,
                                int limit,
                                int offset,
                                ResponseCallback callback)
{
    if (callback == nullptr)
        return;

    // Build query string with filters
    juce::String encodedQuery = juce::URL::addEscapeChars(query, true);
    juce::String endpoint = buildApiPath("/search/posts") + "?q=" + encodedQuery
                          + "&limit=" + juce::String(limit)
                          + "&offset=" + juce::String(offset);

    if (!genre.isEmpty())
    {
        juce::String encodedGenre = juce::URL::addEscapeChars(genre, true);
        endpoint += "&genre=" + encodedGenre;
    }

    if (bpmMin > 0)
        endpoint += "&bpm_min=" + juce::String(bpmMin);

    if (bpmMax < 200)
        endpoint += "&bpm_max=" + juce::String(bpmMax);

    if (!key.isEmpty())
    {
        juce::String encodedKey = juce::URL::addEscapeChars(key, true);
        endpoint += "&key=" + encodedKey;
    }

    Async::runVoid([this, endpoint, callback]() {
        auto result = makeRequestWithRetry(endpoint, "GET", juce::var(), true);

        juce::MessageManager::callAsync([callback, result]() {
            auto outcome = requestResultToOutcome(result);
            callback(outcome);
        });
    });
}

void NetworkClient::getSearchSuggestions(const juce::String& query, int limit, ResponseCallback callback)
{
    if (callback == nullptr)
        return;

    juce::String encodedQuery = juce::URL::addEscapeChars(query, true);
    juce::String endpoint = buildApiPath("/search/suggestions") + "?q=" + encodedQuery
                          + "&limit=" + juce::String(limit);

    Async::runVoid([this, endpoint, callback]() {
        auto result = makeRequestWithRetry(endpoint, "GET", juce::var(), true);

        juce::MessageManager::callAsync([callback, result]() {
            auto outcome = requestResultToOutcome(result);
            callback(outcome);
        });
    });
}
