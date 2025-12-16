#include "util/OSNotification.h"

//==============================================================================
// macOS stubs - real implementation is in OSNotification_mac.mm
//==============================================================================
#if JUCE_MAC
// Implementations provided in OSNotification_mac.mm
#endif

//==============================================================================
// Windows Implementation (Windows Runtime Toast Notifications)
//==============================================================================
#if JUCE_WINDOWS

// Disable warning about WinRT being deprecated in favor of C++/WinRT
#pragma warning(push)
#pragma warning(disable : 4996)

#include <windows.h>
#include <winrt/base.h>

// Only use WinRT toast notifications on Windows 10+
// Check at runtime since we want to compile on older SDKs too
static bool isWindows10OrLater() {
  // Use RtlGetVersion to get accurate version (GetVersionEx lies on Win10+)
  using RtlGetVersionPtr = LONG(WINAPI *)(PRTL_OSVERSIONINFOW);
  HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
  if (ntdll) {
    auto rtlGetVersion = reinterpret_cast<RtlGetVersionPtr>(GetProcAddress(ntdll, "RtlGetVersion"));
    if (rtlGetVersion) {
      RTL_OSVERSIONINFOW osvi = {};
      osvi.dwOSVersionInfoSize = sizeof(osvi);
      if (rtlGetVersion(&osvi) == 0) {
        return osvi.dwMajorVersion >= 10;
      }
    }
  }
  return false;
}

bool OSNotification::showWindows(const juce::String &title, const juce::String &message, const juce::String &subtitle,
                                 bool sound) {
  // Toast notifications require Windows 10+
  if (!isWindows10OrLater()) {
    Log::warn("Windows toast notifications require Windows 10 or later");
    return false;
  }

  // Use a simple fallback: MessageBox for now
  // Full WinRT toast implementation requires COM initialization and
  // application manifest with proper AUMID registration
  juce::String bodyText = subtitle.isNotEmpty() ? (subtitle + "\n" + message) : message;
  if (bodyText.isEmpty())
    bodyText = title;

  // For VST plugins, we can't reliably use toast notifications because:
  // 1. The host application owns the COM apartment
  // 2. We don't have control over the application manifest
  // 3. Toast notifications require AUMID registration
  //
  // Instead, we'll use a non-blocking approach via JUCE's native alert
  juce::MessageManager::callAsync([title, bodyText]() {
    juce::NativeMessageBox::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon, title, bodyText, nullptr, nullptr);
  });

  juce::ignoreUnused(sound);
  return true;
}

bool OSNotification::hasPermissionWindows() {
  // Windows message boxes don't require permission
  return true;
}

void OSNotification::requestPermissionWindows(std::function<void(bool)> callback) {
  // Windows doesn't require explicit permission requests
  if (callback) {
    juce::MessageManager::callAsync([callback]() { callback(true); });
  }
}

#pragma warning(pop)

#endif // JUCE_WINDOWS

//==============================================================================
// Linux Implementation (libnotify or D-Bus notifications)
//==============================================================================
#if JUCE_LINUX

#ifndef SIDECHAIN_HAS_LIBNOTIFY
#define SIDECHAIN_HAS_LIBNOTIFY 0
#endif

#ifndef SIDECHAIN_HAS_DBUS
#define SIDECHAIN_HAS_DBUS 0
#endif

#if SIDECHAIN_HAS_LIBNOTIFY

#include <libnotify/notify.h>
#include <unistd.h>

static bool initializeLibNotify() {
  static bool initialized = false;
  if (!initialized) {
    if (!notify_init("Sidechain")) {
      Log::warn("OSNotification: Failed to initialize libnotify");
      return false;
    }
    initialized = true;
  }
  return true;
}

bool OSNotification::showLinux(const juce::String &title, const juce::String &message, const juce::String &subtitle,
                               [[maybe_unused]] bool sound) {
  if (!initializeLibNotify())
    return false;

  // Build body text
  juce::String bodyText = message;
  if (subtitle.isNotEmpty()) {
    if (bodyText.isNotEmpty())
      bodyText = subtitle + " - " + bodyText;
    else
      bodyText = subtitle;
  }

  // Create notification
  NotifyNotification *notification = notify_notification_new(
      title.toUTF8().getAddress(), bodyText.isNotEmpty() ? bodyText.toUTF8().getAddress() : nullptr,
      nullptr // icon - nullptr uses default
  );

  if (!notification) {
    Log::warn("OSNotification: Failed to create libnotify notification");
    return false;
  }

  // Set timeout (5 seconds)
  notify_notification_set_timeout(notification, 5000);

  // Set urgency (normal)
  notify_notification_set_urgency(notification, NOTIFY_URGENCY_NORMAL);

  // Show notification
  GError *error = nullptr;
  if (!notify_notification_show(notification, &error)) {
    if (error) {
      Log::warn("OSNotification: Failed to show notification: " + juce::String(error->message));
      g_error_free(error);
    }
    g_object_unref(notification);
    return false;
  }

  // Note: notification object will be automatically cleaned up by libnotify
  // after it's shown, but we can explicitly unref it here
  g_object_unref(notification);

  return true;
}

bool OSNotification::hasPermissionLinux() {
  return initializeLibNotify();
}

void OSNotification::requestPermissionLinux(std::function<void(bool)> callback) {
  // Linux doesn't require explicit permission requests
  // Notifications are controlled through desktop environment settings
  bool hasPermission = hasPermissionLinux();
  if (callback) {
    juce::MessageManager::callAsync([callback, hasPermission]() { callback(hasPermission); });
  }
}

#elif SIDECHAIN_HAS_DBUS

#include <dbus/dbus.h>
#include <unistd.h>

static DBusConnection *getDBusConnection() {
  static DBusConnection *connection = nullptr;
  static bool initialized = false;

  if (initialized)
    return connection;

  DBusError error;
  dbus_error_init(&error);

  connection = dbus_bus_get(DBUS_BUS_SESSION, &error);

  if (dbus_error_is_set(&error)) {
    Log::warn("Failed to connect to D-Bus: " + juce::String(error.message));
    dbus_error_free(&error);
    return nullptr;
  }

  initialized = true;
  return connection;
}

bool OSNotification::showLinux(const juce::String &title, const juce::String &message, const juce::String &subtitle,
                               bool sound) {
  DBusConnection *connection = getDBusConnection();
  if (!connection) {
    Log::warn("D-Bus connection not available for notifications");
    return false;
  }

  DBusMessage *msg = dbus_message_new_method_call("org.freedesktop.Notifications",  // Service name
                                                  "/org/freedesktop/Notifications", // Object path
                                                  "org.freedesktop.Notifications",  // Interface
                                                  "Notify"                          // Method name
  );

  if (!msg) {
    Log::warn("Failed to create D-Bus notification message");
    return false;
  }

  // Build body text
  juce::String bodyText = message;
  if (subtitle.isNotEmpty()) {
    if (bodyText.isNotEmpty())
      bodyText = subtitle + " - " + bodyText;
    else
      bodyText = subtitle;
  }

  // Parameters for Notify method:
  // app_name, replaces_id, app_icon, summary, body, actions, hints,
  // expire_timeout
  const char *appName = "Sidechain";
  uint32_t replacesId = 0;  // 0 means don't replace any existing notification
  const char *appIcon = ""; // Empty string = no icon
  const char *summary = title.toUTF8().getAddress();
  const char *body = bodyText.isNotEmpty() ? bodyText.toUTF8().getAddress() : "";

  DBusMessageIter args, array, dict, entry, variant;
  dbus_message_iter_init_append(msg, &args);

  // app_name (string)
  dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &appName);

  // replaces_id (uint32)
  dbus_message_iter_append_basic(&args, DBUS_TYPE_UINT32, &replacesId);

  // app_icon (string)
  dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &appIcon);

  // summary (string)
  dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &summary);

  // body (string)
  dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &body);

  // actions (array of strings) - empty array
  dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY, DBUS_TYPE_STRING_AS_STRING, &array);
  dbus_message_iter_close_container(&args, &array);

  // hints (dictionary) - optional metadata
  dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY, "{sv}", &dict);

  if (sound) {
    const char *hintKey = "sound-name";
    const char *hintValue = "message-new-instant";

    dbus_message_iter_open_container(&dict, DBUS_TYPE_DICT_ENTRY, nullptr, &entry);
    dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &hintKey);

    dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, DBUS_TYPE_STRING_AS_STRING, &variant);
    dbus_message_iter_append_basic(&variant, DBUS_TYPE_STRING, &hintValue);
    dbus_message_iter_close_container(&entry, &variant);
    dbus_message_iter_close_container(&dict, &entry);
  }

  dbus_message_iter_close_container(&args, &dict);

  // expire_timeout (int32) - -1 = default, 0 = never expire
  int32_t expireTimeout = 5000; // 5 seconds
  dbus_message_iter_append_basic(&args, DBUS_TYPE_INT32, &expireTimeout);

  // Send message and wait for reply
  DBusPendingCall *pending;
  if (!dbus_connection_send_with_reply(connection, msg, &pending, -1)) {
    Log::warn("Failed to send D-Bus notification message");
    dbus_message_unref(msg);
    return false;
  }

  dbus_connection_flush(connection);
  dbus_message_unref(msg);

  // Wait for reply (non-blocking, with timeout)
  dbus_pending_call_block(pending);
  DBusMessage *reply = dbus_pending_call_steal_reply(pending);
  dbus_pending_call_unref(pending);

  if (reply) {
    dbus_message_unref(reply);
  }

  return true;
}

bool OSNotification::hasPermissionLinux() {
  // Linux desktop environments generally allow notifications
  // There's no standard API to check, so we assume true
  // The notification will simply fail if D-Bus is unavailable
  return getDBusConnection() != nullptr;
}

void OSNotification::requestPermissionLinux(std::function<void(bool)> callback) {
  // Linux doesn't require explicit permission requests
  // Notifications are controlled through desktop environment settings
  bool hasPermission = hasPermissionLinux();
  if (callback) {
    juce::MessageManager::callAsync([callback, hasPermission]() { callback(hasPermission); });
  }
}

#else // Neither libnotify nor D-Bus available

// Stub implementations when no notification backend is available
bool OSNotification::showLinux(const juce::String &title, const juce::String &message, const juce::String &subtitle,
                               bool sound) {
  Log::warn("OSNotification: Neither libnotify nor D-Bus available. Desktop "
            "notifications disabled on Linux.");
  juce::ignoreUnused(title, message, subtitle, sound);
  return false;
}

bool OSNotification::hasPermissionLinux() {
  return false;
}

void OSNotification::requestPermissionLinux(std::function<void(bool)> callback) {
  if (callback) {
    juce::MessageManager::callAsync([callback]() { callback(false); });
  }
}

#endif // SIDECHAIN_HAS_LIBNOTIFY / SIDECHAIN_HAS_DBUS

#endif // JUCE_LINUX

//==============================================================================
// Public API Implementation
//==============================================================================

bool OSNotification::isSupported() {
#if JUCE_MAC || JUCE_WINDOWS || JUCE_LINUX
  return true;
#else
  return false;
#endif
}

bool OSNotification::hasPermission() {
#if JUCE_MAC
  return hasPermissionMacOS();
#elif JUCE_WINDOWS
  return hasPermissionWindows();
#elif JUCE_LINUX
  return hasPermissionLinux();
#else
  return false;
#endif
}

void OSNotification::requestPermission(std::function<void(bool)> callback) {
#if JUCE_MAC
  requestPermissionMacOS(callback);
#elif JUCE_WINDOWS
  requestPermissionWindows(callback);
#elif JUCE_LINUX
  requestPermissionLinux(callback);
#else
  if (callback) {
    juce::MessageManager::callAsync([callback]() { callback(false); });
  }
#endif
}

bool OSNotification::show(const juce::String &title, const juce::String &message, const juce::String &subtitle,
                          bool sound) {
  // Check if notifications are supported on this platform
  if (!isSupported()) {
    Log::debug("OSNotification::show called but notifications not supported on "
               "this platform");
    return false;
  }

  if (title.isEmpty()) {
    Log::warn("OSNotification::show called with empty title");
    return false;
  }

  // Ensure we're on the message thread for platform-specific APIs
  if (juce::MessageManager::getInstance()->isThisTheMessageThread()) {
#if JUCE_MAC
    return showMacOS(title, message, subtitle, sound);
#elif JUCE_WINDOWS
    return showWindows(title, message, subtitle, sound);
#elif JUCE_LINUX
    return showLinux(title, message, subtitle, sound);
#else
    Log::warn("OSNotification not supported on this platform");
    return false;
#endif
  } else {
    // Call from message thread asynchronously
    bool result = false;
    juce::MessageManager::callAsync(
        [title, message, subtitle, sound, &result]() { result = show(title, message, subtitle, sound); });
    return result;
  }
}
