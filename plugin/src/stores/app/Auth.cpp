#include "../AppStore.h"
#include "../../util/logging/Logger.h"
#include "../../util/rx/JuceScheduler.h"
#include <rxcpp/rx.hpp>

namespace Sidechain {
namespace Stores {

void AppStore::login(const juce::String &email, const juce::String &password) {
  if (!networkClient) {
    AuthState errorState = stateManager.auth->getState();
    errorState.authError = "Network client not initialized";
    stateManager.auth->setState(errorState);
    return;
  }

  // Optimistic update: show loading
  AuthState loadingState = stateManager.auth->getState();
  loadingState.isAuthenticating = true;
  loadingState.authError = "";
  stateManager.auth->setState(loadingState);

  networkClient->loginWithTwoFactor(email, password, [this, email](NetworkClient::LoginResult result) {
    auto authState = stateManager.auth;

    if (!result.success) {
      AuthState errorState = authState->getState();
      errorState.isAuthenticating = false;
      errorState.authError = result.errorMessage;
      authState->setState(errorState);
      return;
    }

    // Check if 2FA is required
    if (result.requires2FA) {
      AuthState twoFAState = authState->getState();
      twoFAState.isAuthenticating = false;
      twoFAState.is2FARequired = true;
      twoFAState.twoFactorUserId = result.userId;
      authState->setState(twoFAState);
      return;
    }

    // 2FA not required, proceed with login
    juce::String username = result.username.isEmpty() ? "user" : result.username;

    AuthState successState = authState->getState();
    successState.isAuthenticating = false;
    successState.is2FARequired = false;
    successState.isLoggedIn = true;
    successState.userId = result.userId;
    successState.username = username;
    successState.email = email;
    successState.authToken = result.token;
    successState.lastAuthTime = juce::Time::getCurrentTime().toMilliseconds();
    // Set token expiry to 24 hours from now (backend default)
    successState.tokenExpiresAt = juce::Time::getCurrentTime().toMilliseconds() + (24 * 60 * 60 * 1000);
    successState.authError = "";
    authState->setState(successState);

    // Fetch user profile after successful login
    fetchUserProfile(false);

    // Start token refresh timer after successful login
    startTokenRefreshTimer();
  });
}

void AppStore::registerAccount(const juce::String &email, const juce::String &username, const juce::String &password,
                               const juce::String &displayName) {
  if (!networkClient) {
    AuthState errorState = stateManager.auth->getState();
    errorState.authError = "Network client not initialized";
    stateManager.auth->setState(errorState);
    return;
  }

  AuthState loadingState = stateManager.auth->getState();
  loadingState.isAuthenticating = true;
  loadingState.authError = "";
  stateManager.auth->setState(loadingState);

  networkClient->registerAccount(
      email, username, password, displayName,
      [this, email, username, displayName](Outcome<std::pair<juce::String, juce::String>> result) {
        auto authState = stateManager.auth;

        if (!result.isOk()) {
          AuthState errorState = authState->getState();
          errorState.isAuthenticating = false;
          errorState.authError = result.getError();
          authState->setState(errorState);
          return;
        }

        // Registration successful - extract token and userId from result pair
        auto [token, userId] = result.getValue();

        // Update auth state with user info
        AuthState successState = authState->getState();
        successState.isAuthenticating = false;
        successState.isLoggedIn = true;
        successState.userId = userId;
        successState.username = username;
        successState.email = email;
        successState.authToken = token;
        successState.authError = "";
        successState.lastAuthTime = juce::Time::getCurrentTime().toMilliseconds();
        // Set token expiry to 24 hours from now (backend default)
        successState.tokenExpiresAt = juce::Time::getCurrentTime().toMilliseconds() + (24 * 60 * 60 * 1000);
        authState->setState(successState);
      });
}

void AppStore::verify2FA(const juce::String &code) {
  auto authState = stateManager.auth;
  auto currentAuth = authState->getState();

  if (!networkClient) {
    AuthState errorState = authState->getState();
    errorState.authError = "Network client not initialized";
    authState->setState(errorState);
    return;
  }

  if (currentAuth.twoFactorUserId.isEmpty()) {
    AuthState errorState = authState->getState();
    errorState.authError = "2FA not initiated";
    authState->setState(errorState);
    return;
  }

  AuthState verifyingState = authState->getState();
  verifyingState.isVerifying2FA = true;
  authState->setState(verifyingState);

  networkClient->verify2FALogin(
      currentAuth.twoFactorUserId, code, [this, authState](Outcome<std::pair<juce::String, juce::String>> result) {
        if (!result.isOk()) {
          AuthState errorState = authState->getState();
          errorState.isVerifying2FA = false;
          errorState.authError = result.getError();
          authState->setState(errorState);
          return;
        }

        // Extract token and userId from result pair
        auto [token, userId] = result.getValue();

        AuthState successState = authState->getState();
        successState.isVerifying2FA = false;
        successState.is2FARequired = false;
        successState.isLoggedIn = true;
        successState.authToken = token;
        successState.userId = userId;
        successState.authError = "";
        // Set token expiry to 24 hours from now (backend default)
        successState.tokenExpiresAt = juce::Time::getCurrentTime().toMilliseconds() + (24 * 60 * 60 * 1000);
        authState->setState(successState);

        // Fetch user profile after successful 2FA
        fetchUserProfile(false);
      });
}

void AppStore::requestPasswordReset(const juce::String &email) {
  auto authState = stateManager.auth;

  if (!networkClient) {
    AuthState errorState = authState->getState();
    errorState.authError = "Network client not initialized";
    authState->setState(errorState);
    return;
  }

  AuthState resetState = authState->getState();
  resetState.isResettingPassword = true;
  authState->setState(resetState);

  networkClient->requestPasswordReset(email, [authState](Outcome<juce::var> result) {
    if (!result.isOk()) {
      AuthState errorState = authState->getState();
      errorState.isResettingPassword = false;
      errorState.authError = result.getError();
      authState->setState(errorState);
      return;
    }

    AuthState doneState = authState->getState();
    doneState.isResettingPassword = false;
    doneState.authError = "";
    authState->setState(doneState);

    Util::logInfo("AppStore", "Password reset email sent successfully");
  });
}

void AppStore::resetPassword(const juce::String &token, const juce::String &newPassword) {
  auto authState = stateManager.auth;

  if (!networkClient) {
    AuthState errorState = authState->getState();
    errorState.authError = "Network client not initialized";
    authState->setState(errorState);
    return;
  }

  AuthState resetState = authState->getState();
  resetState.isResettingPassword = true;
  authState->setState(resetState);

  networkClient->resetPassword(token, newPassword, [this](Outcome<juce::var> result) {
    auto authStatePtr = stateManager.auth;

    if (!result.isOk()) {
      AuthState errorState = authStatePtr->getState();
      errorState.isResettingPassword = false;
      errorState.authError = result.getError();
      authStatePtr->setState(errorState);
      return;
    }

    AuthState doneState = authStatePtr->getState();
    doneState.isResettingPassword = false;
    doneState.authError = "";
    authStatePtr->setState(doneState);

    Util::logInfo("AppStore", "Password reset successful");
  });
}

void AppStore::logout() {
  auto authState = stateManager.auth;
  auto userState = stateManager.user;

  // Stop token refresh timer
  stopTokenRefreshTimer();

  AuthState logoutState = authState->getState();
  logoutState.isLoggedIn = false;
  logoutState.userId = "";
  logoutState.username = "";
  logoutState.email = "";
  logoutState.authToken = "";
  logoutState.refreshToken = "";
  logoutState.tokenExpiresAt = 0;
  logoutState.is2FARequired = false;
  logoutState.twoFactorUserId = "";
  logoutState.authError = "";
  authState->setState(logoutState);

  // Clear user state
  UserState clearedState;
  userState->setState(clearedState);
}

void AppStore::setAuthToken(const juce::String &token) {
  AuthState newState = stateManager.auth->getState();
  newState.authToken = token;
  if (!token.isEmpty()) {
    newState.isLoggedIn = true;
  }
  stateManager.auth->setState(newState);
}

void AppStore::refreshAuthToken() {
  auto authState = stateManager.auth;
  auto currentAuth = authState->getState();

  if (!networkClient) {
    AuthState errorState = authState->getState();
    errorState.authError = "Network client not initialized";
    authState->setState(errorState);
    return;
  }

  if (currentAuth.authToken.isEmpty()) {
    Util::logInfo("AppStore", "No token to refresh (token is empty)");
    return;
  }

  // Call the new refresh endpoint
  networkClient->refreshAuthToken(
      currentAuth.authToken, [authState](Outcome<std::pair<juce::String, juce::String>> result) {
        if (result.isOk()) {
          // Extract new token and userId
          auto [newToken, userId] = result.getValue();

          AuthState successState = authState->getState();
          successState.authToken = newToken;
          successState.userId = userId;
          // Reset token expiry to 24 hours from now
          successState.tokenExpiresAt = juce::Time::getCurrentTime().toMilliseconds() + (24 * 60 * 60 * 1000);
          successState.lastAuthTime = juce::Time::getCurrentTime().toMilliseconds();
          successState.authError = "";
          authState->setState(successState);

          Util::logInfo("AppStore", "Token refreshed successfully");
        } else {
          // Token refresh failed - likely invalid/expired token
          Util::logError("AppStore", "Token refresh failed: " + result.getError());

          // If token is truly expired, log user out
          AuthState errorState = authState->getState();
          errorState.authError = "Session expired - please log in again";
          errorState.isLoggedIn = false;
          errorState.authToken = "";
          errorState.userId = "";
          errorState.tokenExpiresAt = 0;
          authState->setState(errorState);
        }
      });
}

void AppStore::startTokenRefreshTimer() {
  if (!tokenRefreshTimer_) {
    tokenRefreshTimer_ = std::make_unique<TokenRefreshTimer>(this);
  }

  // Check every 30 minutes if token needs refresh
  tokenRefreshTimer_->startTimer(30 * 60 * 1000);
  Util::logInfo("AppStore", "Token refresh timer started (checks every 30 minutes)");
}

void AppStore::stopTokenRefreshTimer() {
  if (tokenRefreshTimer_) {
    tokenRefreshTimer_->stopTimer();
    Util::logInfo("AppStore", "Token refresh timer stopped");
  }
}

void AppStore::checkAndRefreshToken() {
  auto authState = stateManager.auth;
  auto currentAuth = authState->getState();

  // Only refresh if logged in and token needs refresh
  if (!currentAuth.isLoggedIn) {
    return;
  }

  if (currentAuth.shouldRefreshToken()) {
    Util::logInfo("AppStore", "Token needs refresh (< 1 hour remaining), refreshing automatically");
    refreshAuthToken();
  } else if (currentAuth.isTokenExpired()) {
    Util::logError("AppStore", "Token already expired, logging out");
    logout();
  } else {
    auto timeRemaining = currentAuth.tokenExpiresAt - juce::Time::getCurrentTime().toMilliseconds();
    auto hoursRemaining = timeRemaining / (60 * 60 * 1000);
    Util::logInfo("AppStore", "Token still valid (" + juce::String(hoursRemaining) + " hours remaining)");
  }
}

// ==============================================================================
// Reactive Auth Observables (Phase 7)
// ==============================================================================

rxcpp::observable<AppStore::LoginResult> AppStore::loginObservable(const juce::String &email,
                                                                   const juce::String &password) {
  return rxcpp::sources::create<LoginResult>([this, email, password](auto observer) {
           if (!networkClient) {
             Util::logError("AppStore", "Network client not initialized");
             observer.on_error(std::make_exception_ptr(std::runtime_error("Network client not initialized")));
             return;
           }

           Util::logDebug("AppStore", "Login via observable for: " + email);

           networkClient->loginWithTwoFactor(
               email, password, [this, observer, email](NetworkClient::LoginResult result) {
                 LoginResult loginResult;

                 if (!result.success) {
                   loginResult.success = false;
                   loginResult.errorMessage = result.errorMessage;
                   observer.on_next(loginResult);
                   observer.on_completed();
                   return;
                 }

                 if (result.requires2FA) {
                   loginResult.success = true;
                   loginResult.requires2FA = true;
                   loginResult.userId = result.userId;

                   // Update state for 2FA
                   AuthState twoFAState = stateManager.auth->getState();
                   twoFAState.is2FARequired = true;
                   twoFAState.twoFactorUserId = result.userId;
                   stateManager.auth->setState(twoFAState);

                   observer.on_next(loginResult);
                   observer.on_completed();
                   return;
                 }

                 // Success - update state
                 loginResult.success = true;
                 loginResult.requires2FA = false;
                 loginResult.userId = result.userId;
                 loginResult.username = result.username;
                 loginResult.token = result.token;

                 AuthState successState = stateManager.auth->getState();
                 successState.isAuthenticating = false;
                 successState.is2FARequired = false;
                 successState.isLoggedIn = true;
                 successState.userId = result.userId;
                 successState.username = result.username.isEmpty() ? "user" : result.username;
                 successState.email = email;
                 successState.authToken = result.token;
                 successState.lastAuthTime = juce::Time::getCurrentTime().toMilliseconds();
                 successState.tokenExpiresAt = juce::Time::getCurrentTime().toMilliseconds() + (24 * 60 * 60 * 1000);
                 successState.authError = "";
                 stateManager.auth->setState(successState);

                 fetchUserProfile(false);
                 startTokenRefreshTimer();

                 observer.on_next(loginResult);
                 observer.on_completed();
               });
         })
      .observe_on(Rx::observe_on_juce_thread());
}

rxcpp::observable<AppStore::LoginResult> AppStore::registerAccountObservable(const juce::String &email,
                                                                             const juce::String &username,
                                                                             const juce::String &password,
                                                                             const juce::String &displayName) {
  return rxcpp::sources::create<LoginResult>([this, email, username, password, displayName](auto observer) {
           if (!networkClient) {
             Util::logError("AppStore", "Network client not initialized");
             observer.on_error(std::make_exception_ptr(std::runtime_error("Network client not initialized")));
             return;
           }

           Util::logDebug("AppStore", "Register account via observable for: " + email);

           networkClient->registerAccount(
               email, username, password, displayName,
               [this, observer, email, username](Outcome<std::pair<juce::String, juce::String>> result) {
                 if (!result.isOk()) {
                   LoginResult loginResult;
                   loginResult.success = false;
                   loginResult.errorMessage = result.getError();
                   observer.on_next(loginResult);
                   observer.on_completed();
                   return;
                 }

                 auto [token, userId] = result.getValue();

                 LoginResult loginResult;
                 loginResult.success = true;
                 loginResult.userId = userId;
                 loginResult.username = username;
                 loginResult.token = token;

                 AuthState successState = stateManager.auth->getState();
                 successState.isAuthenticating = false;
                 successState.isLoggedIn = true;
                 successState.userId = userId;
                 successState.username = username;
                 successState.email = email;
                 successState.authToken = token;
                 successState.authError = "";
                 successState.lastAuthTime = juce::Time::getCurrentTime().toMilliseconds();
                 successState.tokenExpiresAt = juce::Time::getCurrentTime().toMilliseconds() + (24 * 60 * 60 * 1000);
                 stateManager.auth->setState(successState);

                 observer.on_next(loginResult);
                 observer.on_completed();
               });
         })
      .observe_on(Rx::observe_on_juce_thread());
}

rxcpp::observable<AppStore::LoginResult> AppStore::verify2FAObservable(const juce::String &code) {
  return rxcpp::sources::create<LoginResult>([this, code](auto observer) {
           auto authState = stateManager.auth;
           auto currentAuth = authState->getState();

           if (!networkClient) {
             Util::logError("AppStore", "Network client not initialized");
             observer.on_error(std::make_exception_ptr(std::runtime_error("Network client not initialized")));
             return;
           }

           if (currentAuth.twoFactorUserId.isEmpty()) {
             observer.on_error(std::make_exception_ptr(std::runtime_error("2FA not initiated")));
             return;
           }

           Util::logDebug("AppStore", "Verify 2FA via observable");

           networkClient->verify2FALogin(currentAuth.twoFactorUserId, code,
                                         [this, observer](Outcome<std::pair<juce::String, juce::String>> result) {
                                           if (!result.isOk()) {
                                             LoginResult loginResult;
                                             loginResult.success = false;
                                             loginResult.errorMessage = result.getError();
                                             observer.on_next(loginResult);
                                             observer.on_completed();
                                             return;
                                           }

                                           auto [token, userId] = result.getValue();

                                           LoginResult loginResult;
                                           loginResult.success = true;
                                           loginResult.userId = userId;
                                           loginResult.token = token;

                                           AuthState successState = stateManager.auth->getState();
                                           successState.isVerifying2FA = false;
                                           successState.is2FARequired = false;
                                           successState.isLoggedIn = true;
                                           successState.authToken = token;
                                           successState.userId = userId;
                                           successState.authError = "";
                                           successState.tokenExpiresAt =
                                               juce::Time::getCurrentTime().toMilliseconds() + (24 * 60 * 60 * 1000);
                                           stateManager.auth->setState(successState);

                                           fetchUserProfile(false);

                                           observer.on_next(loginResult);
                                           observer.on_completed();
                                         });
         })
      .observe_on(Rx::observe_on_juce_thread());
}

rxcpp::observable<int> AppStore::requestPasswordResetObservable(const juce::String &email) {
  return rxcpp::sources::create<int>([this, email](auto observer) {
           if (!networkClient) {
             Util::logError("AppStore", "Network client not initialized");
             observer.on_error(std::make_exception_ptr(std::runtime_error("Network client not initialized")));
             return;
           }

           Util::logDebug("AppStore", "Request password reset via observable for: " + email);

           networkClient->requestPasswordReset(email, [observer](Outcome<juce::var> result) {
             if (result.isOk()) {
               Util::logInfo("AppStore", "Password reset email sent successfully");
               observer.on_next(0);
               observer.on_completed();
             } else {
               Util::logError("AppStore", "Failed to request password reset: " + result.getError());
               observer.on_error(std::make_exception_ptr(std::runtime_error(result.getError().toStdString())));
             }
           });
         })
      .observe_on(Rx::observe_on_juce_thread());
}

rxcpp::observable<int> AppStore::resetPasswordObservable(const juce::String &token, const juce::String &newPassword) {
  return rxcpp::sources::create<int>([this, token, newPassword](auto observer) {
           if (!networkClient) {
             Util::logError("AppStore", "Network client not initialized");
             observer.on_error(std::make_exception_ptr(std::runtime_error("Network client not initialized")));
             return;
           }

           Util::logDebug("AppStore", "Reset password via observable");

           networkClient->resetPassword(token, newPassword, [observer](Outcome<juce::var> result) {
             if (result.isOk()) {
               Util::logInfo("AppStore", "Password reset successful");
               observer.on_next(0);
               observer.on_completed();
             } else {
               Util::logError("AppStore", "Failed to reset password: " + result.getError());
               observer.on_error(std::make_exception_ptr(std::runtime_error(result.getError().toStdString())));
             }
           });
         })
      .observe_on(Rx::observe_on_juce_thread());
}

rxcpp::observable<int> AppStore::refreshAuthTokenObservable() {
  return rxcpp::sources::create<int>([this](auto observer) {
           auto authState = stateManager.auth;
           auto currentAuth = authState->getState();

           if (!networkClient) {
             Util::logError("AppStore", "Network client not initialized");
             observer.on_error(std::make_exception_ptr(std::runtime_error("Network client not initialized")));
             return;
           }

           if (currentAuth.authToken.isEmpty()) {
             Util::logInfo("AppStore", "No token to refresh (token is empty)");
             observer.on_next(0);
             observer.on_completed();
             return;
           }

           Util::logDebug("AppStore", "Refresh auth token via observable");

           networkClient->refreshAuthToken(
               currentAuth.authToken, [this, observer](Outcome<std::pair<juce::String, juce::String>> result) {
                 if (result.isOk()) {
                   auto [newToken, userId] = result.getValue();

                   AuthState successState = stateManager.auth->getState();
                   successState.authToken = newToken;
                   successState.userId = userId;
                   successState.tokenExpiresAt = juce::Time::getCurrentTime().toMilliseconds() + (24 * 60 * 60 * 1000);
                   successState.lastAuthTime = juce::Time::getCurrentTime().toMilliseconds();
                   successState.authError = "";
                   stateManager.auth->setState(successState);

                   Util::logInfo("AppStore", "Token refreshed successfully");
                   observer.on_next(0);
                   observer.on_completed();
                 } else {
                   Util::logError("AppStore", "Token refresh failed: " + result.getError());
                   observer.on_error(std::make_exception_ptr(std::runtime_error(result.getError().toStdString())));
                 }
               });
         })
      .observe_on(Rx::observe_on_juce_thread());
}

} // namespace Stores
} // namespace Sidechain
