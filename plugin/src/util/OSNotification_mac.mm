//==============================================================================
// macOS Implementation (UserNotifications framework)
// NOTE: Foundation/UserNotifications must be imported BEFORE JUCE headers
// to avoid type name conflicts (Point, Component, Rect, etc.)
//==============================================================================
#if __APPLE__
#import <Foundation/Foundation.h>
#import <UserNotifications/UserNotifications.h>

#include "util/OSNotification.h"

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
        content.title = [NSString stringWithUTF8String:title.toUTF8()];

        if (message.isNotEmpty())
            content.body = [NSString stringWithUTF8String:message.toUTF8()];

        if (subtitle.isNotEmpty())
            content.subtitle = [NSString stringWithUTF8String:subtitle.toUTF8()];

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
        [center addNotificationRequest:request withCompletionHandler:^(NSError* error) {
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

#endif // __APPLE__
