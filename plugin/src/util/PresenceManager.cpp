#include "PresenceManager.h"
#include "DAWProjectFolder.h"
#include "Log.h"
#include "../network/WebSocketClient.h"

PresenceManager::PresenceManager(WebSocketClient *webSocket) : juce::Thread("PresenceManager"), webSocket(webSocket) {
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

    // Mark user as offline
    if (webSocket && webSocket->isConnected()) {
      juce::var payload(new juce::DynamicObject());
      payload.getDynamicObject()->setProperty("status", "offline");
      payload.getDynamicObject()->setProperty("timestamp", juce::Time::currentTimeMillis());
      webSocket->send("presence", payload);
    }

    Log::debug("PresenceManager: Stopped");
  }
}

juce::String PresenceManager::getDetectedDAW() const {
  return detectedDAW;
}

juce::String PresenceManager::getCurrentStatus() const {
  return currentStatus.load();
}

void PresenceManager::setStatus(const juce::String &status) {
  if (status == "online" || status == "in_studio") {
    currentStatus.store(status);
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
  if (!webSocket || !webSocket->isConnected()) {
    return;
  }

  juce::var payload(new juce::DynamicObject());
  payload.getDynamicObject()->setProperty("status", currentStatus.load());
  payload.getDynamicObject()->setProperty("daw", detectedDAW);
  payload.getDynamicObject()->setProperty("timestamp", juce::Time::currentTimeMillis());

  webSocket->send("presence", payload);

  Log::debug("PresenceManager: Sent update (" + currentStatus.load() + ", DAW: " + detectedDAW + ")");
}

void PresenceManager::detectDAW() {
  // Use the existing DAW detection from DAWProjectFolder
  DAWProjectFolder::DAWProjectInfo info = DAWProjectFolder::detectDAWProjectFolder("");
  detectedDAW = info.dawName;

  Log::debug("PresenceManager: Detected DAW - " + detectedDAW);
}
