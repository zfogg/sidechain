#pragma once

#include "../network/NetworkClient.h"
#include "../util/logging/Logger.h"
#include "Store.h"
#include <JuceHeader.h>

namespace Sidechain {
namespace Stores {

/**
 * AuthState - Immutable authentication and session state
 */
struct AuthState {
  // Authentication
  bool isLoggedIn = false;
  juce::String userId;
  juce::String username;
  juce::String email;
  juce::String authToken;
  juce::String refreshToken;

  // Login/Signup state
  bool isAuthenticating = false;
  bool is2FARequired = false;
  bool isVerifying2FA = false;
  juce::String twoFactorUserId; // Temporary user ID during 2FA flow

  // Password reset
  bool isResettingPassword = false;

  // Error handling
  juce::String error;
  int64_t lastAuthTime = 0;

  bool operator==(const AuthState &other) const {
    return isLoggedIn == other.isLoggedIn && userId == other.userId && username == other.username &&
           email == other.email && authToken == other.authToken && refreshToken == other.refreshToken &&
           isAuthenticating == other.isAuthenticating && is2FARequired == other.is2FARequired &&
           isVerifying2FA == other.isVerifying2FA && isResettingPassword == other.isResettingPassword;
  }
};

/**
 * AuthStore - Reactive store for authentication and session management
 *
 * Handles:
 * - User login/signup with email and password
 * - OAuth authentication flows
 * - Two-factor authentication
 * - Password reset
 * - Token management
 *
 * Usage:
 *   auto& authStore = AuthStore::getInstance();
 *   authStore.setNetworkClient(networkClient);
 *
 *   auto unsubscribe = authStore.subscribe([](const AuthState& state) {
 *       if (state.isLoggedIn) {
 *           showMainUI();
 *       } else if (!state.error.isEmpty()) {
 *           showError(state.error);
 *       }
 *   });
 *
 *   // Login
 *   authStore.login("user@example.com", "password");
 *
 *   // If 2FA required, verify code
 *   authStore.verify2FA("123456");
 */
class AuthStore : public Store<AuthState> {
public:
  /**
   * Get singleton instance
   */
  static AuthStore &getInstance() {
    static AuthStore instance;
    return instance;
  }

  /**
   * Set the network client for API calls
   */
  void setNetworkClient(NetworkClient *client) {
    networkClient = client;
  }

  //==============================================================================
  // Authentication Methods

  /**
   * Login with email and password
   * Triggers isAuthenticating flag, then either isLoggedIn or error state
   * If 2FA required, sets is2FARequired and twoFactorUserId for next step
   */
  void login(const juce::String &email, const juce::String &password);

  /**
   * Register a new account
   */
  void registerAccount(const juce::String &email, const juce::String &username, const juce::String &password,
                       const juce::String &displayName);

  /**
   * Verify two-factor authentication code
   * Must call login() or oauthCallback() first
   */
  void verify2FA(const juce::String &code);

  /**
   * Request password reset email
   */
  void requestPasswordReset(const juce::String &email);

  /**
   * Reset password with token from email
   */
  void resetPassword(const juce::String &token, const juce::String &newPassword);

  /**
   * Logout current user
   */
  void logout();

  /**
   * OAuth callback - called when OAuth provider returns authorization code
   */
  void oauthCallback(const juce::String &provider, const juce::String &code);

  /**
   * Set auth token directly (from saved session)
   */
  void setAuthToken(const juce::String &token);

  /**
   * Refresh auth token
   */
  void refreshAuthToken();

private:
  AuthStore() : Store<AuthState>() {}

  NetworkClient *networkClient = nullptr;

  // Helper methods
  void markAuthenticating();
  void clearError();
};

} // namespace Stores
} // namespace Sidechain
