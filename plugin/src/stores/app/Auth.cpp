#include "../AppStore.h"
#include "../../util/logging/Logger.h"

namespace Sidechain {
namespace Stores {

void AppStore::login(const juce::String &email, const juce::String &password) {
  if (!networkClient) {
    AuthState errorState = sliceManager.getAuthSlice()->getState();
    errorState.authError = "Network client not initialized";
    sliceManager.getAuthSlice()->setState(errorState);
    return;
  }

  // Optimistic update: show loading
  AuthState loadingState = sliceManager.getAuthSlice()->getState();
  loadingState.isAuthenticating = true;
  loadingState.authError = "";
  sliceManager.getAuthSlice()->setState(loadingState);

  networkClient->loginWithTwoFactor(email, password, [this, email](NetworkClient::LoginResult result) {
    auto authSlice = sliceManager.getAuthSlice();

    if (!result.success) {
      AuthState errorState = authSlice->getState();
      errorState.isAuthenticating = false;
      errorState.authError = result.errorMessage;
      authSlice->setState(errorState);
      return;
    }

    // Check if 2FA is required
    if (result.requires2FA) {
      AuthState twoFAState = authSlice->getState();
      twoFAState.isAuthenticating = false;
      twoFAState.is2FARequired = true;
      twoFAState.twoFactorUserId = result.userId;
      authSlice->setState(twoFAState);
      return;
    }

    // 2FA not required, proceed with login
    juce::String username = result.username.isEmpty() ? "user" : result.username;

    AuthState successState = authSlice->getState();
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
    authSlice->setState(successState);

    // Fetch user profile after successful login
    fetchUserProfile(false);

    // Start token refresh timer after successful login
    startTokenRefreshTimer();
  });
}

void AppStore::registerAccount(const juce::String &email, const juce::String &username, const juce::String &password,
                               const juce::String &displayName) {
  if (!networkClient) {
    AuthState errorState = sliceManager.getAuthSlice()->getState();
    errorState.authError = "Network client not initialized";
    sliceManager.getAuthSlice()->setState(errorState);
    return;
  }

  AuthState loadingState = sliceManager.getAuthSlice()->getState();
  loadingState.isAuthenticating = true;
  loadingState.authError = "";
  sliceManager.getAuthSlice()->setState(loadingState);

  networkClient->registerAccount(
      email, username, password, displayName,
      [this, email, username, displayName](Outcome<std::pair<juce::String, juce::String>> result) {
        auto authSlice = sliceManager.getAuthSlice();

        if (!result.isOk()) {
          AuthState errorState = authSlice->getState();
          errorState.isAuthenticating = false;
          errorState.authError = result.getError();
          authSlice->setState(errorState);
          return;
        }

        // Registration successful - extract token and userId from result pair
        auto [token, userId] = result.getValue();

        // Update auth state with user info
        AuthState successState = authSlice->getState();
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
        authSlice->setState(successState);
      });
}

void AppStore::verify2FA(const juce::String &code) {
  auto authSlice = sliceManager.getAuthSlice();
  auto currentAuth = authSlice->getState();

  if (!networkClient) {
    AuthState errorState = authSlice->getState();
    errorState.authError = "Network client not initialized";
    authSlice->setState(errorState);
    return;
  }

  if (currentAuth.twoFactorUserId.isEmpty()) {
    AuthState errorState = authSlice->getState();
    errorState.authError = "2FA not initiated";
    authSlice->setState(errorState);
    return;
  }

  AuthState verifyingState = authSlice->getState();
  verifyingState.isVerifying2FA = true;
  authSlice->setState(verifyingState);

  networkClient->verify2FALogin(
      currentAuth.twoFactorUserId, code, [this, authSlice](Outcome<std::pair<juce::String, juce::String>> result) {
        if (!result.isOk()) {
          AuthState errorState = authSlice->getState();
          errorState.isVerifying2FA = false;
          errorState.authError = result.getError();
          authSlice->setState(errorState);
          return;
        }

        // Extract token and userId from result pair
        auto [token, userId] = result.getValue();

        AuthState successState = authSlice->getState();
        successState.isVerifying2FA = false;
        successState.is2FARequired = false;
        successState.isLoggedIn = true;
        successState.authToken = token;
        successState.userId = userId;
        successState.authError = "";
        // Set token expiry to 24 hours from now (backend default)
        successState.tokenExpiresAt = juce::Time::getCurrentTime().toMilliseconds() + (24 * 60 * 60 * 1000);
        authSlice->setState(successState);

        // Fetch user profile after successful 2FA
        fetchUserProfile(false);
      });
}

void AppStore::requestPasswordReset(const juce::String &email) {
  auto authSlice = sliceManager.getAuthSlice();

  if (!networkClient) {
    AuthState errorState = authSlice->getState();
    errorState.authError = "Network client not initialized";
    authSlice->setState(errorState);
    return;
  }

  AuthState resetState = authSlice->getState();
  resetState.isResettingPassword = true;
  authSlice->setState(resetState);

  networkClient->requestPasswordReset(email, [authSlice](Outcome<juce::var> result) {
    if (!result.isOk()) {
      AuthState errorState = authSlice->getState();
      errorState.isResettingPassword = false;
      errorState.authError = result.getError();
      authSlice->setState(errorState);
      return;
    }

    AuthState doneState = authSlice->getState();
    doneState.isResettingPassword = false;
    doneState.authError = "";
    authSlice->setState(doneState);

    Util::logInfo("AppStore", "Password reset email sent successfully");
  });
}

void AppStore::resetPassword(const juce::String &token, const juce::String &newPassword) {
  auto authSlice = sliceManager.getAuthSlice();

  if (!networkClient) {
    AuthState errorState = authSlice->getState();
    errorState.authError = "Network client not initialized";
    authSlice->setState(errorState);
    return;
  }

  AuthState resetState = authSlice->getState();
  resetState.isResettingPassword = true;
  authSlice->setState(resetState);

  networkClient->resetPassword(token, newPassword, [this](Outcome<juce::var> result) {
    auto authSlicePtr = sliceManager.getAuthSlice();

    if (!result.isOk()) {
      AuthState errorState = authSlicePtr->getState();
      errorState.isResettingPassword = false;
      errorState.authError = result.getError();
      authSlicePtr->setState(errorState);
      return;
    }

    AuthState doneState = authSlicePtr->getState();
    doneState.isResettingPassword = false;
    doneState.authError = "";
    authSlicePtr->setState(doneState);

    Util::logInfo("AppStore", "Password reset successful");
  });
}

void AppStore::logout() {
  auto authSlice = sliceManager.getAuthSlice();
  auto userSlice = sliceManager.getUserSlice();

  // Stop token refresh timer
  stopTokenRefreshTimer();

  AuthState logoutState = authSlice->getState();
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
  authSlice->setState(logoutState);

  // Clear user state
  UserState clearedState;
  userSlice->setState(clearedState);
}

void AppStore::setAuthToken(const juce::String &token) {
  AuthState newState = sliceManager.getAuthSlice()->getState();
  newState.authToken = token;
  if (!token.isEmpty()) {
    newState.isLoggedIn = true;
  }
  sliceManager.getAuthSlice()->setState(newState);
}

void AppStore::refreshAuthToken() {
  auto authSlice = sliceManager.getAuthSlice();
  auto currentAuth = authSlice->getState();

  if (!networkClient) {
    AuthState errorState = authSlice->getState();
    errorState.authError = "Network client not initialized";
    authSlice->setState(errorState);
    return;
  }

  if (currentAuth.authToken.isEmpty()) {
    Util::logInfo("AppStore", "No token to refresh (token is empty)");
    return;
  }

  // Call the new refresh endpoint
  networkClient->refreshAuthToken(
      currentAuth.authToken, [authSlice](Outcome<std::pair<juce::String, juce::String>> result) {
        if (result.isOk()) {
          // Extract new token and userId
          auto [newToken, userId] = result.getValue();

          AuthState successState = authSlice->getState();
          successState.authToken = newToken;
          successState.userId = userId;
          // Reset token expiry to 24 hours from now
          successState.tokenExpiresAt = juce::Time::getCurrentTime().toMilliseconds() + (24 * 60 * 60 * 1000);
          successState.lastAuthTime = juce::Time::getCurrentTime().toMilliseconds();
          successState.authError = "";
          authSlice->setState(successState);

          Util::logInfo("AppStore", "Token refreshed successfully");
        } else {
          // Token refresh failed - likely invalid/expired token
          Util::logError("AppStore", "Token refresh failed: " + result.getError());

          // If token is truly expired, log user out
          AuthState errorState = authSlice->getState();
          errorState.authError = "Session expired - please log in again";
          errorState.isLoggedIn = false;
          errorState.authToken = "";
          errorState.userId = "";
          errorState.tokenExpiresAt = 0;
          authSlice->setState(errorState);
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
  auto authSlice = sliceManager.getAuthSlice();
  auto currentAuth = authSlice->getState();

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

} // namespace Stores
} // namespace Sidechain
