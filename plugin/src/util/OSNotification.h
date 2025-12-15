#pragma once

#include "util/Log.h"
#include <JuceHeader.h>

/**
 * OSNotification - Cross-platform desktop notification utility
 *
 * Shows native operating system notifications on macOS, Windows, and Linux.
 * Uses platform-specific APIs:
 * - macOS: UserNotifications framework
 * - Windows: Windows Runtime Toast Notifications API
 * - Linux: libnotify (preferred) or D-Bus notification system
 *
 * Thread-safe - can be called from any thread. Notifications are queued
 * and displayed on the main message thread.
 */
class OSNotification {
public:
  /**
   * Show a desktop notification
   *
   * Automatically checks if notifications are supported on the current
   * platform. If not supported, returns false without showing a notification.
   *
   * @param title The notification title (required)
   * @param message The notification body text (optional, can be empty)
   * @param subtitle Optional subtitle (platform-dependent, may be ignored)
   * @param sound Whether to play a notification sound (default: true)
   * @return true if the notification was successfully sent, false otherwise
   */
  static bool show(const juce::String &title, const juce::String &message = "", const juce::String &subtitle = "",
                   bool sound = true);

  /**
   * Check if the current platform supports desktop notifications
   *
   * @return true if notifications are supported on this platform
   */
  static bool isSupported();

  /**
   * Request notification permissions (required on macOS, optional on other
   * platforms)
   *
   * This is a non-blocking call. The result will be delivered asynchronously
   * if a callback is provided.
   *
   * @param callback Optional callback called with the permission result (true =
   * granted)
   */
  static void requestPermission(std::function<void(bool)> callback = nullptr);

  /**
   * Check if notification permissions have been granted
   *
   * @return true if permissions are granted, false otherwise
   *         On platforms that don't require permissions, returns true
   */
  static bool hasPermission();

private:
  // Platform-specific implementations
  static bool showMacOS(const juce::String &title, const juce::String &message, const juce::String &subtitle,
                        bool sound);

  static bool showWindows(const juce::String &title, const juce::String &message, const juce::String &subtitle,
                          bool sound);

  static bool showLinux(const juce::String &title, const juce::String &message, const juce::String &subtitle,
                        bool sound);

  // Permission checking
  static bool hasPermissionMacOS();
  static bool hasPermissionWindows();
  static bool hasPermissionLinux();

  // Permission requesting
  static void requestPermissionMacOS(std::function<void(bool)> callback);
  static void requestPermissionWindows(std::function<void(bool)> callback);
  static void requestPermissionLinux(std::function<void(bool)> callback);
};
