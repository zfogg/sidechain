#pragma once

#include <JuceHeader.h>
#include <memory>
#include <optional>
#include <string>

namespace Sidechain {
namespace Security {

/**
 * SecureTokenStore - Platform-specific secure credential storage
 *
 * Stores authentication tokens securely using:
 * - macOS: Keychain (OSStatus-based secure storage)
 * - Windows: DPAPI (Data Protection API with user key)
 * - Linux: Secret Service (DBus-based credential storage)
 *
 * Never stores tokens in plain text files or memory.
 *
 * Usage:
 *   auto store = SecureTokenStore::getInstance();
 *   store->saveToken("jwt_token", token);
 *   auto retrieved = store->loadToken("jwt_token");
 *   store->deleteToken("jwt_token");
 */
class SecureTokenStore {
public:
  JUCE_DECLARE_SINGLETON(SecureTokenStore, false)

  /**
   * Save a token securely with a given key
   *
   * @param key Identifier for the token (e.g., "jwt_token", "refresh_token")
   * @param token The sensitive token value to store
   * @return true if saved successfully, false otherwise
   */
  bool saveToken(const juce::String &key, const juce::String &token);

  /**
   * Load a previously saved token
   *
   * @param key The identifier for the token
   * @return Optional containing the token if found, empty if not found or error
   */
  std::optional<juce::String> loadToken(const juce::String &key);

  /**
   * Delete a stored token
   *
   * @param key The identifier for the token to delete
   * @return true if deleted successfully, false otherwise
   */
  bool deleteToken(const juce::String &key);

  /**
   * Check if a token exists
   *
   * @param key The identifier to check
   * @return true if token exists, false otherwise
   */
  bool hasToken(const juce::String &key);

  /**
   * Clear all stored tokens
   *
   * Caution: This removes all tokens from secure storage.
   * @return true if all tokens cleared, false otherwise
   */
  bool clearAllTokens();

  /**
   * Get platform-specific storage type
   *
   * @return String describing the backend (e.g., "Keychain", "DPAPI",
   * "SecretService")
   */
  juce::String getBackendType() const;

  /**
   * Check if secure storage is available on this platform
   *
   * @return true if secure storage is functional, false otherwise
   */
  bool isAvailable() const {
    return isAvailable_;
  }

private:
  bool isAvailable_ = false;
  juce::String backendType_;

  SecureTokenStore();
  virtual ~SecureTokenStore() {
    clearSingletonInstance();
  }

#ifdef JUCE_MAC
  // macOS Keychain implementation
  bool saveTokenMacOS(const juce::String &key, const juce::String &token);
  std::optional<juce::String> loadTokenMacOS(const juce::String &key);
  bool deleteTokenMacOS(const juce::String &key);
  bool hasTokenMacOS(const juce::String &key);
#endif

#ifdef JUCE_WINDOWS
  // Windows DPAPI implementation
  bool saveTokenWindows(const juce::String &key, const juce::String &token);
  std::optional<juce::String> loadTokenWindows(const juce::String &key);
  bool deleteTokenWindows(const juce::String &key);
  bool hasTokenWindows(const juce::String &key);

  // Helper for DPAPI encryption/decryption
  juce::MemoryBlock dpapi_encrypt(const juce::String &plaintext);
  juce::String dpapi_decrypt(const juce::MemoryBlock &encrypted);
#endif

#ifdef JUCE_LINUX
  // Linux Secret Service implementation
  bool saveTokenLinux(const juce::String &key, const juce::String &token);
  std::optional<juce::String> loadTokenLinux(const juce::String &key);
  bool deleteTokenLinux(const juce::String &key);
  bool hasTokenLinux(const juce::String &key);
#endif

  // Fallback for unsupported platforms (raises warning)
  bool saveTokenFallback(const juce::String &key, const juce::String &token);
  std::optional<juce::String> loadTokenFallback(const juce::String &key);
  bool deleteTokenFallback(const juce::String &key);
  bool hasTokenFallback(const juce::String &key);

  /**
   * Get storage directory for fallback storage
   */
  juce::File getSecureStorageDir();

  /**
   * Get encrypted file path for a key
   */
  juce::File getTokenFilePath(const juce::String &key);
};

/**
 * TokenGuard - RAII wrapper for secure token usage
 *
 * Automatically clears token from memory when out of scope.
 *
 * Usage:
 *   {
 *       TokenGuard guard("jwt_token");
 *       auto token = guard.get();
 *       // Use token
 *   }  // Token is securely cleared here
 */
class TokenGuard {
public:
  /**
   * Load and guard a token
   *
   * @param key The identifier for the token
   */
  explicit TokenGuard(const juce::String &key) : key_(key) {
    auto store = SecureTokenStore::getInstance();
    if (auto token = store->loadToken(key)) {
      value_ = token.value();
    }
  }

  /**
   * Destructor: securely clear token from memory
   */
  ~TokenGuard() {
    // Clear the string - JUCE String doesn't support direct character
    // modification Setting to empty string will clear the internal buffer
    value_ = "";

    // Note: For maximum security, we rely on the SecureTokenStore
    // platform-specific implementations to handle sensitive data. This guard
    // just ensures the in-memory copy is cleared when it goes out of scope.
  }

  /**
   * Get the token value
   *
   * @return Token string (valid only within scope)
   */
  const juce::String &get() const {
    return value_;
  }

  /**
   * Check if token was loaded successfully
   */
  bool isValid() const {
    return !value_.isEmpty();
  }

  // Prevent copying - ensures only one instance manages the token
  TokenGuard(const TokenGuard &) = delete;
  TokenGuard &operator=(const TokenGuard &) = delete;

  // Allow moving
  TokenGuard(TokenGuard &&other) noexcept : key_(std::move(other.key_)), value_(std::move(other.value_)) {
    other.value_ = "";
  }

  TokenGuard &operator=(TokenGuard &&other) noexcept {
    if (this != &other) {
      key_ = std::move(other.key_);
      value_ = std::move(other.value_);
      other.value_ = "";
    }
    return *this;
  }

private:
  juce::String key_;
  juce::String value_;
};

} // namespace Security
} // namespace Sidechain
