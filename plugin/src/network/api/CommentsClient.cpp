//==============================================================================
// CommentsClient.cpp - Comment operations
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
void NetworkClient::getComments(const juce::String& postId, int limit, int offset, CommentsListCallback callback)
{
    if (callback == nullptr)
        return;

    juce::String endpoint = buildApiPath("/posts") + "/" + postId + "/comments?limit=" + juce::String(limit)
                          + "&offset=" + juce::String(offset);

    Async::runVoid([this, endpoint, callback]() {
        auto result = makeRequestWithRetry(endpoint, "GET", juce::var(), true);

        int totalCount = 0;
        juce::var comments;

        if (result.isSuccess() && result.data.isObject())
        {
            comments = result.data.getProperty("comments", juce::var());
            totalCount = static_cast<int>(result.data.getProperty("total_count", 0));
        }

        juce::MessageManager::callAsync([callback, result, comments, totalCount]() {
            if (result.isSuccess())
                callback(Outcome<std::pair<juce::var, int>>::ok({comments, totalCount}));
            else
                callback(Outcome<std::pair<juce::var, int>>::error(result.getUserFriendlyError()));
        });
    });
}

void NetworkClient::createComment(const juce::String& postId, const juce::String& content,
                                  const juce::String& parentId, CommentCallback callback)
{
    if (!isAuthenticated())
    {
        if (callback)
            callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
        return;
    }

    Async::runVoid([this, postId, content, parentId, callback]() {
        juce::var data = juce::var(new juce::DynamicObject());
        data.getDynamicObject()->setProperty("content", content);

        if (parentId.isNotEmpty())
            data.getDynamicObject()->setProperty("parent_id", parentId);

        juce::String endpoint = buildApiPath("/posts") + "/" + postId + "/comments";
        auto result = makeRequestWithRetry(endpoint, "POST", data, true);
        Log::debug("Create comment response: " + juce::JSON::toString(result.data));

        if (callback)
        {
            juce::MessageManager::callAsync([callback, result]() {
                auto outcome = requestResultToOutcome(result);
                callback(outcome);
            });
        }
    });
}

void NetworkClient::getCommentReplies(const juce::String& commentId, int limit, int offset, CommentsListCallback callback)
{
    if (callback == nullptr)
        return;

    juce::String endpoint = buildApiPath("/comments") + "/" + commentId + "/replies?limit=" + juce::String(limit)
                          + "&offset=" + juce::String(offset);

    Async::runVoid([this, endpoint, callback]() {
        auto result = makeRequestWithRetry(endpoint, "GET", juce::var(), true);

        int totalCount = 0;
        juce::var replies;

        if (result.isSuccess() && result.data.isObject())
        {
            replies = result.data.getProperty("replies", juce::var());
            totalCount = static_cast<int>(result.data.getProperty("total_count", 0));
        }

        juce::MessageManager::callAsync([callback, result, replies, totalCount]() {
            if (result.isSuccess())
                callback(Outcome<std::pair<juce::var, int>>::ok({replies, totalCount}));
            else
                callback(Outcome<std::pair<juce::var, int>>::error(result.getUserFriendlyError()));
        });
    });
}

void NetworkClient::updateComment(const juce::String& commentId, const juce::String& content, CommentCallback callback)
{
    if (!isAuthenticated())
    {
        if (callback)
            callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
        return;
    }

    Async::runVoid([this, commentId, content, callback]() {
        juce::var data = juce::var(new juce::DynamicObject());
        data.getDynamicObject()->setProperty("content", content);

        juce::String endpoint = buildApiPath("/comments") + "/" + commentId;
        auto result = makeRequestWithRetry(endpoint, "PUT", data, true);
        Log::debug("Update comment response: " + juce::JSON::toString(result.data));

        if (callback)
        {
            juce::MessageManager::callAsync([callback, result]() {
                auto outcome = requestResultToOutcome(result);
                callback(outcome);
            });
        }
    });
}

void NetworkClient::deleteComment(const juce::String& commentId, ResponseCallback callback)
{
    if (!isAuthenticated())
    {
        if (callback)
            callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
        return;
    }

    Async::runVoid([this, commentId, callback]() {
        juce::String endpoint = buildApiPath("/comments") + "/" + commentId;
        auto result = makeRequestWithRetry(endpoint, "DELETE", juce::var(), true);
        Log::debug("Delete comment response: " + juce::JSON::toString(result.data));

        if (callback)
        {
            juce::MessageManager::callAsync([callback, result]() {
                auto outcome = requestResultToOutcome(result);
                callback(outcome);
            });
        }
    });
}

void NetworkClient::likeComment(const juce::String& commentId, ResponseCallback callback)
{
    if (!isAuthenticated())
    {
        if (callback)
            callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
        return;
    }

    Async::runVoid([this, commentId, callback]() {
        juce::String endpoint = buildApiPath("/comments") + "/" + commentId + "/like";
        auto result = makeRequestWithRetry(endpoint, "POST", juce::var(), true);
        Log::debug("Like comment response: " + juce::JSON::toString(result.data));

        if (callback)
        {
            juce::MessageManager::callAsync([callback, result]() {
                auto outcome = requestResultToOutcome(result);
                callback(outcome);
            });
        }
    });
}

void NetworkClient::unlikeComment(const juce::String& commentId, ResponseCallback callback)
{
    if (!isAuthenticated())
    {
        if (callback)
            callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
        return;
    }

    Async::runVoid([this, commentId, callback]() {
        juce::String endpoint = buildApiPath("/comments") + "/" + commentId + "/like";
        auto result = makeRequestWithRetry(endpoint, "DELETE", juce::var(), true);
        Log::debug("Unlike comment response: " + juce::JSON::toString(result.data));

        if (callback)
        {
            juce::MessageManager::callAsync([callback, result]() {
                auto outcome = requestResultToOutcome(result);
                callback(outcome);
            });
        }
    });
}

void NetworkClient::reportComment(const juce::String& commentId, const juce::String& reason, const juce::String& description, ResponseCallback callback)
{
    if (!isAuthenticated())
    {
        if (callback)
            callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
        return;
    }

    Async::runVoid([this, commentId, reason, description, callback]() {
        juce::String endpoint = buildApiPath("/comments") + "/" + commentId + "/report";
        juce::var data = juce::var(new juce::DynamicObject());
        auto* obj = data.getDynamicObject();
        if (obj != nullptr)
        {
            obj->setProperty("reason", reason);
            if (description.isNotEmpty())
                obj->setProperty("description", description);
        }

        auto result = makeRequestWithRetry(endpoint, "POST", data, true);
        Log::debug("Report comment response: " + juce::JSON::toString(result.data));

        if (callback)
        {
            juce::MessageManager::callAsync([callback, result]() {
                auto outcome = requestResultToOutcome(result);
                callback(outcome);
            });
        }
    });
}
