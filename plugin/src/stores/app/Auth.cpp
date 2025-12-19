#include "../AppStore.h"
#include "../../util/logging/Logger.h"

namespace Sidechain {
namespace Stores {

void AppStore::login(const juce::String &email, const juce::String &password) {
  if (!networkClient) {
    sliceManager.getAuthSlice()->dispatch([](AuthState &state) { state.authError = "Network client not initialized"; });
    return;
  }

  // Optimistic update: show loading
  sliceManager.getAuthSlice()->dispatch([](AuthState &state) {
    state.isAuthenticating = true;
    state.authError = "";
  });

  networkClient->loginWithTwoFactor(email, password, [this, email](NetworkClient::LoginResult result) {
    auto authSlice = sliceManager.getAuthSlice();

    if (!result.success) {
      authSlice->dispatch([result](AuthState &state) {
        state.isAuthenticating = false;
        state.authError = result.errorMessage;
      });
      return;
    }

    // Check if 2FA is required
    if (result.requires2FA) {
      authSlice->dispatch([result](AuthState &state) {
        state.isAuthenticating = false;
        state.is2FARequired = true;
        state.twoFactorUserId = result.userId;
      });
      return;
    }

    // 2FA not required, proceed with login
    juce::String username = result.username.isEmpty() ? "user" : result.username;

    authSlice->dispatch([result, email, username](AuthState &state) {
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
    sliceManager.getAuthSlice()->dispatch([](AuthState &state) { state.authError = "Network client not initialized"; });
    return;
  }

  sliceManager.getAuthSlice()->dispatch([](AuthState &state) {
    state.isAuthenticating = true;
    state.authError = "";
  });

  networkClient->registerAccount(
      email, username, password, displayName,
      [this, email, username, displayName](Outcome<std::pair<juce::String, juce::String>> result) {
        auto authSlice = sliceManager.getAuthSlice();

        if (!result.isOk()) {
          authSlice->dispatch([error = result.getError()](AuthState &state) {
            state.isAuthenticating = false;
            state.authError = error;
          });
          return;
        }

        // Registration successful - extract token and userId from result pair
        auto [token, userId] = result.getValue();

        // Update auth state with user info
        authSlice->dispatch([token, userId, email, username, displayName](AuthState &state) {
          state.isAuthenticating = false;
          state.isLoggedIn = true;
          state.userId = userId;
          state.username = username;
          state.email = email;
          state.authToken = token;
          state.authError = "";
          state.lastAuthTime = juce::Time::getCurrentTime().toMilliseconds();
        });
      });
}

void AppStore::verify2FA(const juce::String &code) {
  auto authSlice = sliceManager.getAuthSlice();
  auto currentAuth = authSlice->getState();

  if (!networkClient) {
    authSlice->dispatch([](AuthState &state) { state.authError = "Network client not initialized"; });
    return;
  }

  if (currentAuth.twoFactorUserId.isEmpty()) {
    authSlice->dispatch([](AuthState &state) { state.authError = "2FA not initiated"; });
    return;
  }

  authSlice->dispatch([](AuthState &state) { state.isVerifying2FA = true; });

  networkClient->verify2FALogin(currentAuth.twoFactorUserId, code,
                                [this](Outcome<std::pair<juce::String, juce::String>> result) {
                                  auto authSlicePtr = sliceManager.getAuthSlice();

                                  if (!result.isOk()) {
                                    authSlice->dispatch([error = result.getError()](AuthState &state) {
                                      state.isVerifying2FA = false;
                                      state.authError = error;
                                    });
                                    return;
                                  }

                                  // Extract token and userId from result pair
                                  auto [token, userId] = result.getValue();

                                  authSlice->dispatch([token, userId](AuthState &state) {
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
  auto authSlice = sliceManager.getAuthSlice();

  if (!networkClient) {
    authSlice->dispatch([](AuthState &state) { state.authError = "Network client not initialized"; });
    return;
  }

  authSlice->dispatch([](AuthState &state) { state.isResettingPassword = true; });

  networkClient->requestPasswordReset(email, [this](Outcome<juce::var> result) {
    auto authSlice = sliceManager.getAuthSlice();

    if (!result.isOk()) {
      authSlice->dispatch([error = result.getError()](AuthState &state) {
        state.isResettingPassword = false;
        state.authError = error;
      });
      return;
    }

    authSlice->dispatch([](AuthState &state) {
      state.isResettingPassword = false;
      state.authError = "";
    });

    Util::logInfo("AppStore", "Password reset email sent successfully");
  });
}

void AppStore::resetPassword(const juce::String &token, const juce::String &newPassword) {
  auto authSlice = sliceManager.getAuthSlice();

  if (!networkClient) {
    authSlice->dispatch([](AuthState &state) { state.authError = "Network client not initialized"; });
    return;
  }

  authSlice->dispatch([](AuthState &state) { state.isResettingPassword = true; });

  networkClient->resetPassword(token, newPassword, [this](Outcome<juce::var> result) {
    auto authSlice = sliceManager.getAuthSlice();

    if (!result.isOk()) {
      authSlice->dispatch([error = result.getError()](AuthState &state) {
        state.isResettingPassword = false;
        state.authError = error;
      });
      return;
    }

    authSlice->dispatch([](AuthState &state) {
      state.isResettingPassword = false;
      state.authError = "";
    });

    Util::logInfo("AppStore", "Password reset successful");
  });
}

void AppStore::logout() {
  auto authSlice = sliceManager.getAuthSlice();
  auto userSlice = sliceManager.getUserSlice();

  authSlice->dispatch([](AuthState &state) {
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
  userSlice->dispatch([](UserState &state) {
    state = UserState{}; // Reset to default state
  });
}

void AppStore::oauthCallback(const juce::String & [[maybe_unused]] provider, const juce::String & [[maybe_unused]] code) {
  // TODO: Implement OAuth flow
  sliceManager.getAuthSlice()->dispatch([](AuthState &state) { state.authError = "OAuth not yet implemented"; });
}

void AppStore::setAuthToken(const juce::String &token) {
  sliceManager.getAuthSlice()->dispatch([token](AuthState &state) {
    state.authToken = token;
    if (!token.isEmpty()) {
      state.isLoggedIn = true;
    }
  });
}

void AppStore::refreshAuthToken() {
  // TODO: Implement token refresh
  sliceManager.getAuthSlice()->dispatch(
      [](AuthState &state) { state.authError = "Token refresh not yet implemented"; });
}

} // namespace Stores
} // namespace Sidechain
