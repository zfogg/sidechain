#include "AuthStore.h"

namespace Sidechain {
namespace Stores {

void AuthStore::login(const juce::String &email, const juce::String &password) {
  if (!networkClient) {
    updateState([](AuthState &state) { state.error = "Network client not initialized"; });
    return;
  }

  // Mark as authenticating
  markAuthenticating();

  networkClient->loginWithTwoFactor(email, password, [this, email](NetworkClient::LoginResult result) {
    if (!result.success) {
      updateState([result](AuthState &state) {
        state.isAuthenticating = false;
        state.error = result.errorMessage;
      });
      return;
    }

    // Check if 2FA is required
    if (result.requires2FA) {
      updateState([result](AuthState &state) {
        state.isAuthenticating = false;
        state.is2FARequired = true;
        state.twoFactorUserId = result.userId;
      });
      return;
    }

    // 2FA not required, proceed with login
    // Get username from auth token
    juce::String username = result.username.isEmpty() ? "user" : result.username;

    updateState([result, email, username](AuthState &state) {
      state.isAuthenticating = false;
      state.is2FARequired = false;
      state.isLoggedIn = true;
      state.userId = result.userId;
      state.username = username;
      state.email = email;
      state.authToken = result.token;
      state.lastAuthTime = juce::Time::getCurrentTime().toMilliseconds();
      state.error = "";
    });
  });
}

void AuthStore::registerAccount(const juce::String &email, const juce::String &username, const juce::String &password,
                                const juce::String &displayName) {
  if (!networkClient) {
    updateState([](AuthState &state) { state.error = "Network client not initialized"; });
    return;
  }

  markAuthenticating();

  // TODO: Implement registration with correct NetworkClient method
  updateState([](AuthState &state) {
    state.isAuthenticating = false;
    state.error = "Registration not yet implemented";
  });
}

void AuthStore::verify2FA(const juce::String &code) {
  if (!networkClient) {
    updateState([](AuthState &state) { state.error = "Network client not initialized"; });
    return;
  }

  AuthState currentState = getState();
  if (currentState.twoFactorUserId.isEmpty()) {
    updateState([](AuthState &state) { state.error = "2FA not initiated"; });
    return;
  }

  updateState([](AuthState &state) { state.isVerifying2FA = true; });

  networkClient->verify2FALogin(currentState.twoFactorUserId, code,
                                [this, currentState](Outcome<std::pair<juce::String, juce::String>> result) {
                                  if (!result.isOk()) {
                                    updateState([error = result.getError()](AuthState &state) {
                                      state.isVerifying2FA = false;
                                      state.error = error;
                                    });
                                    return;
                                  }

                                  // Extract token and userId from result pair
                                  auto [token, userId] = result.getValue();

                                  updateState([token, userId, currentState](AuthState &state) {
                                    state.isVerifying2FA = false;
                                    state.is2FARequired = false;
                                    state.isLoggedIn = true;
                                    state.authToken = token;
                                    state.userId = userId;
                                    state.error = "";
                                  });
                                });
}

void AuthStore::requestPasswordReset(const juce::String &email) {
  if (!networkClient) {
    updateState([](AuthState &state) { state.error = "Network client not initialized"; });
    return;
  }

  updateState([](AuthState &state) { state.isResettingPassword = true; });
  // TODO: Implement password reset
  updateState([](AuthState &state) {
    state.isResettingPassword = false;
    state.error = "";
  });
}

void AuthStore::resetPassword(const juce::String &token, const juce::String &newPassword) {
  // TODO: Implement password reset with token
  updateState([](AuthState &state) { state.error = "Password reset not yet implemented"; });
}

void AuthStore::logout() {
  updateState([](AuthState &state) {
    state.isLoggedIn = false;
    state.userId = "";
    state.username = "";
    state.email = "";
    state.authToken = "";
    state.refreshToken = "";
    state.is2FARequired = false;
    state.twoFactorUserId = "";
    state.error = "";
  });
}

void AuthStore::oauthCallback(const juce::String &provider, const juce::String &code) {
  // TODO: Implement OAuth flow
  updateState([](AuthState &state) { state.error = "OAuth not yet implemented"; });
}

void AuthStore::setAuthToken(const juce::String &token) {
  updateState([token](AuthState &state) {
    state.authToken = token;
    if (!token.isEmpty()) {
      state.isLoggedIn = true;
    }
  });
}

void AuthStore::refreshAuthToken() {
  // TODO: Implement token refresh
  updateState([](AuthState &state) { state.error = "Token refresh not yet implemented"; });
}

void AuthStore::markAuthenticating() {
  updateState([](AuthState &state) {
    state.isAuthenticating = true;
    state.error = "";
  });
}

void AuthStore::clearError() {
  updateState([](AuthState &state) { state.error = ""; });
}

} // namespace Stores
} // namespace Sidechain
