//==============================================================================
// FeedClient.cpp - Feed operations
// Part of NetworkClient implementation split
//==============================================================================

#include "../NetworkClient.h"
#include "../../util/Constants.h"
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
void NetworkClient::getGlobalFeed(int limit, int offset, FeedCallback callback)
{
    if (!isAuthenticated())
        return;

    Async::runVoid([this, limit, offset, callback]() {
        // Use enriched endpoint to get reaction counts and own reactions from getstream.io
        juce::String endpoint = buildApiPath("/feed/global/enriched") + "?limit=" + juce::String(limit) + "&offset=" + juce::String(offset);
        auto response = makeRequest(endpoint, "GET", juce::var(), true);

        if (callback)
        {
            juce::MessageManager::callAsync([callback, response]() {
                if (response.isObject() || response.isArray())
                    callback(Outcome<juce::var>::ok(response));
                else
                    callback(Outcome<juce::var>::error("Invalid feed response"));
            });
        }
    });
}

void NetworkClient::getTimelineFeed(int limit, int offset, FeedCallback callback)
{
    if (!isAuthenticated())
        return;

    Async::runVoid([this, limit, offset, callback]() {
        // Use unified endpoint which combines followed users, Gorse recommendations, trending, and recent posts
        // This ensures users always see content even when not following anyone
        juce::String endpoint = buildApiPath("/feed/unified") + "?limit=" + juce::String(limit) + "&offset=" + juce::String(offset);
        auto response = makeRequest(endpoint, "GET", juce::var(), true);

        if (callback)
        {
            juce::MessageManager::callAsync([callback, response]() {
                if (response.isObject() || response.isArray())
                    callback(Outcome<juce::var>::ok(response));
                else
                    callback(Outcome<juce::var>::error("Invalid feed response"));
            });
        }
    });
}

void NetworkClient::getTrendingFeed(int limit, int offset, FeedCallback callback)
{
    if (!isAuthenticated())
        return;

    Async::runVoid([this, limit, offset, callback]() {
        // Trending feed uses engagement scoring (likes, plays, comments weighted by recency)
        juce::String endpoint = buildApiPath("/feed/trending") + "?limit=" + juce::String(limit) + "&offset=" + juce::String(offset);
        auto response = makeRequest(endpoint, "GET", juce::var(), true);

        if (callback)
        {
            juce::MessageManager::callAsync([callback, response]() {
                if (response.isObject() || response.isArray())
                    callback(Outcome<juce::var>::ok(response));
                else
                    callback(Outcome<juce::var>::error("Invalid feed response"));
            });
        }
    });
}

void NetworkClient::getForYouFeed(int limit, int offset, FeedCallback callback)
{
    if (!isAuthenticated())
    {
        if (callback)
            callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
        return;
    }

    Async::runVoid([this, limit, offset, callback]() {
        // For You feed uses personalized recommendations
        juce::String endpoint = buildApiPath("/recommendations/for-you") + "?limit=" + juce::String(limit) + "&offset=" + juce::String(offset);
        auto response = makeRequest(endpoint, "GET", juce::var(), true);

        if (callback)
        {
            juce::MessageManager::callAsync([callback, response]() {
                if (response.isObject() || response.isArray())
                    callback(Outcome<juce::var>::ok(response));
                else
                    callback(Outcome<juce::var>::error("Invalid feed response"));
            });
        }
    });
}

void NetworkClient::getSimilarPosts(const juce::String& postId, int limit, FeedCallback callback)
{
    if (!isAuthenticated())
    {
        if (callback)
            callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
        return;
    }

    Async::runVoid([this, postId, limit, callback]() {
        juce::String path = "/recommendations/similar-posts/" + postId;
        juce::String endpoint = buildApiPath(path.toRawUTF8()) + "?limit=" + juce::String(limit);
        auto response = makeRequest(endpoint, "GET", juce::var(), true);

        if (callback)
        {
            juce::MessageManager::callAsync([callback, response]() {
                if (response.isObject() || response.isArray())
                    callback(Outcome<juce::var>::ok(response));
                else
                    callback(Outcome<juce::var>::error("Invalid feed response"));
            });
        }
    });
}

void NetworkClient::likePost(const juce::String& activityId, const juce::String& emoji, ResponseCallback callback)
{
    if (!isAuthenticated())
    {
        if (callback)
            callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
        return;
    }

    Async::runVoid([this, activityId, emoji, callback]() {
        juce::var data = juce::var(new juce::DynamicObject());
        data.getDynamicObject()->setProperty("activity_id", activityId);

        juce::String endpoint;
        if (!emoji.isEmpty())
        {
            // Use emoji reaction endpoint
            data.getDynamicObject()->setProperty("emoji", emoji);
            endpoint = buildApiPath("/social/react");
        }
        else
        {
            // Use standard like endpoint
            endpoint = buildApiPath("/social/like");
        }

        auto result = makeRequestWithRetry(endpoint, "POST", data, true);
        Log::debug("Like/reaction response: " + juce::JSON::toString(result.data));

        if (callback)
        {
            juce::MessageManager::callAsync([callback, result]() {
                auto outcome = requestResultToOutcome(result);
                callback(outcome);
            });
        }
    });
}

void NetworkClient::unlikePost(const juce::String& activityId, ResponseCallback callback)
{
    if (!isAuthenticated())
    {
        if (callback)
            callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
        return;
    }

    Async::runVoid([this, activityId, callback]() {
        juce::var data = juce::var(new juce::DynamicObject());
        data.getDynamicObject()->setProperty("activity_id", activityId);

        auto result = makeRequestWithRetry(buildApiPath("/social/like"), "DELETE", data, true);
        Log::debug("Unlike response: " + juce::JSON::toString(result.data));

        if (callback)
        {
            juce::MessageManager::callAsync([callback, result]() {
                auto outcome = requestResultToOutcome(result);
                callback(outcome);
            });
        }
    });
}

void NetworkClient::deletePost(const juce::String& postId, ResponseCallback callback)
{
    if (!isAuthenticated())
    {
        if (callback)
            callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
        return;
    }

    Async::runVoid([this, postId, callback]() {
        juce::String endpoint = buildApiPath(("/posts/" + postId).toRawUTF8());
        auto result = makeRequestWithRetry(endpoint, "DELETE", juce::var(), true);
        Log::debug("Delete post response: " + juce::JSON::toString(result.data));

        if (callback)
        {
            juce::MessageManager::callAsync([callback, result]() {
                auto outcome = requestResultToOutcome(result);
                callback(outcome);
            });
        }
    });
}

void NetworkClient::reportPost(const juce::String& postId, const juce::String& reason, const juce::String& description, ResponseCallback callback)
{
    if (!isAuthenticated())
    {
        if (callback)
            callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
        return;
    }

    Async::runVoid([this, postId, reason, description, callback]() {
        juce::String endpoint = buildApiPath(("/posts/" + postId + "/report").toRawUTF8());
        juce::var data = juce::var(new juce::DynamicObject());
        auto* obj = data.getDynamicObject();
        if (obj != nullptr)
        {
            obj->setProperty("reason", reason);
            if (description.isNotEmpty())
                obj->setProperty("description", description);
        }

        auto result = makeRequestWithRetry(endpoint, "POST", data, true);
        Log::debug("Report post response: " + juce::JSON::toString(result.data));

        if (callback)
        {
            juce::MessageManager::callAsync([callback, result]() {
                auto outcome = requestResultToOutcome(result);
                callback(outcome);
            });
        }
    });
}

//==============================================================================
// R.3.2 Remix Chains

void NetworkClient::getRemixChain(const juce::String& postId, ResponseCallback callback)
{
    if (!isAuthenticated())
    {
        if (callback)
            callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
        return;
    }

    Async::runVoid([this, postId, callback]() {
        juce::String endpoint = buildApiPath(("/posts/" + postId + "/remix-chain").toRawUTF8());
        auto result = makeRequestWithRetry(endpoint, "GET", juce::var(), true);
        Log::debug("Get remix chain response: " + juce::JSON::toString(result.data));

        if (callback)
        {
            juce::MessageManager::callAsync([callback, result]() {
                auto outcome = requestResultToOutcome(result);
                callback(outcome);
            });
        }
    });
}

void NetworkClient::getPostRemixes(const juce::String& postId, ResponseCallback callback)
{
    if (!isAuthenticated())
    {
        if (callback)
            callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
        return;
    }

    Async::runVoid([this, postId, callback]() {
        juce::String endpoint = buildApiPath(("/posts/" + postId + "/remixes").toRawUTF8());
        auto result = makeRequestWithRetry(endpoint, "GET", juce::var(), true);
        Log::debug("Get post remixes response: " + juce::JSON::toString(result.data));

        if (callback)
        {
            juce::MessageManager::callAsync([callback, result]() {
                auto outcome = requestResultToOutcome(result);
                callback(outcome);
            });
        }
    });
}

void NetworkClient::getRemixSource(const juce::String& postId, ResponseCallback callback)
{
    if (!isAuthenticated())
    {
        if (callback)
            callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
        return;
    }

    Async::runVoid([this, postId, callback]() {
        juce::String endpoint = buildApiPath(("/posts/" + postId + "/remix-source").toRawUTF8());
        auto result = makeRequestWithRetry(endpoint, "GET", juce::var(), true);
        Log::debug("Get remix source response: " + juce::JSON::toString(result.data));

        if (callback)
        {
            juce::MessageManager::callAsync([callback, result]() {
                auto outcome = requestResultToOutcome(result);
                callback(outcome);
            });
        }
    });
}

void NetworkClient::createRemixPost(const juce::String& sourcePostId, const juce::String& remixType, ResponseCallback callback)
{
    if (!isAuthenticated())
    {
        if (callback)
            callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
        return;
    }

    Async::runVoid([this, sourcePostId, remixType, callback]() {
        juce::String endpoint = buildApiPath(("/posts/" + sourcePostId + "/remix").toRawUTF8());

        juce::var data = juce::var(new juce::DynamicObject());
        auto* obj = data.getDynamicObject();
        if (obj != nullptr)
        {
            obj->setProperty("remix_type", remixType);
        }

        auto result = makeRequestWithRetry(endpoint, "POST", data, true);
        Log::debug("Create remix post response: " + juce::JSON::toString(result.data));

        if (callback)
        {
            juce::MessageManager::callAsync([callback, result]() {
                auto outcome = requestResultToOutcome(result);
                callback(outcome);
            });
        }
    });
}
