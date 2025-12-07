#include "util/OSNotification.h"

//==============================================================================
// macOS Implementation (UserNotifications framework)
//==============================================================================
#if JUCE_MAC

#import <Foundation/Foundation.h>
#import <UserNotifications/UserNotifications.h>

// Objective-C++ bridge for macOS notifications
@interface NotificationDelegate : NSObject <UNUserNotificationCenterDelegate>
@end

@implementation NotificationDelegate
- (void)userNotificationCenter:(UNUserNotificationCenter*)center
       willPresentNotification:(UNNotification*)notification
         withCompletionHandler:(void (^)(UNNotificationPresentationOptions))completionHandler
{
    // Always show notifications, even when app is in foreground
    completionHandler(UNNotificationPresentationOptionBanner |
                     UNNotificationPresentationOptionSound |
                     UNNotificationPresentationOptionBadge);
}

- (void)userNotificationCenter:(UNUserNotificationCenter*)center
didReceiveNotificationResponse:(UNNotificationResponse*)response
         withCompletionHandler:(void (^)(void))completionHandler
{
    completionHandler();
}
@end

static NotificationDelegate* notificationDelegate = nil;

static void initializeMacOSNotifications()
{
    static bool initialized = false;
    if (initialized)
        return;
    
    notificationDelegate = [[NotificationDelegate alloc] init];
    UNUserNotificationCenter* center = [UNUserNotificationCenter currentNotificationCenter];
    center.delegate = notificationDelegate;
    initialized = true;
}

bool OSNotification::showMacOS(const juce::String& title,
                                const juce::String& message,
                                const juce::String& subtitle,
                                bool sound)
{
    @autoreleasepool
    {
        initializeMacOSNotifications();
        
        UNUserNotificationCenter* center = [UNUserNotificationCenter currentNotificationCenter];
        
        // Create notification content
        UNMutableNotificationContent* content = [[UNMutableNotificationContent alloc] init];
        content.title = title.toNSString();
        
        if (message.isNotEmpty())
            content.body = message.toNSString();
        
        if (subtitle.isNotEmpty())
            content.subtitle = subtitle.toNSString();
        
        if (sound)
            content.sound = [UNNotificationSound defaultSound];
        
        // Use application bundle identifier as category
        content.categoryIdentifier = @"SIDECHAIN_NOTIFICATION";
        
        // Create a unique identifier for this notification
        NSString* identifier = [NSString stringWithFormat:@"sidechain-%ld", (long)[[NSDate date] timeIntervalSince1970]];
        
        // Create trigger (immediate)
        UNTimeIntervalNotificationTrigger* trigger = [UNTimeIntervalNotificationTrigger
                                                      triggerWithTimeInterval:0.1
                                                      repeats:NO];
        
        // Create request
        UNNotificationRequest* request = [UNNotificationRequest requestWithIdentifier:identifier
                                                                              content:content
                                                                              trigger:trigger];
        
        // Schedule notification
        [center addNotificationRequest:request withErrorHandler:^(NSError* error) {
            if (error)
            {
                Log::warn("Failed to show macOS notification: " + juce::String(error.localizedDescription.UTF8String));
            }
        }];
        
        return true;
    }
}

bool OSNotification::hasPermissionMacOS()
{
    __block bool hasPermission = false;
    __block bool checked = false;
    
    UNUserNotificationCenter* center = [UNUserNotificationCenter currentNotificationCenter];
    [center getNotificationSettingsWithCompletionHandler:^(UNNotificationSettings* settings) {
        hasPermission = (settings.authorizationStatus == UNAuthorizationStatusAuthorized);
        checked = true;
    }];
    
    // Wait for async result (with timeout) - run runloop on main thread
    if ([NSThread isMainThread])
    {
        NSRunLoop* runLoop = [NSRunLoop currentRunLoop];
        NSDate* timeout = [NSDate dateWithTimeIntervalSinceNow:1.0];
        while (!checked && [timeout timeIntervalSinceNow] > 0)
        {
            [runLoop runMode:NSDefaultRunLoopMode beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.1]];
        }
    }
    else
    {
        // If not on main thread, we can't reliably check synchronously
        // Return false and let the caller handle it asynchronously
        return false;
    }
    
    return hasPermission;
}

void OSNotification::requestPermissionMacOS(std::function<void(bool)> callback)
{
    UNUserNotificationCenter* center = [UNUserNotificationCenter currentNotificationCenter];
    [center requestAuthorizationWithOptions:(UNAuthorizationOptionAlert | UNAuthorizationOptionSound | UNAuthorizationOptionBadge)
                          completionHandler:^(BOOL granted, NSError* error) {
        if (error)
        {
            Log::warn("Failed to request macOS notification permission: " + 
                     juce::String(error.localizedDescription.UTF8String));
        }
        
        if (callback)
        {
            juce::MessageManager::callAsync([callback, granted]() {
                callback(granted);
            });
        }
    }];
}

#endif // JUCE_MAC

//==============================================================================
// Windows Implementation (Windows Runtime Toast Notifications)
//==============================================================================
#if JUCE_WINDOWS

#include <windows.h>
#include <wrl/client.h>
#include <wrl/implements.h>
#include <notificationactivationcallback.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Data.Xml.Dom.h>
#include <winrt/Windows.UI.Notifications.h>

using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Data::Xml::Dom;
using namespace Windows::UI::Notifications;

static bool initializeWindowsNotifications()
{
    // Windows notifications don't require explicit initialization
    // The ToastNotificationManager is automatically available
    return true;
}

bool OSNotification::showWindows(const juce::String& title,
                                  const juce::String& message,
                                  const juce::String& subtitle,
                                  bool sound)
{
    try
    {
        // Create XML toast template
        XmlDocument toastXml = ToastNotificationManager::GetTemplateContent(
            ToastTemplateType::ToastText02); // Two text fields (title + message)
        
        // Get text elements
        XmlNodeList textNodes = toastXml.GetElementsByTagName(L"text");
        
        if (textNodes.Size() >= 1)
        {
            textNodes.Item(0).AppendChild(toastXml.CreateTextNode(title.toWideCharPointer()));
        }
        
        juce::String bodyText = subtitle.isNotEmpty() ? (subtitle + " - " + message) : message;
        if (bodyText.isEmpty())
            bodyText = title;
            
        if (textNodes.Size() >= 2)
        {
            textNodes.Item(1).AppendChild(toastXml.CreateTextNode(bodyText.toWideCharPointer()));
        }
        else if (textNodes.Size() == 1 && bodyText.isNotEmpty())
        {
            // Add second text node if template only has one
            XmlElement toast = toastXml.SelectSingleNode(L"/toast").as<XmlElement>();
            XmlElement visual = toast.SelectSingleNode(L"visual").as<XmlElement>();
            XmlElement binding = visual.SelectSingleNode(L"binding").as<XmlElement>();
            
            XmlElement textNode = toastXml.CreateElement(L"text");
            textNode.AppendChild(toastXml.CreateTextNode(bodyText.toWideCharPointer()));
            binding.AppendChild(textNode);
        }
        
        // Configure sound
        if (sound)
        {
            XmlElement toast = toastXml.SelectSingleNode(L"/toast").as<XmlElement>();
            XmlElement audio = toastXml.CreateElement(L"audio");
            audio.SetAttribute(L"src", L"ms-winsoundevent:Notification.Default");
            toast.AppendChild(audio);
        }
        
        // Create and show notification
        ToastNotification toast{ toastXml };
        
        // Get application ID (use executable name as fallback)
        juce::String appId = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
                               .getFileNameWithoutExtension();
        
        ToastNotificationManager::CreateToastNotifier(appId.toWideCharPointer()).Show(toast);
        
        return true;
    }
    catch (const hresult_error& ex)
    {
        Log::warn("Failed to show Windows notification: " + 
                 juce::String(ex.message().c_str()));
        return false;
    }
    catch (...)
    {
        Log::warn("Failed to show Windows notification: Unknown error");
        return false;
    }
}

bool OSNotification::hasPermissionWindows()
{
    // Windows 10+ generally allows notifications by default
    // There's no simple API to check, so we assume true
    // The notification will simply fail to show if blocked
    return true;
}

void OSNotification::requestPermissionWindows(std::function<void(bool)> callback)
{
    // Windows doesn't require explicit permission requests
    // Notifications are controlled through system settings
    if (callback)
    {
        juce::MessageManager::callAsync([callback]() {
            callback(true);
        });
    }
}

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

static bool initializeLibNotify()
{
    static bool initialized = false;
    if (!initialized)
    {
        if (!notify_init("Sidechain"))
        {
            Log::warn("OSNotification: Failed to initialize libnotify");
            return false;
        }
        initialized = true;
    }
    return true;
}

bool OSNotification::showLinux(const juce::String& title,
                                const juce::String& message,
                                const juce::String& subtitle,
                                bool sound)
{
    if (!initializeLibNotify())
        return false;
    
    // Build body text
    juce::String bodyText = message;
    if (subtitle.isNotEmpty())
    {
        if (bodyText.isNotEmpty())
            bodyText = subtitle + " - " + bodyText;
        else
            bodyText = subtitle;
    }
    
    // Create notification
    NotifyNotification* notification = notify_notification_new(
        title.toUTF8().getAddress(),
        bodyText.isNotEmpty() ? bodyText.toUTF8().getAddress() : nullptr,
        nullptr  // icon - nullptr uses default
    );
    
    if (!notification)
    {
        Log::warn("OSNotification: Failed to create libnotify notification");
        return false;
    }
    
    // Set timeout (5 seconds)
    notify_notification_set_timeout(notification, 5000);
    
    // Set urgency (normal)
    notify_notification_set_urgency(notification, NOTIFY_URGENCY_NORMAL);
    
    // Show notification
    GError* error = nullptr;
    if (!notify_notification_show(notification, &error))
    {
        if (error)
        {
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

bool OSNotification::hasPermissionLinux()
{
    return initializeLibNotify();
}

void OSNotification::requestPermissionLinux(std::function<void(bool)> callback)
{
    // Linux doesn't require explicit permission requests
    // Notifications are controlled through desktop environment settings
    bool hasPermission = hasPermissionLinux();
    if (callback)
    {
        juce::MessageManager::callAsync([callback, hasPermission]() {
            callback(hasPermission);
        });
    }
}

#elif SIDECHAIN_HAS_DBUS

#include <dbus/dbus.h>
#include <unistd.h>

static DBusConnection* getDBusConnection()
{
    static DBusConnection* connection = nullptr;
    static bool initialized = false;
    
    if (initialized)
        return connection;
    
    DBusError error;
    dbus_error_init(&error);
    
    connection = dbus_bus_get(DBUS_BUS_SESSION, &error);
    
    if (dbus_error_is_set(&error))
    {
        Log::warn("Failed to connect to D-Bus: " + juce::String(error.message));
        dbus_error_free(&error);
        return nullptr;
    }
    
    initialized = true;
    return connection;
}

bool OSNotification::showLinux(const juce::String& title,
                                const juce::String& message,
                                const juce::String& subtitle,
                                bool sound)
{
    DBusConnection* connection = getDBusConnection();
    if (!connection)
    {
        Log::warn("D-Bus connection not available for notifications");
        return false;
    }
    
    DBusMessage* msg = dbus_message_new_method_call(
        "org.freedesktop.Notifications",              // Service name
        "/org/freedesktop/Notifications",             // Object path
        "org.freedesktop.Notifications",              // Interface
        "Notify"                                      // Method name
    );
    
    if (!msg)
    {
        Log::warn("Failed to create D-Bus notification message");
        return false;
    }
    
    // Build body text
    juce::String bodyText = message;
    if (subtitle.isNotEmpty())
    {
        if (bodyText.isNotEmpty())
            bodyText = subtitle + " - " + bodyText;
        else
            bodyText = subtitle;
    }
    
    // Parameters for Notify method:
    // app_name, replaces_id, app_icon, summary, body, actions, hints, expire_timeout
    const char* appName = "Sidechain";
    uint32_t replacesId = 0;  // 0 means don't replace any existing notification
    const char* appIcon = "";  // Empty string = no icon
    const char* summary = title.toUTF8().getAddress();
    const char* body = bodyText.isNotEmpty() ? bodyText.toUTF8().getAddress() : "";
    
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
    
    if (sound)
    {
        const char* hintKey = "sound-name";
        const char* hintValue = "message-new-instant";
        
        dbus_message_iter_open_container(&dict, DBUS_TYPE_DICT_ENTRY, nullptr, &entry);
        dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &hintKey);
        
        dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, DBUS_TYPE_STRING_AS_STRING, &variant);
        dbus_message_iter_append_basic(&variant, DBUS_TYPE_STRING, &hintValue);
        dbus_message_iter_close_container(&entry, &variant);
        dbus_message_iter_close_container(&dict, &entry);
    }
    
    dbus_message_iter_close_container(&args, &dict);
    
    // expire_timeout (int32) - -1 = default, 0 = never expire
    int32_t expireTimeout = 5000;  // 5 seconds
    dbus_message_iter_append_basic(&args, DBUS_TYPE_INT32, &expireTimeout);
    
    // Send message and wait for reply
    DBusPendingCall* pending;
    if (!dbus_connection_send_with_reply(connection, msg, &pending, -1))
    {
        Log::warn("Failed to send D-Bus notification message");
        dbus_message_unref(msg);
        return false;
    }
    
    dbus_connection_flush(connection);
    dbus_message_unref(msg);
    
    // Wait for reply (non-blocking, with timeout)
    dbus_pending_call_block(pending);
    DBusMessage* reply = dbus_pending_call_steal_reply(pending);
    dbus_pending_call_unref(pending);
    
    if (reply)
    {
        dbus_message_unref(reply);
    }
    
    return true;
}

bool OSNotification::hasPermissionLinux()
{
    // Linux desktop environments generally allow notifications
    // There's no standard API to check, so we assume true
    // The notification will simply fail if D-Bus is unavailable
    return getDBusConnection() != nullptr;
}

void OSNotification::requestPermissionLinux(std::function<void(bool)> callback)
{
    // Linux doesn't require explicit permission requests
    // Notifications are controlled through desktop environment settings
    bool hasPermission = hasPermissionLinux();
    if (callback)
    {
        juce::MessageManager::callAsync([callback, hasPermission]() {
            callback(hasPermission);
        });
    }
}

#else // Neither libnotify nor D-Bus available

// Stub implementations when no notification backend is available
bool OSNotification::showLinux(const juce::String& title,
                                const juce::String& message,
                                const juce::String& subtitle,
                                bool sound)
{
    Log::warn("OSNotification: Neither libnotify nor D-Bus available. Desktop notifications disabled on Linux.");
    juce::ignoreUnused(title, message, subtitle, sound);
    return false;
}

bool OSNotification::hasPermissionLinux()
{
    return false;
}

void OSNotification::requestPermissionLinux(std::function<void(bool)> callback)
{
    if (callback)
    {
        juce::MessageManager::callAsync([callback]() {
            callback(false);
        });
    }
}

#endif // SIDECHAIN_HAS_LIBNOTIFY / SIDECHAIN_HAS_DBUS

#endif // JUCE_LINUX

//==============================================================================
// Public API Implementation
//==============================================================================

bool OSNotification::isSupported()
{
#if JUCE_MAC || JUCE_WINDOWS || JUCE_LINUX
    return true;
#else
    return false;
#endif
}

bool OSNotification::hasPermission()
{
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

void OSNotification::requestPermission(std::function<void(bool)> callback)
{
#if JUCE_MAC
    requestPermissionMacOS(callback);
#elif JUCE_WINDOWS
    requestPermissionWindows(callback);
#elif JUCE_LINUX
    requestPermissionLinux(callback);
#else
    if (callback)
    {
        juce::MessageManager::callAsync([callback]() {
            callback(false);
        });
    }
#endif
}

bool OSNotification::show(const juce::String& title,
                           const juce::String& message,
                           const juce::String& subtitle,
                           bool sound)
{
    // Check if notifications are supported on this platform
    if (!isSupported())
    {
        Log::debug("OSNotification::show called but notifications not supported on this platform");
        return false;
    }
    
    if (title.isEmpty())
    {
        Log::warn("OSNotification::show called with empty title");
        return false;
    }
    
    // Ensure we're on the message thread for platform-specific APIs
    if (juce::MessageManager::getInstance()->isThisTheMessageThread())
    {
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
    }
    else
    {
        // Call from message thread asynchronously
        bool result = false;
        juce::MessageManager::callAsync([title, message, subtitle, sound, &result]() {
            result = show(title, message, subtitle, sound);
        });
        return result;
    }
}
