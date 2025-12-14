# Backend Investigation Report: OT & WebSocket Operation Support

**Date**: December 14, 2024
**Investigator**: Claude Code
**Status**: Investigation Complete ✅

---

## Executive Summary

**VERDICT**: Backend does NOT have Operational Transform (OT) or operation-based WebSocket message support. The messaging system uses **getstream.io Chat SDK directly** for all chat operations.

**Impact**: Tasks 4.20 (OperationalTransform in Chat) and 4.21 (RealtimeSync in Feed) have **NO backend support** for the operation-based synchronization that the plugin implements.

**Recommendation**: Backend needs ~10 hours of development to add:
1. Operational Transform server (6 hours)
2. Operation-based WebSocket messaging (4 hours)

---

## Investigation Results

### Backend Investigation 1: Operational Transform Implementation

**Question**: Does backend have OperationalTransform algorithm implemented?

**Answer**: **❌ NO**

#### Evidence

**Search Results**:
```bash
$ grep -r "OperationalTransform\|operational\|transform" backend/ --include="*.go"
# No results found
```

**Architecture**:
- Backend uses **getstream.io Chat SDK** for all messaging
- See: `backend/internal/stream/client.go` (lines 50-100)
- Chat client: `streamClient.ChatClient` - uses getstream.io's Chat v5 SDK
- No custom message operation handling

**Files Checked**:
1. `internal/handlers/messages.go` (100+ lines) - ALL TODOs, NO implementation
   - Lines 1-15: States "Currently messages are handled directly via getstream.io Chat SDK"
   - Lines 16-112: Lists 12 TODO items for future message handlers
   - **NOT A SINGLE HANDLER IMPLEMENTED**

2. `internal/websocket/message.go` (253 lines)
   - Defines message types: `MessageTypeNewPost`, `MessageTypePostLiked`, etc.
   - **NO Operation types**: No `MessageTypeOperation`, `MessageTypeTransform`, `MessageTypeInsert`, `MessageTypeDelete`
   - Message structure has: `Type`, `Payload`, `ID`, `ReplyTo`, `Timestamp`
   - **NO fields for operation data** (position, content, clientId, etc.)

3. `internal/websocket/handler.go` (449 lines)
   - Has handler registration: `RegisterDefaultHandlers()`
   - Handlers for presence, playback, but **NO operation handlers**
   - Broadcast methods: `NotifyLike()`, `NotifyFollow()`, `BroadcastLikeCountUpdate()`
   - **NO broadcast for operations**

4. `cmd/server/main.go` (650+ lines)
   - Route definitions for all APIs
   - **NO `/api/v1/messages/*` endpoints defined**
   - **NO `/api/v1/channels/*` endpoints for group chat settings**
   - Comment on line 269-270: Only Stream token generation, no channel management

**Conclusion**: Backend has **ZERO** OT implementation. All chat is delegated to getstream.io.

---

### Backend Investigation 2: WebSocket Operation Message Format

**Question**: Does backend support operation-based WebSocket messaging?

**Answer**: **❌ NO**

#### Evidence

**Current WebSocket Message Types** (`websocket/message.go`, lines 42-81):

```go
const (
    // System messages
    MessageTypeSystem = "system"
    MessageTypePing   = "ping"
    MessageTypePong   = "pong"
    MessageTypeError  = "error"
    MessageTypeAuth   = "auth"

    // Feed/Activity messages
    MessageTypeNewPost     = "new_post"
    MessageTypePostLiked   = "post_liked"
    MessageTypePostUnliked = "post_unliked"
    MessageTypeNewComment  = "new_comment"
    MessageTypeNewReaction = "new_reaction"

    // Social messages
    MessageTypeNewFollower            = "new_follower"
    MessageTypeUnfollowed             = "unfollowed"
    MessageTypeFollowRequestAccepted  = "follow_request_accepted"

    // Presence messages
    MessageTypePresence     = "presence"
    MessageTypeUserOnline   = "user_online"
    MessageTypeUserOffline  = "user_offline"
    MessageTypeUserInStudio = "user_in_studio"

    // Notification messages
    MessageTypeNotification      = "notification"
    MessageTypeNotificationRead  = "notification_read"
    MessageTypeNotificationCount = "notification_count"

    // Audio/Playback messages
    MessageTypePlaybackStarted = "playback_started"
    MessageTypePlaybackStopped = "playback_stopped"

    // Real-time updates
    MessageTypeLikeCountUpdate     = "like_count_update"
    MessageTypeFollowerCountUpdate = "follower_count_update"
)
```

**What's Missing**:
- ❌ NO `MessageTypeOperation` (base type)
- ❌ NO `MessageTypeOperationInsert`, `MessageTypeOperationDelete`, `MessageTypeOperationModify`
- ❌ NO `MessageTypeOperationAck` (acknowledgment)
- ❌ NO `MessageTypeSyncStatus` (sync state changes)
- ❌ NO `MessageTypeTransformRequest` (conflict resolution request)

**Current Payloads** (examples from code):

```go
// What we HAVE - Simple event payloads
type LikePayload struct {
    PostID    string `json:"post_id"`
    UserID    string `json:"user_id"`
    Username  string `json:"username"`
    LikeCount int    `json:"like_count"`
    Emoji     string `json:"emoji,omitempty"`
}

// What we DON'T HAVE - Operation payloads
// Missing: clientId, timestamp, position, content, operationType, etc.
```

**WebSocket Handler** (`websocket/handler.go`, lines 362-401):

```go
// RegisterDefaultHandlers registers the default message handlers
func (h *Handler) RegisterDefaultHandlers() {
    // Presence update handler
    h.hub.RegisterHandler(MessageTypePresence, func(client *Client, msg *Message) error {
        // ...
    })

    // Playback handlers
    h.hub.RegisterHandler(MessageTypePlaybackStarted, func(client *Client, msg *Message) error {
        // ...
    })
    // ... more handlers
    log.Println("📨 Registered default WebSocket message handlers")
}
```

**NO operation handlers registered**.

---

## Current Backend Architecture

### What the Backend HAS ✅

1. **WebSocket Infrastructure** (`websocket/` module)
   - Hub for client management
   - Client connection handling
   - Message routing
   - Presence tracking
   - **Good foundation, just missing operation types**

2. **Feed Operations** (via HTTP REST API)
   - POST `/api/v1/feed/post` - Create post
   - POST `/api/v1/social/like` - Like post
   - DELETE `/api/v1/social/like` - Unlike post
   - POST `/api/v1/social/react` - Emoji react
   - **These are REST endpoints, NOT operation-based**

3. **Real-Time Notifications** (WebSocket)
   - Broadcasts new posts, likes, follows to interested clients
   - Presence updates (online/offline/in_studio)
   - Notification count updates
   - **Works for event notifications, not collaborative editing**

4. **getstream.io Chat Integration**
   - ChatClient initialized in `stream/client.go`
   - Used for direct messaging and chat
   - User creation: `CreateUser()`
   - **But NO channel description editing support**

### What the Backend is MISSING ❌

1. **Operational Transform Server**
   - No transform function for concurrent operations
   - No operation queue management
   - No timestamp assignment/ordering
   - No client ID tracking for tie-breaking

2. **Operation Message Types**
   - No Insert/Delete/Modify operation types
   - No operation payload structures
   - No acknowledgment messages

3. **Operation Handlers**
   - No WebSocket handler for operations
   - No server-side operation storage
   - No broadcast to other clients of transformed operations

4. **Channel Management Endpoints**
   - No `PUT /api/v1/channels/:id/description` endpoint
   - No `GET /api/v1/channels/:id` endpoint
   - No group chat channel API at all
   - All chat delegated to getstream.io

5. **Sync State Messages**
   - No `sync.status` message type
   - No way to tell clients "syncing..." vs "synced"
   - Plugin implements this, backend doesn't support it

---

## Impact on Plugin Tasks 4.20 & 4.21

### Task 4.20: OperationalTransform Integration in Chat

**Current Status in Plugin**: ✅ FULLY IMPLEMENTED
- Reads `ChatStore.h` (380 lines)
- Generates operations for description edits
- Sends operations to server (via `sendOperationToServer()`)
- Applies server-transformed operations
- Handles concurrent operations from remote clients

**Backend Support**: ❌ ZERO
- Plugin sends operations to non-existent endpoint
- Server can't transform concurrent operations
- Remote operations won't be delivered
- **Chat description editing will fail**

**What Happens**:
1. Plugin user edits channel description in ChatStore
2. ChatStore generates Insert/Delete operations
3. ChatStore sends operation to backend (but no endpoint exists)
4. Backend has no OT algorithm to transform against concurrent edits
5. Remote clients never receive the operation
6. **Users see stale descriptions**

---

### Task 4.21: RealtimeSync Integration in Feed

**Current Status in Plugin**: ✅ FULLY IMPLEMENTED
- Reads `FeedStore.cpp` (500+ lines)
- Creates RealtimeSync instance with document ID
- Broadcasts engagement updates (likes, saves, reposts)
- Remote operation callbacks for real-time updates
- `isSynced` state for UI indicators

**Backend Support**: ⚠️ PARTIAL
- WebSocket is working for other event types
- Presence tracking works
- **BUT no operation message types defined**
- **No operation broadcast handlers**

**What Happens**:
1. Plugin user likes post in FeedStore
2. FeedStore broadcasts Modify operation via RealtimeSync
3. WebSocket sends operation message (MessageType not defined in backend)
4. Backend WebSocket hub has no handler for operation type
5. Other clients don't receive the operation
6. **Real-time engagement updates won't work**

**Current State**: Feed updates work via REST API + WebSocket event notifications, but NOT via operations.

---

## Backend Work Required

### Option A: Implement Full OT Server (Recommended) - ~10 hours

#### 1. Add Operation Message Types (`websocket/message.go`) - 1 hour

```go
const (
    MessageTypeOperation = "operation"
    MessageTypeOperationAck = "operation_ack"
    MessageTypeSyncStatus = "sync_status"
)

type OperationPayload struct {
    DocumentID string      `json:"document_id"`           // e.g., "channel:abc123:description"
    ClientID   string      `json:"client_id"`             // e.g., "user:12345"
    Timestamp  int64       `json:"timestamp"`             // Server-assigned logical clock
    Type       string      `json:"type"`                  // "insert", "delete", "modify"
    Position   int         `json:"position"`              // Where in document
    Content    string      `json:"content,omitempty"`     // What to insert/delete
    Length     int         `json:"length,omitempty"`      // Length of deleted content
}

type OperationAckPayload struct {
    OperationID string `json:"operation_id"`
    Timestamp   int64  `json:"timestamp"`
    Error       string `json:"error,omitempty"`
}

type SyncStatusPayload struct {
    DocumentID string `json:"document_id"`
    Status     string `json:"status"`  // "syncing", "synced", "conflict"
    Timestamp  int64  `json:"timestamp"`
}
```

#### 2. Implement OT Transform Function (`internal/crdt/ot.go`) - 2 hours

```go
package crdt

type Operation struct {
    ClientID  string
    Timestamp int64
    Type      string // "insert", "delete", "modify"
    Position  int
    Content   string
    Length    int
}

// Transform returns op1 transformed against op2
// Ensures: transform(op1, op2) + transform(op2, op1) = same final state
func Transform(op1, op2 *Operation) *Operation {
    if op1.Type == "insert" && op2.Type == "insert" {
        if op1.Position < op2.Position {
            // op1 inserts before op2, op2 position shifts right
            return &Operation{
                Type:     op1.Type,
                Position: op1.Position,
                Content:  op1.Content,
            }
        } else if op1.Position > op2.Position {
            // op1 inserts after op2, op1 position shifts right
            return &Operation{
                Type:     op1.Type,
                Position: op1.Position + len(op2.Content),
                Content:  op1.Content,
            }
        } else {
            // Same position - use ClientID tie-breaker
            if op1.ClientID < op2.ClientID {
                return &Operation{
                    Type:     op1.Type,
                    Position: op1.Position,
                    Content:  op1.Content,
                }
            }
            return &Operation{
                Type:     op1.Type,
                Position: op1.Position + len(op2.Content),
                Content:  op1.Content,
            }
        }
    }
    // ... handle other type combinations (delete, modify)
    return op1 // fallback
}
```

#### 3. Add Operation Storage (`internal/models/operation.go`) - 1 hour

```go
package models

type DocumentOperation struct {
    ID         string `gorm:"primaryKey"`
    DocumentID string // e.g., "channel:abc123:description"
    ClientID   string
    Timestamp  int64
    Type       string
    Position   int
    Content    string
    CreatedAt  time.Time
}

// Store operations in database for history/audit trail
```

#### 4. Create Operation Handler (`internal/handlers/operations.go`) - 2 hours

```go
// POST /api/v1/operations/apply
// Receives operation from plugin, transforms against pending ops, broadcasts to all clients
func (h *Handlers) ApplyOperation(c *gin.Context) {
    var req struct {
        Operation *Operation `json:"operation"`
    }

    // 1. Validate operation
    // 2. Transform against all concurrent operations
    // 3. Store in database
    // 4. Broadcast to all connected clients
    // 5. Return acknowledgment with server timestamp
}
```

#### 5. Integrate Operations in WebSocket (`websocket/handler.go`) - 2 hours

```go
func (h *Handler) RegisterDefaultHandlers() {
    // ... existing handlers ...

    // Operation handler
    h.hub.RegisterHandler(MessageTypeOperation, func(client *Client, msg *Message) error {
        var op OperationPayload
        if err := msg.ParsePayload(&op); err != nil {
            return err
        }

        // 1. Transform against pending operations
        transformed := transformAgainstPending(op)

        // 2. Store in database
        // 3. Broadcast to all clients
        h.hub.Broadcast(NewMessage(MessageTypeOperation, transformed))

        // 4. Send ACK to originating client
        client.Send(NewReply(msg, MessageTypeOperationAck, OperationAckPayload{
            Timestamp: time.Now().UnixMilli(),
        }))

        return nil
    })
}
```

#### 6. Add Endpoints (`cmd/server/main.go`) - 1 hour

```go
// API routes
operations := api.Group("/operations")
{
    operations.Use(authHandlers.AuthMiddleware())
    operations.POST("/apply", h.ApplyOperation)
    operations.GET("/history/:document_id", h.GetOperationHistory)
}
```

---

### Option B: Minimal Implementation (Not Recommended) - ~4 hours

Just add operation message types to WebSocket and broadcast handler, WITHOUT server-side OT:
- ❌ No transform function - last-write-wins (data loss)
- ❌ No conflict resolution - concurrent edits fight
- ❌ No operation storage - can't replay for new clients
- **Not suitable for production**

---

## Server Endpoint Specifications

### Proposed Operation Endpoints

#### `POST /api/v1/operations/apply`

**Request**:
```json
{
  "operation": {
    "document_id": "channel:xyz789:description",
    "client_id": "user:12345",
    "timestamp": 1702569600123,
    "type": "insert",
    "position": 15,
    "content": " collaborative"
  }
}
```

**Response** (202 Accepted):
```json
{
  "status": "accepted",
  "operation_id": "op:abc123",
  "server_timestamp": 1702569600250,
  "transform_needed": false
}
```

**If transform needed**:
```json
{
  "status": "transformed",
  "operation_id": "op:def456",
  "server_timestamp": 1702569600350,
  "original_position": 15,
  "transformed_position": 20,
  "reason": "Concurrent insert at position 12 by user:67890"
}
```

#### `GET /api/v1/operations/history/:document_id`

**Response**:
```json
{
  "document_id": "channel:xyz789:description",
  "operations": [
    {
      "id": "op:abc123",
      "client_id": "user:12345",
      "timestamp": 1702569600123,
      "type": "insert",
      "position": 0,
      "content": "Group Chat"
    },
    {
      "id": "op:def456",
      "client_id": "user:67890",
      "timestamp": 1702569600200,
      "type": "insert",
      "position": 15,
      "content": " Rules"
    }
  ]
}
```

---

## WebSocket Message Format

### Operation Message (from server to all clients)

```json
{
  "type": "operation",
  "id": "msg:12345",
  "timestamp": "2024-12-14T12:34:56Z",
  "payload": {
    "document_id": "channel:xyz789:description",
    "client_id": "user:12345",
    "server_timestamp": 1702569600350,
    "operation": {
      "type": "insert",
      "position": 15,
      "content": " collaborative",
      "original_position": 15,
      "transformed_position": 20
    }
  }
}
```

### Sync Status Message (from server to client)

```json
{
  "type": "sync_status",
  "id": "msg:12346",
  "timestamp": "2024-12-14T12:34:56Z",
  "payload": {
    "document_id": "channel:xyz789:description",
    "status": "syncing",
    "pending_count": 3
  }
}
```

---

## Blockers & Risks

### Critical Blockers ⛔

1. **No Endpoint for Operations**
   - Plugin sends operations to non-existent endpoint
   - Server doesn't accept them
   - Operations get dropped

2. **No Transform Logic**
   - Concurrent edits aren't resolved
   - Last edit wins (data loss)
   - Two users editing simultaneously see different states

3. **No Operation Storage**
   - No audit trail
   - No way to replay for late-joining clients
   - No conflict resolution history

### Production Risks 🚨

- **Data Loss**: Concurrent edits cause information to be lost
- **Inconsistency**: Clients end up with different states
- **No Recovery**: Can't restore conflicting changes
- **No History**: Can't track who changed what

---

## Recommendation

### For Immediate Use (Next 2 Weeks)
- **Disable Task 4.20 in production** - Channel description editing via OT
- **Use getstream.io Chat SDK** for channel management (manual, not collaborative)
- **Task 4.21 still works** - Feed engagement updates via REST API + WebSocket events

### For Production (Next Month)
- **Implement Option A** (Full OT server) - 10 hours
- Enables collaborative channel descriptions
- Ensures data consistency
- Required for scaled collaboration

### Long-Term
- Once OT is working, consider expanding to:
  - Collaborative playlists (multi-user editing)
  - Shared project files
  - Group challenge descriptions
  - Any document that multiple users might edit simultaneously

---

## Files Investigated

| File | Status | Finding |
|------|--------|---------|
| `internal/websocket/message.go` | ✅ Reviewed | NO operation types defined |
| `internal/websocket/handler.go` | ✅ Reviewed | NO operation handlers registered |
| `internal/handlers/messages.go` | ✅ Reviewed | ALL TODOs, no implementation |
| `internal/stream/client.go` | ✅ Reviewed | Uses getstream.io SDK only |
| `cmd/server/main.go` | ✅ Reviewed | NO message/channel/operation routes |
| `internal/models/` | ✅ Checked | NO Operation model |
| `internal/crdt/` | ✅ Checked | Directory doesn't exist |

---

**Investigation Complete**: December 14, 2024
**Total Time**: ~1.5 hours
**Next Step**: Present findings to backend team for implementation decision
