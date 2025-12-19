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
void NetworkClient::registerAccount(const juce::String &email, const juce::String &username,
                                    const juce::String &password, const juce::String &displayName,
                                    AuthenticationCallback callback) {
  Async::runVoid([this, email, username, password, displayName, callback]() {
    juce::var registerData = juce::var(new juce::DynamicObject());
    registerData.getDynamicObject()->setProperty("email", email);
    registerData.getDynamicObject()->setProperty("username", username);
    registerData.getDynamicObject()->setProperty("password", password);
    registerData.getDynamicObject()->setProperty("display_name", displayName);

    auto response = makeRequest(buildApiPath("/auth/register"), "POST", registerData, false);

    juce::String token, userId, responseUsername;
    bool success = false;

    if (response.isObject()) {
      auto authData = response.getProperty("auth", juce::var());
      if (authData.isObject()) {
        token = authData.getProperty("token", "").toString();
        auto user = authData.getProperty("user", juce::var());

        if (!token.isEmpty() && user.isObject()) {
          userId = user.getProperty("id", "").toString();
          responseUsername = user.getProperty("username", "").toString();
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
    juce::var loginData = juce::var(new juce::DynamicObject());
    loginData.getDynamicObject()->setProperty("email", email);
    loginData.getDynamicObject()->setProperty("password", password);

    auto response = makeRequest(buildApiPath("/auth/login"), "POST", loginData, false);

    juce::String token, userId, username;
    bool success = false;

    if (response.isObject()) {
      auto authData = response.getProperty("auth", juce::var());
      if (authData.isObject()) {
        token = authData.getProperty("token", "").toString();
        auto user = authData.getProperty("user", juce::var());

        if (!token.isEmpty() && user.isObject()) {
          userId = user.getProperty("id", "").toString();
          username = user.getProperty("username", "").toString();
          success = true;
        }
      }
    }

    // Extract email_verified status separately
    bool emailVerified = true;
    if (response.isObject()) {
      auto authData = response.getProperty("auth", juce::var());
      if (authData.isObject()) {
        auto user = authData.getProperty("user", juce::var());
        if (user.isObject()) {
          emailVerified = user.getProperty("email_verified", true).operator bool();
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
    juce::var resetData = juce::var(new juce::DynamicObject());
    resetData.getDynamicObject()->setProperty("email", email);

    auto result = makeRequestWithRetry(buildApiPath("/auth/reset-password"), "POST", resetData, false);
    Log::debug("Password reset request response: " + juce::JSON::toString(result.data));

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
    juce::var resetData = juce::var(new juce::DynamicObject());
    resetData.getDynamicObject()->setProperty("token", token);
    resetData.getDynamicObject()->setProperty("new_password", newPassword);

    auto result = makeRequestWithRetry(buildApiPath("/auth/reset-password/confirm"), "POST", resetData, false);
    Log::debug("Password reset confirm response: " + juce::JSON::toString(result.data));

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
    juce::var loginData = juce::var(new juce::DynamicObject());
    loginData.getDynamicObject()->setProperty("email", email);
    loginData.getDynamicObject()->setProperty("password", password);

    auto response = makeRequest(buildApiPath("/auth/login"), "POST", loginData, false);

    LoginResult result;

    if (response.isObject()) {
      // Check if 2FA is required
      if (response.getProperty("requires_2fa", false).operator bool()) {
        result.requires2FA = true;
        result.userId = response.getProperty("user_id", "").toString();
        result.twoFactorType = response.getProperty("two_factor_type", "totp").toString();
        Log::info("Login requires 2FA verification (type: " + result.twoFactorType + ")");
      } else {
        // Normal login success
        auto authData = response.getProperty("auth", juce::var());
        if (authData.isObject()) {
          result.token = authData.getProperty("token", "").toString();
          auto user = authData.getProperty("user", juce::var());

          if (!result.token.isEmpty() && user.isObject()) {
            result.success = true;
            result.userId = user.getProperty("id", "").toString();
            result.username = user.getProperty("username", "").toString();
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
    juce::var verifyData = juce::var(new juce::DynamicObject());
    verifyData.getDynamicObject()->setProperty("user_id", userId);
    verifyData.getDynamicObject()->setProperty("code", code);

    auto response = makeRequest(buildApiPath("/auth/2fa/login"), "POST", verifyData, false);

    juce::String token, returnedUserId, username;
    bool success = false;

    if (response.isObject()) {
      auto authData = response.getProperty("auth", juce::var());
      if (authData.isObject()) {
        token = authData.getProperty("token", "").toString();
        auto user = authData.getProperty("user", juce::var());

        if (!token.isEmpty() && user.isObject()) {
          returnedUserId = user.getProperty("id", "").toString();
          username = user.getProperty("username", "").toString();
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
    auto result = makeRequestWithRetry(buildApiPath("/auth/2fa/status"), "GET", juce::var(), true);

    TwoFactorStatus status;
    bool success = false;

    if (result.success && result.data.isObject()) {
      status.enabled = result.data.getProperty("enabled", false).operator bool();
      status.type = result.data.getProperty("type", "").toString();
      status.backupCodesRemaining = static_cast<int>(result.data.getProperty("backup_codes_remaining", 0));
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
    juce::var enableData = juce::var(new juce::DynamicObject());
    enableData.getDynamicObject()->setProperty("password", password);
    enableData.getDynamicObject()->setProperty("type", type);

    auto result = makeRequestWithRetry(buildApiPath("/auth/2fa/enable"), "POST", enableData, true);

    TwoFactorSetup setup;
    bool success = false;

    if (result.success && result.data.isObject()) {
      setup.type = result.data.getProperty("type", "totp").toString();
      setup.secret = result.data.getProperty("secret", "").toString();
      setup.qrCodeUrl = result.data.getProperty("qr_code_url", "").toString();
      setup.counter = static_cast<uint64_t>(result.data.getProperty("counter", 0).operator int64());

      auto codes = result.data.getProperty("backup_codes", juce::var());
      if (codes.isArray()) {
        for (int i = 0; i < codes.size(); ++i) {
          setup.backupCodes.add(codes[i].toString());
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
    juce::var verifyData = juce::var(new juce::DynamicObject());
    verifyData.getDynamicObject()->setProperty("code", code);

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
    juce::var disableData = juce::var(new juce::DynamicObject());
    // The backend accepts either code or password
    // If it looks like a 6-digit code or backup code format, send as code
    if (codeOrPassword.length() == 6 || codeOrPassword.contains("-")) {
      disableData.getDynamicObject()->setProperty("code", codeOrPassword);
    } else {
      disableData.getDynamicObject()->setProperty("password", codeOrPassword);
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
    juce::var regenData = juce::var(new juce::DynamicObject());
    regenData.getDynamicObject()->setProperty("code", code);

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
