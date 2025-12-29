#include "SecureTokenStore.h"
#include <JuceHeader.h>

// Include platform-specific headers after JUCE
// Use a workaround to avoid Point naming conflict
#ifdef JUCE_MAC
#define Point MacPoint
#define Rect MacRect
#include <Security/Security.h>
#undef Point
#undef Rect
#endif

#ifdef JUCE_WINDOWS
#include <wincrypt.h>
#include <windows.h>
#pragma comment(lib, "crypt32.lib")
#endif

#ifdef JUCE_LINUX
#include <sys/stat.h>
#endif

namespace Sidechain {
namespace Security {

// Static instance definition (JUCE singleton implementation)
JUCE_IMPLEMENT_SINGLETON(SecureTokenStore)

SecureTokenStore::SecureTokenStore() {
#ifdef JUCE_MAC
  isAvailable_ = true;
  backendType_ = "Keychain";
#elif JUCE_WINDOWS
  isAvailable_ = true;
  backendType_ = "DPAPI";
#elif JUCE_LINUX
  isAvailable_ = true;
  backendType_ = "SecretService";
#else
  isAvailable_ = false;
  backendType_ = "Unsupported";
#endif
}

bool SecureTokenStore::saveToken(const juce::String &key, const juce::String &token) {
  if (!isAvailable_)
    return saveTokenFallback(key, token);

#ifdef JUCE_MAC
  return saveTokenMacOS(key, token);
#elif JUCE_WINDOWS
  return saveTokenWindows(key, token);
#elif JUCE_LINUX
  return saveTokenLinux(key, token);
#endif
  return false;
}

std::optional<juce::String> SecureTokenStore::loadToken(const juce::String &key) {
  if (!isAvailable_)
    return loadTokenFallback(key);

#ifdef JUCE_MAC
  return loadTokenMacOS(key);
#elif JUCE_WINDOWS
  return loadTokenWindows(key);
#elif JUCE_LINUX
  return loadTokenLinux(key);
#endif
  return std::nullopt;
}

bool SecureTokenStore::deleteToken(const juce::String &key) {
  if (!isAvailable_)
    return deleteTokenFallback(key);

#ifdef JUCE_MAC
  return deleteTokenMacOS(key);
#elif JUCE_WINDOWS
  return deleteTokenWindows(key);
#elif JUCE_LINUX
  return deleteTokenLinux(key);
#endif
  return false;
}

bool SecureTokenStore::hasToken(const juce::String &key) {
  if (!isAvailable_)
    return hasTokenFallback(key);

#ifdef JUCE_MAC
  return hasTokenMacOS(key);
#elif JUCE_WINDOWS
  return hasTokenWindows(key);
#elif JUCE_LINUX
  return hasTokenLinux(key);
#endif
  return false;
}

bool SecureTokenStore::clearAllTokens() {
  // Get all token files and remove them
  auto storageDir = getSecureStorageDir();
  if (!storageDir.exists())
    return true;

  bool allCleared = true;
  for (const auto &entry :
       juce::RangedDirectoryIterator(storageDir, false, "*.token", juce::File::findFilesAndDirectories)) {
    if (!entry.getFile().deleteFile())
      allCleared = false;
  }

  return allCleared;
}

juce::String SecureTokenStore::getBackendType() const {
  return backendType_;
}

juce::File SecureTokenStore::getSecureStorageDir() {
  auto appSupport = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
  auto storageDir = appSupport.getChildFile("Sidechain").getChildFile("SecureTokens");
  storageDir.createDirectory();
  return storageDir;
}

juce::File SecureTokenStore::getTokenFilePath(const juce::String &key) {
  auto storageDir = getSecureStorageDir();
  // Use hash of key to create filename
  auto hashedKey = juce::String(std::hash<juce::String>{}(key));
  return storageDir.getChildFile(hashedKey + ".token");
}

// ========== macOS Keychain Implementation ==========

#ifdef JUCE_MAC

// SecKeychain APIs are deprecated since macOS 10.10 but still work.
// Migration to SecItem would require significant refactoring.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

bool SecureTokenStore::saveTokenMacOS(const juce::String &key, const juce::String &token) {
  const char *serviceName = "Sidechain";
  auto keyCStr = key.toRawUTF8();
  auto tokenCStr = token.toRawUTF8();

  // First, try to delete existing token
  SecKeychainItemRef itemRef = nullptr;
  OSStatus status = SecKeychainFindGenericPassword(nullptr, // search default keychain
                                                   (UInt32)strlen(serviceName), serviceName, (UInt32)key.length(),
                                                   keyCStr, nullptr, nullptr, &itemRef);

  if (status == noErr && itemRef != nullptr) {
    SecKeychainItemDelete(itemRef);
    CFRelease(itemRef);
  }

  // Add new token to keychain
  status = SecKeychainAddGenericPassword(nullptr, // add to default keychain
                                         (UInt32)strlen(serviceName), serviceName, (UInt32)key.length(), keyCStr,
                                         (UInt32)token.length(), tokenCStr,
                                         nullptr // don't return item reference
  );

  return status == noErr;
}

std::optional<juce::String> SecureTokenStore::loadTokenMacOS(const juce::String &key) {
  const char *serviceName = "Sidechain";
  auto keyCStr = key.toRawUTF8();

  UInt32 passwordLength = 0;
  void *passwordData = nullptr;
  SecKeychainItemRef itemRef = nullptr;

  OSStatus status = SecKeychainFindGenericPassword(nullptr, // search default keychain
                                                   (UInt32)strlen(serviceName), serviceName, (UInt32)key.length(),
                                                   keyCStr, &passwordLength, &passwordData, &itemRef);

  if (status != noErr || passwordData == nullptr)
    return std::nullopt;

  juce::String result(static_cast<const char *>(passwordData), passwordLength);

  // Clean up
  SecKeychainItemFreeContent(nullptr, passwordData);
  if (itemRef != nullptr)
    CFRelease(itemRef);

  return result;
}

bool SecureTokenStore::deleteTokenMacOS(const juce::String &key) {
  const char *serviceName = "Sidechain";
  auto keyCStr = key.toRawUTF8();

  SecKeychainItemRef itemRef = nullptr;
  OSStatus status = SecKeychainFindGenericPassword(nullptr, (UInt32)strlen(serviceName), serviceName,
                                                   (UInt32)key.length(), keyCStr, nullptr, nullptr, &itemRef);

  if (status != noErr || itemRef == nullptr)
    return false;

  status = SecKeychainItemDelete(itemRef);
  CFRelease(itemRef);

  return status == noErr;
}

bool SecureTokenStore::hasTokenMacOS(const juce::String &key) {
  const char *serviceName = "Sidechain";
  auto keyCStr = key.toRawUTF8();

  SecKeychainItemRef itemRef = nullptr;
  OSStatus status = SecKeychainFindGenericPassword(nullptr, (UInt32)strlen(serviceName), serviceName,
                                                   (UInt32)key.length(), keyCStr, nullptr, nullptr, &itemRef);

  bool found = status == noErr;
  if (itemRef != nullptr)
    CFRelease(itemRef);

  return found;
}

#pragma clang diagnostic pop

#endif // JUCE_MAC

// ========== Windows DPAPI Implementation ==========

#ifdef JUCE_WINDOWS

#include <dpapi.h>
#pragma comment(lib, "crypt32.lib")

juce::MemoryBlock SecureTokenStore::dpapi_encrypt(const juce::String &plaintext) {
  auto plaintextCStr = plaintext.toRawUTF8();
  size_t plaintextLen = plaintext.length();

  DATA_BLOB plaintextBlob{};
  plaintextBlob.pbData = reinterpret_cast<BYTE *>(const_cast<char *>(plaintextCStr));
  plaintextBlob.cbData = static_cast<DWORD>(plaintextLen);

  DATA_BLOB encryptedBlob{};
  BOOL result = CryptProtectData(&plaintextBlob, L"Sidechain Token", nullptr, nullptr, nullptr,
                                 CRYPTPROTECT_UI_FORBIDDEN, &encryptedBlob);

  juce::MemoryBlock encrypted;
  if (result && encryptedBlob.pbData != nullptr) {
    encrypted.append(encryptedBlob.pbData, encryptedBlob.cbData);
    LocalFree(encryptedBlob.pbData);
  }

  return encrypted;
}

juce::String SecureTokenStore::dpapi_decrypt(const juce::MemoryBlock &encrypted) {
  DATA_BLOB encryptedBlob{};
  encryptedBlob.pbData = static_cast<BYTE *>(const_cast<void *>(encrypted.getData()));
  encryptedBlob.cbData = static_cast<DWORD>(encrypted.getSize());

  DATA_BLOB decryptedBlob{};
  BOOL result =
      CryptUnprotectData(&encryptedBlob, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &decryptedBlob);

  juce::String decrypted;
  if (result && decryptedBlob.pbData != nullptr) {
    decrypted = juce::String(reinterpret_cast<const char *>(decryptedBlob.pbData), decryptedBlob.cbData);
    LocalFree(decryptedBlob.pbData);
  }

  return decrypted;
}

bool SecureTokenStore::saveTokenWindows(const juce::String &key, const juce::String &token) {
  auto encrypted = dpapi_encrypt(token);
  if (encrypted.isEmpty())
    return false;

  auto filepath = getTokenFilePath(key);
  return filepath.replaceWithData(encrypted.getData(), encrypted.getSize());
}

std::optional<juce::String> SecureTokenStore::loadTokenWindows(const juce::String &key) {
  auto filepath = getTokenFilePath(key);
  if (!filepath.exists())
    return std::nullopt;

  juce::MemoryBlock encryptedData;
  if (!filepath.loadFileAsData(encryptedData))
    return std::nullopt;

  auto decrypted = dpapi_decrypt(encryptedData);
  if (decrypted.isEmpty())
    return std::nullopt;

  return decrypted;
}

bool SecureTokenStore::deleteTokenWindows(const juce::String &key) {
  auto filepath = getTokenFilePath(key);
  return filepath.deleteFile();
}

bool SecureTokenStore::hasTokenWindows(const juce::String &key) {
  auto filepath = getTokenFilePath(key);
  return filepath.exists();
}

#endif // JUCE_WINDOWS

// ========== Linux Secret Service Implementation ==========

#ifdef JUCE_LINUX

bool SecureTokenStore::saveTokenLinux(const juce::String &key, const juce::String &token) {
  // WARNING: Linux uses file-based storage as this is a VST plugin limitation.
  // Tokens are stored in the application data directory with restricted permissions.
  // For production deployments, consider using a secure credential manager or
  // environment-based authentication instead of file-based token storage.
  juce::Logger::writeToLog("WARNING: Storing authentication tokens in file system (Linux fallback). "
                           "Consider using environment variables or a credential manager for production use.");

  auto filepath = getTokenFilePath(key);
  if (!filepath.replaceWithText(token))
    return false;

  // Restrict file permissions to owner read/write only (0600)
  // This is the best we can do for file-based storage security
#ifdef __linux__
  const char *nativePath = filepath.getFullPathName().toRawUTF8();
  chmod(nativePath, S_IRUSR | S_IWUSR); // 0600: owner read/write only
#endif

  return true;
}

std::optional<juce::String> SecureTokenStore::loadTokenLinux(const juce::String &key) {
  // For Linux, we use file-based storage as a fallback
  auto filepath = getTokenFilePath(key);
  if (!filepath.exists())
    return std::nullopt;

  return filepath.loadFileAsString();
}

bool SecureTokenStore::deleteTokenLinux(const juce::String &key) {
  auto filepath = getTokenFilePath(key);
  return filepath.deleteFile();
}

bool SecureTokenStore::hasTokenLinux(const juce::String &key) {
  auto filepath = getTokenFilePath(key);
  return filepath.exists();
}

#endif // JUCE_LINUX

// ========== Fallback Implementation ==========

bool SecureTokenStore::saveTokenFallback(const juce::String &key, const juce::String &token) {
  juce::Logger::writeToLog("WARNING: Secure token storage not available, using "
                           "fallback file storage (insecure)");
  auto filepath = getTokenFilePath(key);
  return filepath.replaceWithText(token);
}

std::optional<juce::String> SecureTokenStore::loadTokenFallback(const juce::String &key) {
  auto filepath = getTokenFilePath(key);
  if (!filepath.exists())
    return std::nullopt;

  return filepath.loadFileAsString();
}

bool SecureTokenStore::deleteTokenFallback(const juce::String &key) {
  auto filepath = getTokenFilePath(key);
  return filepath.deleteFile();
}

bool SecureTokenStore::hasTokenFallback(const juce::String &key) {
  auto filepath = getTokenFilePath(key);
  return filepath.exists();
}

} // namespace Security
} // namespace Sidechain
