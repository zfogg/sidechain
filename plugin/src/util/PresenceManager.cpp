#include "PresenceManager.h"
#include "DAWProjectFolder.h"
#include "Log.h"
#include "../network/StreamChatClient.h"

PresenceManager::PresenceManager(StreamChatClient *chat) : juce::Thread("PresenceManager"), streamChat(chat) {
  detectDAW();
}

PresenceManager::~PresenceManager() {
  stop();
}

void PresenceManager::start() {
  if (!isThreadRunning()) {
    startThread();
    Log::debug("PresenceManager: Started (DAW: " + detectedDAW + ")");
  }
}

void PresenceManager::stop() {
  if (isThreadRunning()) {
    signalThreadShouldExit();
    waitForThreadToExit(5000);

    // Mark user as offline on getstream.io
    if (streamChat && streamChat->isAuthenticated()) {
      isOnline.store(false);

      juce::var extraData(new juce::DynamicObject());
      extraData.getDynamicObject()->setProperty("in_studio", false);
      extraData.getDynamicObject()->setProperty("daw_type", "");
      extraData.getDynamicObject()->setProperty("last_active", juce::Time::currentTimeMillis());

      streamChat->updateStatus("offline", extraData, [](Outcome<void> result) {
        if (result.isError()) {
          Log::error("PresenceManager: Failed to update offline status - " + result.getError());
        } else {
          Log::debug("PresenceManager: Marked user as offline");
        }
      });
    }

    Log::debug("PresenceManager: Stopped");
  }
}

juce::String PresenceManager::getDetectedDAW() const {
  return detectedDAW;
}

juce::String PresenceManager::getCurrentStatus() const {
  return isInStudio.load() ? "in_studio" : "online";
}

void PresenceManager::setStatus(const juce::String &status) {
  if (status == "online" || status == "in_studio") {
    bool shouldBeInStudio = (status == "in_studio");
    isInStudio.store(shouldBeInStudio);
    isOnline.store(true);
    // Send update immediately when status changes
    sendPresenceUpdate();
  }
}

void PresenceManager::run() {
  // Send initial presence update
  sendPresenceUpdate();

  // Periodically send heartbeat presence updates every 30 seconds
  while (!threadShouldExit()) {
    // Sleep for 30 seconds (check for exit every 100ms)
    for (int i = 0; i < 300; ++i) {
      if (threadShouldExit()) {
        return;
      }
      juce::Thread::sleep(100);
    }

    sendPresenceUpdate();
  }
}

void PresenceManager::sendPresenceUpdate() {
  if (!streamChat || !streamChat->isAuthenticated()) {
    return;
  }

  // Build extra data with presence info
  juce::var extraData(new juce::DynamicObject());
  extraData.getDynamicObject()->setProperty("in_studio", isInStudio.load());
  extraData.getDynamicObject()->setProperty("daw_type", detectedDAW);
  extraData.getDynamicObject()->setProperty("last_active", juce::Time::currentTimeMillis());

  // Determine status string
  juce::String statusStr = isOnline.load() ? "online" : "offline";

  // Update status on getstream.io with DAW metadata
  streamChat->updateStatus(statusStr, extraData, [this](Outcome<void> result) {
    if (result.isError()) {
      Log::error("PresenceManager: Failed to send presence update - " + result.getError());
    } else {
      Log::debug("PresenceManager: Sent update (" + getCurrentStatus() + ", DAW: " + detectedDAW + ")");
    }
  });
}

void PresenceManager::detectDAW() {
  // Use the existing DAW detection from DAWProjectFolder
  DAWProjectFolder::DAWProjectInfo info = DAWProjectFolder::detectDAWProjectFolder("");
  detectedDAW = info.dawName;

  Log::debug("PresenceManager: Detected DAW - " + detectedDAW);
}
