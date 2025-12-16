#include "../AppStore.h"
#include "../../util/logging/Logger.h"

namespace Sidechain {
namespace Stores {

void AppStore::login(const juce::String &email, const juce::String &password) {
  if (!networkClient) {
    updateAuthState([](AuthState &state) { state.authError = "Network client not initialized"; });
    return;
  }

  updateAuthState([](AuthState &state) {
    state.isAuthenticating = true;
    state.authError = "";
  });

  networkClient->loginWithTwoFactor(email, password, [this, email](NetworkClient::LoginResult result) {
    if (!result.success) {
      updateAuthState([result](AuthState &state) {
        state.isAuthenticating = false;
        state.authError = result.errorMessage;
      });
      return;
    }

    // Check if 2FA is required
    if (result.requires2FA) {
      updateAuthState([result](AuthState &state) {
        state.isAuthenticating = false;
        state.is2FARequired = true;
        state.twoFactorUserId = result.userId;
      });
      return;
    }

    // 2FA not required, proceed with login
    juce::String username = result.username.isEmpty() ? "user" : result.username;

    updateAuthState([result, email, username](AuthState &state) {
      state.isAuthenticating = false;
      state.is2FARequired = false;
      state.isLoggedIn = true;
      state.userId = result.userId;
      state.username = username;
      state.email = email;
      state.authToken = result.token;
      state.lastAuthTime = juce::Time::getCurrentTime().toMilliseconds();
      state.authError = "";
    });

    // Fetch user profile after successful login
    fetchUserProfile(false);
  });
}

void AppStore::registerAccount(const juce::String &email, const juce::String &username, const juce::String &password,
                               const juce::String &displayName) {
  if (!networkClient) {
    updateAuthState([](AuthState &state) { state.authError = "Network client not initialized"; });
    return;
  }

  updateAuthState([](AuthState &state) {
    state.isAuthenticating = true;
    state.authError = "";
  });

  networkClient->registerAccount(email, username, password, displayName,
                                 [this, email, username, displayName](Outcome<juce::var> result) {
                                   if (!result.isOk()) {
                                     updateAuthState([error = result.getError()](AuthState &state) {
                                       state.isAuthenticating = false;
                                       state.authError = error;
                                     });
                                     return;
                                   }

                                   // Registration successful - update auth state with user info
                                   updateAuthState([email, username, displayName](AuthState &state) {
                                     state.isAuthenticating = false;
                                     state.isLoggedIn = true;
                                     state.email = email;
                                     state.username = username;
                                     state.authError = "";
                                     state.lastAuthTime = juce::Time::getCurrentTime().toMilliseconds();
                                   });
                                 });
}

void AppStore::verify2FA(const juce::String &code) {
  if (!networkClient) {
    updateAuthState([](AuthState &state) { state.authError = "Network client not initialized"; });
    return;
  }

  auto currentState = getState();
  if (currentState.auth.twoFactorUserId.isEmpty()) {
    updateAuthState([](AuthState &state) { state.authError = "2FA not initiated"; });
    return;
  }

  updateAuthState([](AuthState &state) { state.isVerifying2FA = true; });

  networkClient->verify2FALogin(currentState.auth.twoFactorUserId, code,
                                [this, currentState](Outcome<std::pair<juce::String, juce::String>> result) {
                                  if (!result.isOk()) {
                                    updateAuthState([error = result.getError()](AuthState &state) {
                                      state.isVerifying2FA = false;
                                      state.authError = error;
                                    });
                                    return;
                                  }

                                  // Extract token and userId from result pair
                                  auto [token, userId] = result.getValue();

                                  updateAuthState([token, userId, currentState](AuthState &state) {
                                    state.isVerifying2FA = false;
                                    state.is2FARequired = false;
                                    state.isLoggedIn = true;
                                    state.authToken = token;
                                    state.userId = userId;
                                    state.authError = "";
                                  });

                                  // Fetch user profile after successful 2FA
                                  fetchUserProfile(false);
                                });
}

void AppStore::requestPasswordReset(const juce::String &email) {
  if (!networkClient) {
    updateAuthState([](AuthState &state) { state.authError = "Network client not initialized"; });
    return;
  }

  updateAuthState([](AuthState &state) { state.isResettingPassword = true; });

  networkClient->requestPasswordReset(email, [this](Outcome<juce::var> result) {
    if (!result.isOk()) {
      updateAuthState([error = result.getError()](AuthState &state) {
        state.isResettingPassword = false;
        state.authError = error;
      });
      return;
    }

    updateAuthState([](AuthState &state) {
      state.isResettingPassword = false;
      state.authError = "";
    });

    Util::logInfo("AppStore", "Password reset email sent successfully");
  });
}

void AppStore::resetPassword(const juce::String &token, const juce::String &newPassword) {
  if (!networkClient) {
    updateAuthState([](AuthState &state) { state.authError = "Network client not initialized"; });
    return;
  }

  updateAuthState([](AuthState &state) { state.isResettingPassword = true; });

  networkClient->resetPassword(token, newPassword, [this](Outcome<juce::var> result) {
    if (!result.isOk()) {
      updateAuthState([error = result.getError()](AuthState &state) {
        state.isResettingPassword = false;
        state.authError = error;
      });
      return;
    }

    updateAuthState([](AuthState &state) {
      state.isResettingPassword = false;
      state.authError = "";
    });

    Util::logInfo("AppStore", "Password reset successful");
  });
}

void AppStore::logout() {
  updateAuthState([](AuthState &state) {
    state.isLoggedIn = false;
    state.userId = "";
    state.username = "";
    state.email = "";
    state.authToken = "";
    state.refreshToken = "";
    state.is2FARequired = false;
    state.twoFactorUserId = "";
    state.authError = "";
  });

  // Clear user state
  updateUserState([](UserState &state) {
    state = UserState{}; // Reset to default state
  });
}

void AppStore::oauthCallback(const juce::String &provider, const juce::String &code) {
  // TODO: Implement OAuth flow
  updateAuthState([](AuthState &state) { state.authError = "OAuth not yet implemented"; });
}

void AppStore::setAuthToken(const juce::String &token) {
  updateAuthState([token](AuthState &state) {
    state.authToken = token;
    if (!token.isEmpty()) {
      state.isLoggedIn = true;
    }
  });
}

void AppStore::refreshAuthToken() {
  // TODO: Implement token refresh
  updateAuthState([](AuthState &state) { state.authError = "Token refresh not yet implemented"; });
}

} // namespace Stores
} // namespace Sidechain
