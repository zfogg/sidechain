//==============================================================================
// AuthClient.cpp - Authentication operations
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
void NetworkClient::registerAccount(const juce::String& email, const juce::String& username,
                                   const juce::String& password, const juce::String& displayName,
                                   AuthenticationCallback callback)
{
    Async::runVoid(
        [this, email, username, password, displayName, callback]() {
            juce::var registerData = juce::var(new juce::DynamicObject());
            registerData.getDynamicObject()->setProperty("email", email);
            registerData.getDynamicObject()->setProperty("username", username);
            registerData.getDynamicObject()->setProperty("password", password);
            registerData.getDynamicObject()->setProperty("display_name", displayName);

            auto response = makeRequest(buildApiPath("/auth/register"), "POST", registerData, false);

            juce::String token, userId, responseUsername;
            bool success = false;

            if (response.isObject())
            {
                auto authData = response.getProperty("auth", juce::var());
                if (authData.isObject())
                {
                    token = authData.getProperty("token", "").toString();
                    auto user = authData.getProperty("user", juce::var());

                    if (!token.isEmpty() && user.isObject())
                    {
                        userId = user.getProperty("id", "").toString();
                        responseUsername = user.getProperty("username", "").toString();
                        success = true;
                    }
                }
            }

            juce::MessageManager::callAsync([this, callback, token, userId, responseUsername, success]() {
                if (success)
                {
                    // Store authentication info
                    authToken = token;
                    currentUserId = userId;
                    currentUsername = responseUsername;

                    auto authResult = Outcome<std::pair<juce::String, juce::String>>::ok({token, userId});
                    callback(authResult);
                    Log::info("Account registered successfully: " + responseUsername);
                }
                else
                {
                    auto authResult = Outcome<std::pair<juce::String, juce::String>>::error("Registration failed - invalid input or username already taken");
                    callback(authResult);
                    Log::error("Account registration failed");
                }
            });
        }
    );
}

void NetworkClient::loginAccount(const juce::String& email, const juce::String& password,
                                AuthenticationCallback callback)
{
    Async::runVoid(
        [this, email, password, callback]() {
            juce::var loginData = juce::var(new juce::DynamicObject());
            loginData.getDynamicObject()->setProperty("email", email);
            loginData.getDynamicObject()->setProperty("password", password);

            auto response = makeRequest(buildApiPath("/auth/login"), "POST", loginData, false);

            juce::String token, userId, username;
            bool success = false;

            if (response.isObject())
            {
                auto authData = response.getProperty("auth", juce::var());
                if (authData.isObject())
                {
                    token = authData.getProperty("token", "").toString();
                    auto user = authData.getProperty("user", juce::var());

                    if (!token.isEmpty() && user.isObject())
                    {
                        userId = user.getProperty("id", "").toString();
                        username = user.getProperty("username", "").toString();
                        success = true;
                    }
                }
            }

            // Extract email_verified status separately
            bool emailVerified = true;
            if (response.isObject())
            {
                auto authData = response.getProperty("auth", juce::var());
                if (authData.isObject())
                {
                    auto user = authData.getProperty("user", juce::var());
                    if (user.isObject())
                    {
                        emailVerified = user.getProperty("email_verified", true).operator bool();
                    }
                }
            }

            juce::MessageManager::callAsync([this, callback, token, userId, username, success, emailVerified]() {
                if (success)
                {
                    // Store authentication info
                    authToken = token;
                    currentUserId = userId;
                    currentUsername = username;
                    currentUserEmailVerified = emailVerified;

                    auto authResult = Outcome<std::pair<juce::String, juce::String>>::ok({token, userId});
                    callback(authResult);
                    Log::info("Login successful: " + username);
                }
                else
                {
                    auto authResult = Outcome<std::pair<juce::String, juce::String>>::error("Login failed - invalid credentials");
                    callback(authResult);
                    Log::warn("Login failed");
                }
            });
        }
    );
}

void NetworkClient::setAuthenticationCallback(AuthenticationCallback callback)
{
    authCallback = callback;
}

void NetworkClient::requestPasswordReset(const juce::String& email, ResponseCallback callback)
{
    Async::runVoid([this, email, callback]() {
        juce::var resetData = juce::var(new juce::DynamicObject());
        resetData.getDynamicObject()->setProperty("email", email);

        auto result = makeRequestWithRetry(buildApiPath("/auth/reset-password"), "POST", resetData, false);
        Log::debug("Password reset request response: " + juce::JSON::toString(result.data));

        if (callback)
        {
            juce::MessageManager::callAsync([callback, result]() {
                auto outcome = requestResultToOutcome(result);
                callback(outcome);
            });
        }
    });
}

void NetworkClient::resetPassword(const juce::String& token, const juce::String& newPassword, ResponseCallback callback)
{
    Async::runVoid([this, token, newPassword, callback]() {
        juce::var resetData = juce::var(new juce::DynamicObject());
        resetData.getDynamicObject()->setProperty("token", token);
        resetData.getDynamicObject()->setProperty("new_password", newPassword);

        auto result = makeRequestWithRetry(buildApiPath("/auth/reset-password/confirm"), "POST", resetData, false);
        Log::debug("Password reset confirm response: " + juce::JSON::toString(result.data));

        if (callback)
        {
            juce::MessageManager::callAsync([callback, result]() {
                auto outcome = requestResultToOutcome(result);
                callback(outcome);
            });
        }
    });
}
