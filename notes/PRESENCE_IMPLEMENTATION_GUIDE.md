# Presence Implementation Guide using GetStream.io

## Overview

Sidechain uses GetStream.io's Chat API for user presence tracking. This guide explains how to implement the presence TODOs in the plugin (`Presence.cpp` line 17 and line 40) and backend.

## GetStream.io Presence Features

### User Presence Fields
Each GetStream.io user object includes:
- **`online`**: Boolean indicating if user is currently online
- **`status`**: Text field for custom status message (e.g., "in studio", "away")
- **`invisible`**: Boolean to mark user as invisible (appear offline to others)
- **`last_active`**: Timestamp of last activity (updated on connection and every 15 minutes)

### Presence Events
- `user.presence.changed` - Fired when a user's online status changes
- `user.online` - Fired when a user comes online
- `user.offline` - Fired when a user goes offline
- `user.away` - Fired when a user goes away

## Backend Implementation (Go)

### Current Backend Setup

The backend already has a complete `PresenceManager` in `backend/internal/websocket/presence.go` that:
1. Tracks user presence in-memory
2. Broadcasts presence updates to followers via WebSocket
3. Manages presence timeouts (5-minute inactivity = offline)
4. Updates database (`is_online`, `last_active_at` columns)

### Backend Presence Status Types
```go
const (
    StatusOnline   PresenceStatus = "online"
    StatusOffline  PresenceStatus = "offline"
    StatusInStudio PresenceStatus = "in_studio"
)
```

### Creating a REST Endpoint (Future Enhancement)

To add an HTTP endpoint for the plugin to update presence:

```go
// backend/internal/handlers/presence.go

// UpdatePresenceRequest is the request body for updating presence
type UpdatePresenceRequest struct {
    Status  string `json:"status" binding:"required"` // "online", "away", "dnd", "offline"
    Message string `json:"message"`                   // Optional status message
    DAW     string `json:"daw"`                       // Optional DAW name
}

// UpdatePresenceStatus updates the current user's presence
// PUT /api/v1/presence/status
func (h *Handlers) UpdatePresenceStatus(c *gin.Context) {
    currentUser, ok := util.GetUserFromContext(c)
    if !ok {
        return
    }

    var req UpdatePresenceRequest
    if err := c.ShouldBindJSON(&req); err != nil {
        util.RespondBadRequest(c, "invalid_request", "Invalid request body")
        return
    }

    // Convert status string to PresenceStatus
    status := websocket.PresenceStatus(req.Status)
    
    // Validate status
    if status != websocket.StatusOnline && 
       status != websocket.StatusOffline && 
       status != websocket.StatusInStudio {
        status = websocket.StatusOnline
    }

    // Update presence via manager
    h.PresenceManager.UpdatePresence(currentUser.ID, currentUser.Username, status, req.DAW)

    // Update user status in database
    database.DB.Model(currentUser).Update("status_message", req.Message)

    c.JSON(http.StatusOK, gin.H{
        "status":  "success",
        "message": "presence_updated",
    })
}

// GetPresence retrieves presence for multiple users
// GET /api/v1/presence/users?ids=user1,user2,user3
func (h *Handlers) GetPresence(c *gin.Context) {
    userIDsStr := c.Query("ids")
    if userIDsStr == "" {
        util.RespondBadRequest(c, "missing_ids", "ids query parameter required")
        return
    }

    userIDs := strings.Split(userIDsStr, ",")
    onlineUsers := h.PresenceManager.GetOnlinePresence(userIDs)

    c.JSON(http.StatusOK, gin.H{
        "status": "success",
        "users":  onlineUsers,
    })
}
```

### Register Routes
```go
// In backend/main.go or your router setup:
router.PUT("/api/v1/presence/status", handlers.UpdatePresenceStatus)
router.GET("/api/v1/presence/users", handlers.GetPresence)
```

## Plugin Implementation (C++)

### Step 1: Add to NetworkClient

Add methods to `plugin/src/network/NetworkClient.h`:

```cpp
// Presence callback
using PresenceStatusCallback = std::function<void(Outcome<juce::var> response)>;

// Update current user's presence status
void setPresenceStatus(const juce::String& status, 
                      const juce::String& message = "",
                      const juce::String& daw = "",
                      PresenceStatusCallback callback = nullptr);

// Get presence for multiple users
void getPresenceForUsers(const juce::StringArray& userIds,
                        ResponseCallback callback = nullptr);
```

### Step 2: Implement in NetworkClient.cpp

```cpp
void NetworkClient::setPresenceStatus(const juce::String& status,
                                     const juce::String& message,
                                     const juce::String& daw,
                                     PresenceStatusCallback callback) {
    auto requestBody = std::make_shared<juce::DynamicObject>();
    requestBody->setProperty("status", status);
    if (message.isNotEmpty()) {
        requestBody->setProperty("message", message);
    }
    if (daw.isNotEmpty()) {
        requestBody->setProperty("daw", daw);
    }

    performRequest(
        "PUT",
        "/api/v1/presence/status",
        juce::JSON::toString(juce::var(requestBody.get())),
        [callback](const Outcome<juce::String> &result) {
            if (!callback) return;
            
            if (result.isError()) {
                callback(Outcome<juce::var>::makeError(result.getError()));
                return;
            }

            auto response = juce::JSON::parse(result.getValue());
            callback(Outcome<juce::var>::makeSuccess(response));
        }
    );
}

void NetworkClient::getPresenceForUsers(const juce::StringArray& userIds,
                                       ResponseCallback callback) {
    if (userIds.isEmpty()) {
        if (callback) callback(Outcome<juce::var>::makeError("No user IDs provided"));
        return;
    }

    juce::String idsParam = userIds.joinIntoString(",");
    performRequest(
        "GET",
        "/api/v1/presence/users?ids=" + idsParam,
        "",
        [callback](const Outcome<juce::String> &result) {
            if (!callback) return;
            
            if (result.isError()) {
                callback(Outcome<juce::var>::makeError(result.getError()));
                return;
            }

            auto response = juce::JSON::parse(result.getValue());
            callback(Outcome<juce::var>::makeSuccess(response));
        }
    );
}
```

### Step 3: Integrate with AppStore

In `plugin/src/stores/AppStore.h`, add a method:

```cpp
// Update current user's presence status
// This sends to backend and updates local presence state
void updatePresenceStatus(const juce::String& status,
                         const juce::String& message = "",
                         const juce::String& daw = "");

// Get presence for multiple users
void queryPresenceForUsers(const juce::StringArray& userIds);
```

### Step 4: Implement Presence.cpp TODOs

#### TODO Line 17: Send status message to server

```cpp
void AppStore::setPresenceStatusMessage(const juce::String &message) {
    // Get current status
    auto currentStatus = sliceManager.getPresenceSlice()->getState().currentUserStatus;
    
    // Send to backend
    if (networkClient) {
        networkClient->setPresenceStatus(
            PresenceStatusToString(currentStatus),
            message,
            "", // DAW will be auto-detected by backend
            [this](Outcome<juce::var> result) {
                if (result.isError()) {
                    Util::logError("AppStore", "Failed to update presence message: " + result.getError());
                    return;
                }
                Util::logInfo("AppStore", "Presence status message sent successfully");
            }
        );
    }

    // Update local state
    sliceManager.getPresenceSlice()->dispatch([message](PresenceState &state) {
        state.statusMessage = message;
    });
}

// Helper function
static juce::String PresenceStatusToString(PresenceStatus status) {
    switch (status) {
        case PresenceStatus::Online:
            return "online";
        case PresenceStatus::Away:
            return "away";
        case PresenceStatus::DoNotDisturb:
            return "dnd";
        case PresenceStatus::Offline:
            return "offline";
        default:
            return "online";
    }
}
```

#### TODO Line 40: Establish WebSocket connection to presence service

The backend already handles WebSocket connections through `backend/internal/websocket/hub.go`. For the plugin, presence updates happen through:

1. **HTTP requests** to the REST endpoint (for status changes)
2. **Real-time updates** via the existing StreamChatClient WebSocket connection

The presence updates broadcast to followers through the backend's PresenceManager, which:
- Gets followers from GetStream.io
- Sends messages to online followers via WebSocket
- Updates the database with `is_online` and `last_active_at`

The "WebSocket connection" is already established when the user connects to the chat WebSocket. Simply update presence via the REST endpoint:

```cpp
void AppStore::connectPresence() {
    if (!networkClient) {
        Util::logError("AppStore", "Cannot connect presence - network client not set");
        return;
    }

    sliceManager.getPresenceSlice()->dispatch([](PresenceState &state) {
        state.isUpdatingPresence = true;
    });

    Util::logInfo("AppStore", "Connecting to presence service");

    // Send initial online status to backend
    // The backend will:
    // 1. Update presence in GetStream.io via SetUserPresence API
    // 2. Broadcast to followers via WebSocket
    // 3. Update database
    networkClient->setPresenceStatus(
        "online",
        "", // message
        "", // daw (auto-detected)
        [this](Outcome<juce::var> result) {
            sliceManager.getPresenceSlice()->dispatch([result](PresenceState &state) {
                state.isUpdatingPresence = false;
                if (!result.isError()) {
                    state.isConnected = true;
                    state.currentUserStatus = PresenceStatus::Online;
                    state.currentUserLastActivity = juce::Time::getCurrentTime().toMilliseconds();
                    Util::logInfo("AppStore", "Connected to presence service");
                } else {
                    Util::logError("AppStore", "Failed to connect to presence: " + result.getError());
                }
            });
        }
    );
}
```

### Step 5: Add Heartbeat Timer

To keep the user marked as "online", add periodic heartbeat updates:

```cpp
// In AppStore.h
std::unique_ptr<juce::Timer> presenceHeartbeatTimer;

// In AppStore.cpp constructor
class PresenceHeartbeatTimer : public juce::Timer {
public:
    PresenceHeartbeatTimer(AppStore* store) : appStore(store) {}
    void timerCallback() override {
        // Send heartbeat every 4 minutes (within 5-minute timeout)
        appStore->recordUserActivity();
    }
private:
    AppStore* appStore;
};

// Start heartbeat when user connects
void AppStore::connectPresence() {
    // ... existing code ...
    
    // Start heartbeat timer (every 4 minutes)
    presenceHeartbeatTimer = std::make_unique<PresenceHeartbeatTimer>(this);
    presenceHeartbeatTimer->startTimer(4 * 60 * 1000);
}

// Stop heartbeat when disconnecting
void AppStore::disconnectPresence() {
    if (presenceHeartbeatTimer) {
        presenceHeartbeatTimer->stopTimer();
        presenceHeartbeatTimer = nullptr;
    }
    // ... existing code ...
}
```

## Integration with UI

### Activity Status Settings
Users can control presence visibility in settings:
- `show_activity_status` - Whether to broadcast presence to followers
- `show_last_active` - Whether to show last active time on profile

Implement in plugin UI settings dialog:

```cpp
void ActivityStatusSettingsDialog::updateSettings() {
    if (networkClient) {
        // Update privacy settings
        networkClient->updateActivityStatusSettings(
            showActivityStatus,
            showLastActive,
            [this](Outcome<juce::var> result) {
                if (result.isError()) {
                    showErrorMessage("Failed to update settings");
                }
            }
        );
    }
}
```

## Database Schema

Ensure these columns exist in the `users` table:

```sql
ALTER TABLE users ADD COLUMN is_online BOOLEAN DEFAULT false;
ALTER TABLE users ADD COLUMN last_active_at TIMESTAMP;
ALTER TABLE users ADD COLUMN status_message VARCHAR(255);
ALTER TABLE users ADD COLUMN show_activity_status BOOLEAN DEFAULT true;
ALTER TABLE users ADD COLUMN show_last_active BOOLEAN DEFAULT true;
```

## Testing

1. **Local Development**:
   - Run backend with PresenceManager
   - Connect plugin to backend
   - Check database for `is_online` updates
   - Monitor WebSocket messages in browser DevTools

2. **Follower Notifications**:
   - User A follows User B
   - User B sets status to "in studio"
   - User A should receive presence update via WebSocket

3. **Timeout Behavior**:
   - Disconnect user without sending offline
   - Wait 5+ minutes
   - User should be marked offline
   - Check PresenceManager logs

## GetStream.io Integration (Future)

Currently, the backend broadcasts presence to followers via WebSocket. To also sync with GetStream.io's presence:

```go
// In PresenceManager.UpdatePresence()
if streamClient != nil {
    // Update user's invisible status in GetStream
    streamClient.UpdateUserPartial(ctx, userID, map[string]interface{}{
        "invisible": status == StatusOffline,
    })
}
```

## References

- GetStream.io Presence API: https://getstream.io/chat/docs/go/presence_format/
- Backend WebSocket: `backend/internal/websocket/presence.go`
- Backend Activity Status: `backend/internal/handlers/activity_status.go`
- Plugin Presence State: `plugin/src/stores/app/PresenceState.h`
