# Backend Investigation Notes - Task 4.20-4.21 Requirements

**Date**: December 14, 2025
**Investigator**: Claude Code
**Context**: Assessing backend support for Plugin real-time collaboration and error tracking features

---

## Executive Summary

**Status**: ⚠️ PARTIAL SUPPORT

The backend has **excellent WebSocket infrastructure** for real-time features, but **lacks critical Operational Transform (OT) support** and **error tracking API**. This will require ~8 hours of backend work to fully enable Tasks 4.20-4.21.

| Component | Status | Impact | Effort |
|-----------|--------|--------|--------|
| **WebSocket Real-Time** | ✅ DONE | High | 0 hours |
| **Rate Limiting** | ✅ DONE | Medium | 0 hours |
| **Operational Transform** | ❌ NOT IMPLEMENTED | CRITICAL | 6 hours |
| **Error Tracking API** | ❌ NOT IMPLEMENTED | Medium | 2 hours |
| **Operation Broadcasting** | ❌ NOT IMPLEMENTED | CRITICAL | 1-2 hours |

---

## 1. WebSocket Real-Time Infrastructure ✅ DONE

### What Backend Has
✅ **Excellent WebSocket foundation** at `/ws` endpoint:
- JWT authentication with token validation
- Client connection/disconnection handling
- Message routing with handler registration
- Presence tracking (online/offline/in_studio)
- Broadcast capabilities to all users or specific users

### Message Types Supported
```
System Messages:
- system, ping, pong, error, auth

Feed/Activity:
- new_post, post_liked, post_unliked, new_comment, new_reaction

Social:
- new_follower, unfollowed, follow_request_accepted

Presence:
- presence, user_online, user_offline, user_in_studio

Notifications:
- notification, notification_read, notification_count

Audio/Playback:
- playback_started, playback_stopped

Real-time Updates:
- like_count_update, follower_count_update
```

### Location
- **Handler**: `backend/internal/websocket/handler.go`
- **Hub**: `backend/internal/websocket/hub.go`
- **Messages**: `backend/internal/websocket/message.go`
- **Client**: `backend/internal/websocket/client.go`

### How It Works
1. Client connects to `wss://api.sidechain.live/ws?token=JWT`
2. Server verifies JWT and creates client in hub
3. Hub routes messages to handlers based on message type
4. Handlers can broadcast to specific users or all users
5. Presence manager tracks online status

### Code Quality
- ✅ Thread-safe (proper mutex locking)
- ✅ Proper error handling
- ✅ Graceful shutdown support
- ✅ Metrics available at `/api/v1/websocket/metrics`

### Plugin Integration Status
✅ Plugin WebSocket client can:
- Connect with JWT token
- Send presence updates
- Receive real-time notifications
- Sync engagement counts

---

## 2. Rate Limiting ✅ DONE

### Backend Rate Limiting Implementation
✅ **Properly implemented** at `backend/internal/middleware/ratelimit.go`

#### Features
- ✅ Returns HTTP 429 (Too Many Requests)
- ✅ Sets `Retry-After` header (seconds)
- ✅ Sets `X-RateLimit-*` headers
- ⚠️ **Bug found**: `Retry-After` header uses `string(rune())` conversion (line 107) - converts to single character instead of number
  - Current: `c.Header("Retry-After", string(rune(retryAfter)))` → converts 60 to "`" character
  - Should be: `c.Header("Retry-After", strconv.Itoa(retryAfter))`

#### Limits Configured
```go
// API requests: 100/minute (global, by IP)
DefaultRateLimitConfig(): 100 req/min

// Auth endpoints: 10/minute (stricter for security)
AuthRateLimitConfig(): 10 req/min

// File uploads: 20/minute (by user ID or IP)
UploadRateLimitConfig(): 20 req/min
```

#### Endpoints Using Rate Limiting
- `POST /api/v1/auth/*` - Auth endpoints (10/min)
- `POST /api/v1/audio/upload` - Audio uploads (20/min)
- `POST /api/v1/users/upload-profile-picture` - Profile picture uploads (20/min)

#### Plugin Status
✅ Plugin rate limiter can honor server 429 responses because:
- Server returns 429 with `Retry-After` header
- Client-side limiter enforces same limits (100 API, 10 uploads/hour)
- When server 429 received, plugin waits the `Retry-After` time before retrying

**RECOMMENDATION**: Fix the `Retry-After` header bug in backend:
```go
// Fix line 107 in ratelimit.go
c.Header("Retry-After", strconv.Itoa(retryAfter))
```

---

## 3. Operational Transform Support ❌ NOT IMPLEMENTED

### What Plugin Expects
Plugin's OperationalTransform implementation (Task 4.20) expects server to:

1. **Receive operations from clients**:
```json
{
  "operation": {
    "type": "Insert|Delete",
    "clientId": 12345,
    "timestamp": 1702569600,
    "position": 42,
    "content": "new text"
  },
  "documentId": "channel:abc123:description"
}
```

2. **Transform concurrent operations** - Handle this scenario:
   - Client A: Insert "hello" at position 0
   - Client B: Insert "world" at position 0 (concurrent)
   - Server must transform A against B (or vice versa) to ensure convergence

3. **Apply to server state** - Update the document in database

4. **Broadcast to all clients** - Send transformed operation to everyone
   - Include new position/content after transformation
   - Send acknowledgment with server timestamp

5. **Maintain operation history** - For crash recovery and sync

### What Backend Has
❌ **Zero OT implementation found**

Searching through backend code:
- No `operation` message type
- No `transform` functions
- No operation queue or history
- No document state management for collaborative editing

### Required Backend Work (6 hours)

#### 1. Define Operation Message Format (30 min)
Add to `backend/internal/websocket/message.go`:
```go
const (
    MessageTypeOperation = "operation"
    MessageTypeOperationAck = "operation_ack"
)

type OperationPayload struct {
    DocumentID string      `json:"document_id"`    // channel:id:field
    Operation  struct {
        Type      string `json:"type"`             // "Insert" or "Delete"
        ClientID  int    `json:"client_id"`
        Timestamp int64  `json:"timestamp"`
        Position  int    `json:"position"`
        Content   string `json:"content,omitempty"`
    } `json:"operation"`
}

type OperationAckPayload struct {
    DocumentID       string `json:"document_id"`
    ClientID         int    `json:"client_id"`
    ServerTimestamp  int64  `json:"server_timestamp"`
    TransformedOp    interface{} `json:"transformed_operation"`
}
```

#### 2. Create OT Transform Function (1.5 hours)
Create `backend/internal/crdt/operational_transform.go`:
```go
// TransformOperation handles concurrent edit convergence
func TransformOperation(
    op1, op2 *Operation,
    context int, // tie-breaker for same position
) (*Operation, *Operation) {
    // Transform op1 against op2
    // If both insert at same position, use context (usually client ID)
    // Return transformed versions of both
}

// ApplyOperation applies an operation to a document
func ApplyOperation(doc string, op *Operation) string {
    switch op.Type {
    case "Insert":
        return doc[:op.Position] + op.Content + doc[op.Position:]
    case "Delete":
        return doc[:op.Position] + doc[op.Position+len(op.Content):]
    }
    return doc
}
```

#### 3. Database Schema for Operations (1 hour)
Add migration to store operation history:
```sql
CREATE TABLE operations (
    id UUID PRIMARY KEY,
    document_id VARCHAR(255),
    client_id INT,
    type VARCHAR(20),
    position INT,
    content TEXT,
    timestamp BIGINT,
    server_timestamp BIGINT,
    created_at TIMESTAMP
);

CREATE TABLE documents (
    id VARCHAR(255) PRIMARY KEY,
    current_content TEXT,
    last_operation_timestamp BIGINT,
    created_at TIMESTAMP,
    updated_at TIMESTAMP
);
```

#### 4. WebSocket Handler for Operations (1.5 hours)
Add to `backend/internal/websocket/handler.go`:
```go
h.hub.RegisterHandler(MessageTypeOperation, func(client *Client, msg *Message) error {
    var opPayload OperationPayload
    if err := msg.ParsePayload(&opPayload); err != nil {
        return err
    }

    // Get current document state
    doc := getDocument(opPayload.DocumentID)

    // Transform against pending operations
    transformedOp := transformOperation(opPayload.Operation, doc.PendingOps)

    // Apply to current document
    doc.Content = applyOperation(doc.Content, transformedOp)
    doc.History = append(doc.History, transformedOp)

    // Save to database
    saveDocument(doc)

    // Broadcast transformed operation to all clients
    h.hub.Broadcast(NewMessage(MessageTypeOperation, OperationAckPayload{
        DocumentID: opPayload.DocumentID,
        ClientID: opPayload.Operation.ClientID,
        ServerTimestamp: time.Now().UnixMilli(),
        TransformedOp: transformedOp,
    }))

    return nil
})
```

#### 5. Additional Fixes (1 hour)
- Add index on `operations.document_id` for fast lookups
- Add cascading delete when channel is deleted
- Add garbage collection for old operations (cleanup after 30 days)

### Impact on Plugin
Without OT backend support, collaborative editing features **won't work**:
- Plugin can execute OT locally, but server won't transform concurrent edits
- All clients will have different final state
- Description field could be corrupted with overwriting edits

---

## 4. Error Tracking API ❌ NOT IMPLEMENTED

### What Plugin Expects
Plugin's ErrorTracker (Task 4.19) expects endpoint:

```http
POST /api/v1/errors/batch
Content-Type: application/json
Authorization: Bearer <token>

{
  "errors": [
    {
      "source": "Network|Audio|UI",
      "severity": "Info|Warning|Error|Critical",
      "message": "Upload rate limit exceeded",
      "context": {
        "endpoint": "/api/upload",
        "retries": 3,
        "http_status": 429
      },
      "timestamp": 1702569600,
      "occurrences": 5
    }
  ]
}
```

### What Backend Has
❌ **No error tracking endpoint**

Searching the codebase:
- No `/api/v1/errors` route
- No error aggregation logic
- No analytics database tables

### Required Backend Work (2 hours)

#### 1. Database Schema (30 min)
```sql
CREATE TABLE error_logs (
    id UUID PRIMARY KEY,
    user_id VARCHAR(36),
    source VARCHAR(50),           -- Network, Audio, UI
    severity VARCHAR(20),         -- Info, Warning, Error, Critical
    message TEXT,
    context JSONB,                -- Error-specific data
    occurrences INT DEFAULT 1,
    first_seen TIMESTAMP,
    last_seen TIMESTAMP,
    is_resolved BOOLEAN DEFAULT FALSE,
    created_at TIMESTAMP
);

CREATE INDEX idx_error_user_time ON error_logs(user_id, created_at DESC);
CREATE INDEX idx_error_severity ON error_logs(severity);
```

#### 2. Handler (1 hour)
Create `backend/internal/handlers/error_tracking.go`:
```go
// RecordErrors saves error batch from plugin
func RecordErrors(c *gin.Context) {
    var req struct {
        Errors []struct {
            Source    string                 `json:"source"`
            Severity  string                 `json:"severity"`
            Message   string                 `json:"message"`
            Context   map[string]interface{} `json:"context"`
            Timestamp int64                  `json:"timestamp"`
            Occurrences int                  `json:"occurrences"`
        } `json:"errors"`
    }

    if err := c.ShouldBindJSON(&req); err != nil {
        c.JSON(400, gin.H{"error": err.Error()})
        return
    }

    userID := c.GetString("user_id")

    // Deduplicate and save
    for _, errLog := range req.Errors {
        // Check if similar error exists in last 1 hour
        existing := findSimilarError(userID, errLog.Message)

        if existing != nil {
            // Increment occurrence count
            existing.Occurrences += errLog.Occurrences
            existing.LastSeen = time.Now()
            existing.Save()
        } else {
            // Create new error log
            log := &ErrorLog{
                UserID: userID,
                Source: errLog.Source,
                Severity: errLog.Severity,
                Message: errLog.Message,
                Context: errLog.Context,
                Occurrences: errLog.Occurrences,
                FirstSeen: time.Now(),
                LastSeen: time.Now(),
            }
            log.Save()
        }
    }

    c.JSON(200, gin.H{"status": "recorded"})
}
```

#### 3. Route (10 min)
Add to main.go:
```go
api.POST("/errors/batch", authHandlers.AuthMiddleware(), errorHandlers.RecordErrors)
```

#### 4. Analytics Queries (20 min)
Add helper functions for error analysis:
```go
// GetErrorStats returns error statistics for dashboard
func GetErrorStats(userID string, hours int) (stats map[string]interface{}, err error) {
    // Count errors by severity
    // Top error messages
    // Error trend (hourly)
}
```

### Impact on Plugin
Without error tracking:
- Plugin collects errors but has nowhere to send them
- Support team has no visibility into user issues
- Can't detect and fix common problems

---

## 5. Operation Broadcasting ⚠️ PARTIAL

### Current Status
✅ Backend can broadcast messages, but:
- No specific handlers for operation messages
- No operation-specific routing
- No history retrieval for sync after reconnect

### What's Needed
When operation message received:
1. ✅ Can send to specific user: `hub.SendToUser(userID, msg)`
2. ✅ Can broadcast to all: `hub.Broadcast(msg)`
3. ❌ Can't replay operation history (for new client sync)
4. ❌ Can't send only to channel members

### Required Changes (1-2 hours)
```go
// Add to Hub
func (h *Hub) SendToChannelMembers(channelID string, msg *Message) {
    h.mu.Lock()
    members := h.getChannelMembers(channelID)
    h.mu.Unlock()

    for _, member := range members {
        h.SendToUser(member.UserID, msg)
    }
}

// Add operation replay for sync
func (h *Hub) GetOperationHistory(documentID string, fromTimestamp int64) []*Operation {
    // Return all operations after timestamp
}
```

---

## 6. Summary Table: What's Needed

| Feature | Status | Backend Work | Plugin Status |
|---------|--------|--------------|---------------|
| WebSocket Connection | ✅ Done | 0 hrs | Works |
| Presence Updates | ✅ Done | 0 hrs | Works |
| Real-time Broadcasts | ✅ Done | 0 hrs | Works |
| Rate Limiting (429 + Retry-After) | ⚠️ Partial | 0.5 hrs (fix header bug) | Works mostly |
| **Operational Transform** | ❌ Missing | **6 hrs** | **BLOCKED** |
| **Error Tracking API** | ❌ Missing | **2 hrs** | **BLOCKED** |
| Operation History Replay | ❌ Missing | 1 hr | For recovery |

---

## 7. Recommendations

### Immediate (This Week)
1. **FIX**: Retry-After header bug in rate limiter (0.5 hours)
   - Change `string(rune(retryAfter))` to `strconv.Itoa(retryAfter)`
   - Test with rate limit test case

2. **BUILD**: Operational Transform endpoint (6 hours)
   - Critical blocker for Task 4.20
   - High complexity but straightforward implementation
   - Test with concurrent edits from multiple clients

3. **BUILD**: Error Tracking API (2 hours)
   - Medium priority for observability
   - Simple CRUD operations
   - Add basic dashboard queries

### Timeline
```
Week 1: Fix rate limit header + start OT work
Week 2: Complete OT implementation + error tracking
Week 3: Testing and integration with plugin
```

### Testing Strategy
1. **Unit Tests**: OT transform logic (identity, idempotent, convergence)
2. **Integration Tests**:
   - Two clients make concurrent edits
   - Server transforms correctly
   - Both clients converge to same state
3. **Stress Tests**:
   - 10+ concurrent clients editing same document
   - Network delays and reconnections
   - Operation ordering

---

## 8. Appendix: Code Locations Reference

**WebSocket**:
- Handler: `backend/internal/websocket/handler.go`
- Hub: `backend/internal/websocket/hub.go`
- Messages: `backend/internal/websocket/message.go`

**Rate Limiting**:
- Middleware: `backend/internal/middleware/ratelimit.go`
- Bug: Line 107 (Retry-After header)

**Routes**:
- Main: `backend/cmd/server/main.go` (~300 lines of route setup)

**Database**:
- Migrations: `backend/init.sql`

---

**Investigation Complete** ✓
**Report Updated**: December 14, 2025
**Next Steps**: Share with backend team for prioritization
