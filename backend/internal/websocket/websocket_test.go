package websocket

import (
	"encoding/json"
	"os"
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	"github.com/zfogg/sidechain/backend/internal/logger"
)

func TestMain(m *testing.M) {
	// Initialize logger for tests
	_ = logger.Initialize("error", "")
	os.Exit(m.Run())
}

func TestNewHub(t *testing.T) {
	hub := NewHub()
	assert.NotNil(t, hub)
	assert.NotNil(t, hub.clients)
	assert.NotNil(t, hub.allClients)
	assert.NotNil(t, hub.register)
	assert.NotNil(t, hub.unregister)
	assert.NotNil(t, hub.broadcast)
	assert.NotNil(t, hub.unicast)
	assert.NotNil(t, hub.metrics)
	assert.NotNil(t, hub.handlers)
}

func TestRateLimiter(t *testing.T) {
	// Create a rate limiter allowing 5 per second with burst of 10
	rl := NewRateLimiter(5, 10)

	// Should allow first 10 requests (burst)
	for i := 0; i < 10; i++ {
		assert.True(t, rl.Allow(), "Request %d should be allowed", i+1)
	}

	// Next request should be denied (no tokens left)
	assert.False(t, rl.Allow(), "Request 11 should be denied")

	// After waiting, should be allowed again
	time.Sleep(300 * time.Millisecond)
	assert.True(t, rl.Allow(), "Request after wait should be allowed")
}

func TestNewMessage(t *testing.T) {
	payload := map[string]string{"test": "data"}
	msg := NewMessage(MessageTypeNewPost, payload)

	assert.Equal(t, MessageTypeNewPost, msg.Type)
	assert.NotNil(t, msg.Payload)
	assert.False(t, msg.Timestamp.IsZero())
}

func TestNewMessageWithID(t *testing.T) {
	msg := NewMessageWithID(MessageTypePing, "msg-123", nil)

	assert.Equal(t, MessageTypePing, msg.Type)
	assert.Equal(t, "msg-123", msg.ID)
}

func TestNewReply(t *testing.T) {
	original := NewMessageWithID(MessageTypePing, "original-id", nil)
	reply := NewReply(original, MessageTypePong, nil)

	assert.Equal(t, MessageTypePong, reply.Type)
	assert.Equal(t, "original-id", reply.ReplyTo)
}

func TestNewErrorMessage(t *testing.T) {
	msg := NewErrorMessage("test_error", "Something went wrong")

	assert.Equal(t, MessageTypeError, msg.Type)

	payload, ok := msg.Payload.(ErrorPayload)
	assert.True(t, ok)
	assert.Equal(t, "test_error", payload.Code)
	assert.Equal(t, "Something went wrong", payload.Message)
}

func TestMessageParsePayload(t *testing.T) {
	// Create message with map payload
	msg := NewMessage(MessageTypePing, map[string]interface{}{
		"client_time": float64(1234567890),
	})

	var ping PingPayload
	err := msg.ParsePayload(&ping)
	assert.NoError(t, err)
	assert.Equal(t, int64(1234567890), ping.ClientTime)
}

func TestMessageJSONSerialization(t *testing.T) {
	msg := NewMessage(MessageTypeNewPost, NewPostPayload{
		PostID:   "post-123",
		UserID:   "user-456",
		Username: "testuser",
		BPM:      128,
		Key:      "C minor",
	})
	msg.ID = "msg-id"

	// Serialize to JSON
	data, err := json.Marshal(msg)
	assert.NoError(t, err)

	// Deserialize back
	var parsed Message
	err = json.Unmarshal(data, &parsed)
	assert.NoError(t, err)

	assert.Equal(t, MessageTypeNewPost, parsed.Type)
	assert.Equal(t, "msg-id", parsed.ID)
	assert.NotNil(t, parsed.Payload)
}

func TestHubMetrics(t *testing.T) {
	hub := NewHub()

	metrics := hub.GetMetrics()
	assert.Equal(t, int64(0), metrics.TotalConnections)
	assert.Equal(t, int64(0), metrics.ActiveConnections)
	assert.Equal(t, int64(0), metrics.MessagesReceived)
	assert.Equal(t, int64(0), metrics.MessagesSent)

	// Test metrics string representation
	str := metrics.String()
	assert.Contains(t, str, "connections=0/0")
}

func TestDefaultRateLimitConfig(t *testing.T) {
	config := DefaultRateLimitConfig()

	assert.Equal(t, 10, config.MaxMessagesPerSecond)
	assert.Equal(t, 20, config.BurstSize)
	assert.Equal(t, time.Second, config.Window)
}

func TestHubRegisterHandler(t *testing.T) {
	hub := NewHub()

	// Register a handler
	hub.RegisterHandler("test_type", func(client *Client, msg *Message) error {
		return nil
	})

	// Check handler exists
	handler, ok := hub.GetHandler("test_type")
	assert.True(t, ok)
	assert.NotNil(t, handler)

	// Check non-existent handler
	_, ok = hub.GetHandler("nonexistent")
	assert.False(t, ok)
}

func TestHubIsUserOnline(t *testing.T) {
	hub := NewHub()

	// User should not be online initially
	assert.False(t, hub.IsUserOnline("user-123"))

	// User connection count should be 0
	assert.Equal(t, 0, hub.GetUserConnectionCount("user-123"))
}

func TestHubGetOnlineUsers(t *testing.T) {
	hub := NewHub()

	// No users online initially
	users := hub.GetOnlineUsers()
	assert.Empty(t, users)
}

func TestPresencePayload(t *testing.T) {
	payload := PresencePayload{
		UserID:    "user-123",
		Status:    "in_studio",
		DAW:       "Ableton Live",
		Timestamp: time.Now().UnixMilli(),
	}

	data, err := json.Marshal(payload)
	assert.NoError(t, err)

	var parsed PresencePayload
	err = json.Unmarshal(data, &parsed)
	assert.NoError(t, err)

	assert.Equal(t, "user-123", parsed.UserID)
	assert.Equal(t, "in_studio", parsed.Status)
	assert.Equal(t, "Ableton Live", parsed.DAW)
}

func TestNotificationPayload(t *testing.T) {
	payload := NotificationPayload{
		ID:        "notif-123",
		Type:      "like",
		Title:     "New Like",
		Body:      "Someone liked your post",
		IsRead:    false,
		CreatedAt: time.Now().UnixMilli(),
	}

	data, err := json.Marshal(payload)
	assert.NoError(t, err)

	var parsed NotificationPayload
	err = json.Unmarshal(data, &parsed)
	assert.NoError(t, err)

	assert.Equal(t, "notif-123", parsed.ID)
	assert.Equal(t, "like", parsed.Type)
	assert.False(t, parsed.IsRead)
}

func TestMessageTypes(t *testing.T) {
	// Ensure all message types are defined and unique
	types := []string{
		MessageTypeSystem,
		MessageTypePing,
		MessageTypePong,
		MessageTypeError,
		MessageTypeAuth,
		MessageTypeNewPost,
		MessageTypePostLiked,
		MessageTypePostUnliked,
		MessageTypeNewComment,
		MessageTypeNewReaction,
		MessageTypeNewFollower,
		MessageTypeUnfollowed,
		MessageTypePresence,
		MessageTypeUserOnline,
		MessageTypeUserOffline,
		MessageTypeUserInStudio,
		MessageTypeNotification,
		MessageTypeNotificationRead,
		MessageTypeNotificationCount,
		MessageTypePlaybackStarted,
		MessageTypePlaybackStopped,
		MessageTypeLikeCountUpdate,
		MessageTypeFollowerCountUpdate,
	}

	// Check all are non-empty
	for _, typ := range types {
		assert.NotEmpty(t, typ)
	}

	// Check all are unique
	seen := make(map[string]bool)
	for _, typ := range types {
		assert.False(t, seen[typ], "Duplicate message type: %s", typ)
		seen[typ] = true
	}
}
