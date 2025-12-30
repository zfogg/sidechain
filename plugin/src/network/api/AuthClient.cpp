// ==============================================================================
// AuthClient.cpp - Authentication operations
// Part of NetworkClient implementation split
// ==============================================================================

#include "../../util/Async.h"
#include "../../util/Log.h"
#include "../NetworkClient.h"
#include "Common.h"

using namespace Sidechain::Network::Api;

// ==============================================================================
// Helper to safely get string from nlohmann::json
static juce::String getJsonString(const nlohmann::json &json, const std::string &key,
                                  const juce::String &defaultVal = {}) {
  if (json.is_object() && json.contains(key) && json[key].is_string()) {
    return juce::String(json[key].get<std::string>());
  }
  return defaultVal;
}

// Helper to safely get bool from nlohmann::json
static bool getJsonBool(const nlohmann::json &json, const std::string &key, bool defaultVal = false) {
  if (json.is_object() && json.contains(key) && json[key].is_boolean()) {
    return json[key].get<bool>();
  }
  return defaultVal;
}

// Helper to safely get int from nlohmann::json
static int getJsonInt(const nlohmann::json &json, const std::string &key, int defaultVal = 0) {
  if (json.is_object() && json.contains(key) && json[key].is_number_integer()) {
    return json[key].get<int>();
  }
  return defaultVal;
}

// Helper to safely get int64 from nlohmann::json
static int64_t getJsonInt64(const nlohmann::json &json, const std::string &key, int64_t defaultVal = 0) {
  if (json.is_object() && json.contains(key) && json[key].is_number()) {
    return json[key].get<int64_t>();
  }
  return defaultVal;
}

// Helper to safely get nested object from nlohmann::json
static nlohmann::json getJsonObject(const nlohmann::json &json, const std::string &key) {
  if (json.is_object() && json.contains(key) && json[key].is_object()) {
    return json[key];
  }
  return nlohmann::json();
}

// Helper to safely get array from nlohmann::json
static nlohmann::json getJsonArray(const nlohmann::json &json, const std::string &key) {
  if (json.is_object() && json.contains(key) && json[key].is_array()) {
    return json[key];
  }
  return nlohmann::json::array();
}

// ==============================================================================
void NetworkClient::registerAccount(const juce::String &email, const juce::String &username,
                                    const juce::String &password, const juce::String &displayName,
                                    AuthenticationCallback callback) {
  Async::runVoid([this, email, username, password, displayName, callback]() {
    nlohmann::json registerData = {{"email", email.toStdString()},
                                   {"username", username.toStdString()},
                                   {"password", password.toStdString()},
                                   {"display_name", displayName.toStdString()}};

    auto response = makeRequest(buildApiPath("/auth/register"), "POST", registerData, false);

    juce::String token, userId, responseUsername;
    bool success = false;

    if (response.is_object()) {
      auto authData = getJsonObject(response, "auth");
      if (authData.is_object()) {
        token = getJsonString(authData, "token");
        auto user = getJsonObject(authData, "user");

        if (!token.isEmpty() && user.is_object()) {
          userId = getJsonString(user, "id");
          responseUsername = getJsonString(user, "username");
          success = true;
        }
      }
    }

    juce::MessageManager::callAsync([this, callback, token, userId, responseUsername, success]() {
      if (success) {
        // Store authentication info
        authToken = token;
        currentUserId = userId;
        currentUsername = responseUsername;

        auto authResult = Outcome<std::pair<juce::String, juce::String>>::ok({token, userId});
        callback(authResult);
        Log::info("Account registered successfully: " + responseUsername);
      } else {
        auto authResult = Outcome<std::pair<juce::String, juce::String>>::error(
            "Registration failed - invalid input or username already taken");
        callback(authResult);
        Log::error("Account registration failed");
      }
    });
  });
}

void NetworkClient::loginAccount(const juce::String &email, const juce::String &password,
                                 AuthenticationCallback callback) {
  Async::runVoid([this, email, password, callback]() {
    nlohmann::json loginData = {{"email", email.toStdString()}, {"password", password.toStdString()}};

    auto response = makeRequest(buildApiPath("/auth/login"), "POST", loginData, false);

    juce::String token, userId, username;
    bool success = false;

    if (response.is_object()) {
      auto authData = getJsonObject(response, "auth");
      if (authData.is_object()) {
        token = getJsonString(authData, "token");
        auto user = getJsonObject(authData, "user");

        if (!token.isEmpty() && user.is_object()) {
          userId = getJsonString(user, "id");
          username = getJsonString(user, "username");
          success = true;
        }
      }
    }

    // Extract email_verified status separately
    bool emailVerified = true;
    if (response.is_object()) {
      auto authData = getJsonObject(response, "auth");
      if (authData.is_object()) {
        auto user = getJsonObject(authData, "user");
        if (user.is_object()) {
          emailVerified = getJsonBool(user, "email_verified", true);
        }
      }
    }

    juce::MessageManager::callAsync([this, callback, token, userId, username, success, emailVerified]() {
      if (success) {
        // Store authentication info
        authToken = token;
        currentUserId = userId;
        currentUsername = username;
        currentUserEmailVerified = emailVerified;

        auto authResult = Outcome<std::pair<juce::String, juce::String>>::ok({token, userId});
        callback(authResult);
        Log::info("Login successful: " + username);
      } else {
        auto authResult = Outcome<std::pair<juce::String, juce::String>>::error("Login failed - invalid credentials");
        callback(authResult);
        Log::warn("Login failed");
      }
    });
  });
}

void NetworkClient::setAuthenticationCallback(AuthenticationCallback callback) {
  authCallback = callback;
}

void NetworkClient::requestPasswordReset(const juce::String &email, ResponseCallback callback) {
  Async::runVoid([this, email, callback]() {
    nlohmann::json resetData = {{"email", email.toStdString()}};

    auto result = makeRequestWithRetry(buildApiPath("/auth/reset-password"), "POST", resetData, false);
    Log::debug("Password reset request response: " + juce::String(result.data.dump()));

    if (callback) {
      juce::MessageManager::callAsync([callback, result]() {
        auto outcome = requestResultToOutcome(result);
        callback(outcome);
      });
    }
  });
}

void NetworkClient::resetPassword(const juce::String &token, const juce::String &newPassword,
                                  ResponseCallback callback) {
  Async::runVoid([this, token, newPassword, callback]() {
    nlohmann::json resetData = {{"token", token.toStdString()}, {"new_password", newPassword.toStdString()}};

    auto result = makeRequestWithRetry(buildApiPath("/auth/reset-password/confirm"), "POST", resetData, false);
    Log::debug("Password reset confirm response: " + juce::String(result.data.dump()));

    if (callback) {
      juce::MessageManager::callAsync([callback, result]() {
        auto outcome = requestResultToOutcome(result);
        callback(outcome);
      });
    }
  });
}

// ==============================================================================
// Two-Factor Authentication

void NetworkClient::loginWithTwoFactor(const juce::String &email, const juce::String &password,
                                       LoginCallback callback) {
  Async::runVoid([this, email, password, callback]() {
    nlohmann::json loginData = {{"email", email.toStdString()}, {"password", password.toStdString()}};

    auto response = makeRequest(buildApiPath("/auth/login"), "POST", loginData, false);

    LoginResult result;

    if (response.is_object()) {
      // Check if 2FA is required
      if (getJsonBool(response, "requires_2fa")) {
        result.requires2FA = true;
        result.userId = getJsonString(response, "user_id");
        result.twoFactorType = getJsonString(response, "two_factor_type", "totp");
        Log::info("Login requires 2FA verification (type: " + result.twoFactorType + ")");
      } else {
        // Normal login success
        auto authData = getJsonObject(response, "auth");
        if (authData.is_object()) {
          result.token = getJsonString(authData, "token");
          auto user = getJsonObject(authData, "user");

          if (!result.token.isEmpty() && user.is_object()) {
            result.success = true;
            result.userId = getJsonString(user, "id");
            result.username = getJsonString(user, "username");
          }
        }
      }
    }

    if (!result.success && !result.requires2FA) {
      result.errorMessage = "Login failed - invalid credentials";
    }

    juce::MessageManager::callAsync([this, callback, result]() {
      if (result.success) {
        // Store authentication info
        authToken = result.token;
        currentUserId = result.userId;
        currentUsername = result.username;
        Log::info("Login successful: " + result.username);
      }
      callback(result);
    });
  });
}

void NetworkClient::verify2FALogin(const juce::String &userId, const juce::String &code,
                                   AuthenticationCallback callback) {
  Async::runVoid([this, userId, code, callback]() {
    nlohmann::json verifyData = {{"user_id", userId.toStdString()}, {"code", code.toStdString()}};

    auto response = makeRequest(buildApiPath("/auth/2fa/login"), "POST", verifyData, false);

    juce::String token, returnedUserId, username;
    bool success = false;

    if (response.is_object()) {
      auto authData = getJsonObject(response, "auth");
      if (authData.is_object()) {
        token = getJsonString(authData, "token");
        auto user = getJsonObject(authData, "user");

        if (!token.isEmpty() && user.is_object()) {
          returnedUserId = getJsonString(user, "id");
          username = getJsonString(user, "username");
          success = true;
        }
      }
    }

    juce::MessageManager::callAsync([this, callback, token, returnedUserId, username, success]() {
      if (success) {
        authToken = token;
        currentUserId = returnedUserId;
        currentUsername = username;

        auto authResult = Outcome<std::pair<juce::String, juce::String>>::ok({token, returnedUserId});
        callback(authResult);
        Log::info("2FA verification successful: " + username);
      } else {
        auto authResult = Outcome<std::pair<juce::String, juce::String>>::error("Invalid 2FA code");
        callback(authResult);
        Log::warn("2FA verification failed");
      }
    });
  });
}

void NetworkClient::get2FAStatus(TwoFactorStatusCallback callback) {
  Async::runVoid([this, callback]() {
    auto result = makeRequestWithRetry(buildApiPath("/auth/2fa/status"), "GET", nlohmann::json(), true);

    TwoFactorStatus status;
    bool success = false;

    if (result.success && result.data.is_object()) {
      status.enabled = getJsonBool(result.data, "enabled");
      status.type = getJsonString(result.data, "type");
      status.backupCodesRemaining = getJsonInt(result.data, "backup_codes_remaining");
      success = true;
    }

    juce::MessageManager::callAsync([callback, status, success, result]() {
      if (success) {
        callback(Outcome<TwoFactorStatus>::ok(status));
      } else {
        callback(Outcome<TwoFactorStatus>::error(result.getUserFriendlyError()));
      }
    });
  });
}

void NetworkClient::enable2FA(const juce::String &password, const juce::String &type, TwoFactorSetupCallback callback) {
  Async::runVoid([this, password, type, callback]() {
    nlohmann::json enableData = {{"password", password.toStdString()}, {"type", type.toStdString()}};

    auto result = makeRequestWithRetry(buildApiPath("/auth/2fa/enable"), "POST", enableData, true);

    TwoFactorSetup setup;
    bool success = false;

    if (result.success && result.data.is_object()) {
      setup.type = getJsonString(result.data, "type", "totp");
      setup.secret = getJsonString(result.data, "secret");
      setup.qrCodeUrl = getJsonString(result.data, "qr_code_url");
      setup.counter = static_cast<uint64_t>(getJsonInt64(result.data, "counter"));

      auto codes = getJsonArray(result.data, "backup_codes");
      if (codes.is_array()) {
        for (const auto &code : codes) {
          if (code.is_string()) {
            setup.backupCodes.add(juce::String(code.get<std::string>()));
          }
        }
      }
      success = !setup.secret.isEmpty();
    }

    juce::MessageManager::callAsync([callback, setup, success, result]() {
      if (success) {
        callback(Outcome<TwoFactorSetup>::ok(setup));
        Log::info("2FA setup initiated (type: " + setup.type + ")");
      } else {
        callback(Outcome<TwoFactorSetup>::error(result.getUserFriendlyError()));
      }
    });
  });
}

void NetworkClient::verify2FASetup(const juce::String &code, ResponseCallback callback) {
  Async::runVoid([this, code, callback]() {
    nlohmann::json verifyData = {{"code", code.toStdString()}};

    auto result = makeRequestWithRetry(buildApiPath("/auth/2fa/verify"), "POST", verifyData, true);

    if (callback) {
      juce::MessageManager::callAsync([callback, result]() {
        auto outcome = requestResultToOutcome(result);
        if (outcome.isOk()) {
          Log::info("2FA enabled successfully");
        }
        callback(outcome);
      });
    }
  });
}

void NetworkClient::disable2FA(const juce::String &codeOrPassword, ResponseCallback callback) {
  Async::runVoid([this, codeOrPassword, callback]() {
    nlohmann::json disableData;
    // The backend accepts either code or password
    // If it looks like a 6-digit code or backup code format, send as code
    if (codeOrPassword.length() == 6 || codeOrPassword.contains("-")) {
      disableData["code"] = codeOrPassword.toStdString();
    } else {
      disableData["password"] = codeOrPassword.toStdString();
    }

    auto result = makeRequestWithRetry(buildApiPath("/auth/2fa/disable"), "POST", disableData, true);

    if (callback) {
      juce::MessageManager::callAsync([callback, result]() {
        auto outcome = requestResultToOutcome(result);
        if (outcome.isOk()) {
          Log::info("2FA disabled successfully");
        }
        callback(outcome);
      });
    }
  });
}

void NetworkClient::regenerateBackupCodes(const juce::String &code, ResponseCallback callback) {
  Async::runVoid([this, code, callback]() {
    nlohmann::json regenData = {{"code", code.toStdString()}};

    auto result = makeRequestWithRetry(buildApiPath("/auth/2fa/backup-codes"), "POST", regenData, true);

    if (callback) {
      juce::MessageManager::callAsync([callback, result]() {
        auto outcome = requestResultToOutcome(result);
        if (outcome.isOk()) {
          Log::info("Backup codes regenerated");
        }
        callback(outcome);
      });
    }
  });
}

// ==============================================================================
// Token Refresh
// ==============================================================================

void NetworkClient::refreshAuthToken(const juce::String &currentToken, AuthenticationCallback callback) {
  Async::runVoid([this, currentToken, callback]() {
    // Create request with current token in Authorization header
    nlohmann::json refreshData = {{"token", currentToken.toStdString()}};

    // Don't use current authToken for this request - use the provided token
    auto tempToken = authToken;
    authToken = currentToken;

    auto result = makeRequestWithRetry(buildApiPath(Constants::Endpoints::AUTH_REFRESH), "POST", refreshData, false);

    // Restore previous token in case refresh fails
    if (!result.success) {
      authToken = tempToken;
    }

    juce::String newToken, userId;
    bool success = false;

    if (result.success && result.data.is_object()) {
      newToken = getJsonString(result.data, "token");
      auto user = getJsonObject(result.data, "user");

      if (!newToken.isEmpty() && user.is_object()) {
        userId = getJsonString(user, "id");
        success = true;
      }
    }

    juce::MessageManager::callAsync([this, callback, newToken, userId, success, result]() {
      if (success) {
        // Update stored auth token with new one
        authToken = newToken;
        currentUserId = userId;

        auto authResult = Outcome<std::pair<juce::String, juce::String>>::ok({newToken, userId});
        callback(authResult);
        Log::info("Auth token refreshed successfully");
      } else {
        juce::String errorMsg = "Token refresh failed";
        if (result.data.is_object()) {
          auto error = getJsonString(result.data, "error");
          if (!error.isEmpty()) {
            errorMsg = error;
          }
        }

        auto authResult = Outcome<std::pair<juce::String, juce::String>>::error(errorMsg);
        callback(authResult);
        Log::error("Token refresh failed: " + errorMsg);
      }
    });
  });
}
