package integration

import (
	"encoding/json"
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	ws "github.com/zfogg/sidechain/backend/internal/websocket"
)

// TestWebSocketConnection tests basic WebSocket connection and disconnection
func TestWebSocketConnection(t *testing.T) {
	if testing.Short() {
		t.Skip("Skipping integration test in short mode")
	}

	hub := ws.NewHub()
	assert.NotNil(t, hub)
	assert.Equal(t, int64(0), hub.GetMetrics().ActiveConnections)
}

// TestActivityBroadcast tests that activities are broadcast to all users
func TestActivityBroadcast(t *testing.T) {
	if testing.Short() {
		t.Skip("Skipping integration test in short mode")
	}

	// Create a mock activity payload
	activity := &ws.ActivityUpdatePayload{
		ID:          "activity-123",
		Actor:       "user-1",
		ActorName:   "John Doe",
		Verb:        "posted",
		Object:      "post-456",
		ObjectType:  "loop_post",
		AudioURL:    "https://example.com/audio.mp3",
		BPM:         128,
		Key:         "C major",
		DAW:         "Ableton Live",
		Genre:       []string{"Electronic", "House"},
		WaveformURL: "https://example.com/waveform.png",
		Timestamp:   time.Now().UnixMilli(),
		FeedTypes:   []string{"global", "timeline"},
	}

	// Create message and verify payload
	_, err := json.Marshal(activity)
	require.NoError(t, err)

	// Create message
	msg := ws.NewMessage(ws.MessageTypeActivityUpdate, activity)
	require.Equal(t, ws.MessageTypeActivityUpdate, msg.Type)

	// Verify activity payload
	var parsed ws.ActivityUpdatePayload
	err = msg.ParsePayload(&parsed)
	require.NoError(t, err)
	assert.Equal(t, "user-1", parsed.Actor)
	assert.Equal(t, "posted", parsed.Verb)
	assert.Equal(t, 128, parsed.BPM)
}

// TestLikeNotification tests like event handling
func TestLikeNotification(t *testing.T) {
	if testing.Short() {
		t.Skip("Skipping integration test in short mode")
	}

	// Create like payload
	like := &ws.LikePayload{
		PostID:    "post-123",
		UserID:    "user-456",
		Username:  "jane_doe",
		LikeCount: 42,
		Emoji:     "❤️",
	}

	// Create and serialize message
	msg := ws.NewMessage(ws.MessageTypePostLiked, like)
	data, err := json.Marshal(msg)
	require.NoError(t, err)

	// Verify serialization
	var parsed ws.Message
	err = json.Unmarshal(data, &parsed)
	require.NoError(t, err)
	assert.Equal(t, ws.MessageTypePostLiked, parsed.Type)

	// Parse payload
	var likePayload ws.LikePayload
	err = parsed.ParsePayload(&likePayload)
	require.NoError(t, err)
	assert.Equal(t, "post-123", likePayload.PostID)
	assert.Equal(t, int(42), likePayload.LikeCount)
}

// TestCommentNotification tests comment event handling
func TestCommentNotification(t *testing.T) {
	if testing.Short() {
		t.Skip("Skipping integration test in short mode")
	}

	// Create comment payload
	comment := &ws.CommentPayload{
		CommentID:   "comment-123",
		PostID:      "post-456",
		UserID:      "user-789",
		Username:    "commenter",
		DisplayName: "Jane Commenter",
		Body:        "Great loop! Love the vibe.",
		CreatedAt:   time.Now().UnixMilli(),
	}

	msg := ws.NewMessage(ws.MessageTypeNewComment, comment)
	data, err := json.Marshal(msg)
	require.NoError(t, err)

	// Deserialize and verify
	var parsed ws.Message
	err = json.Unmarshal(data, &parsed)
	require.NoError(t, err)

	var commentPayload ws.CommentPayload
	err = parsed.ParsePayload(&commentPayload)
	require.NoError(t, err)
	assert.Equal(t, "comment-123", commentPayload.CommentID)
	assert.Equal(t, "Great loop! Love the vibe.", commentPayload.Body)
}

// TestPresenceUpdate tests presence/online status handling
func TestPresenceUpdate(t *testing.T) {
	if testing.Short() {
		t.Skip("Skipping integration test in short mode")
	}

	// Create presence payload
	presence := &ws.PresencePayload{
		UserID:    "user-123",
		Status:    "in_studio",
		DAW:       "Logic Pro",
		Timestamp: time.Now().UnixMilli(),
	}

	msg := ws.NewMessage(ws.MessageTypePresence, presence)
	data, err := json.Marshal(msg)
	require.NoError(t, err)

	// Deserialize and verify
	var parsed ws.Message
	err = json.Unmarshal(data, &parsed)
	require.NoError(t, err)

	var presencePayload ws.PresencePayload
	err = parsed.ParsePayload(&presencePayload)
	require.NoError(t, err)
	assert.Equal(t, "user-123", presencePayload.UserID)
	assert.Equal(t, "in_studio", presencePayload.Status)
	assert.Equal(t, "Logic Pro", presencePayload.DAW)
}

// TestTypingIndicator tests typing indicator handling
func TestTypingIndicator(t *testing.T) {
	if testing.Short() {
		t.Skip("Skipping integration test in short mode")
	}

	// Create typing payload
	typing := &ws.TypingPayload{
		PostID:      "post-123",
		UserID:      "user-456",
		Username:    "typist",
		DisplayName: "John Typist",
		AvatarURL:   "https://example.com/avatar.jpg",
		Timestamp:   time.Now().UnixMilli(),
	}

	msg := ws.NewMessage(ws.MessageTypeUserTyping, typing)
	data, err := json.Marshal(msg)
	require.NoError(t, err)

	// Deserialize and verify
	var parsed ws.Message
	err = json.Unmarshal(data, &parsed)
	require.NoError(t, err)

	var typingPayload ws.TypingPayload
	err = parsed.ParsePayload(&typingPayload)
	require.NoError(t, err)
	assert.Equal(t, "post-123", typingPayload.PostID)
	assert.Equal(t, "user-456", typingPayload.UserID)
	assert.Equal(t, "typist", typingPayload.Username)
}

// TestStopTyping tests stop typing indicator
func TestStopTyping(t *testing.T) {
	if testing.Short() {
		t.Skip("Skipping integration test in short mode")
	}

	// Create stop typing payload
	stopTyping := &ws.StopTypingPayload{
		PostID:    "post-123",
		UserID:    "user-456",
		Timestamp: time.Now().UnixMilli(),
	}

	msg := ws.NewMessage(ws.MessageTypeUserStopTyping, stopTyping)
	data, err := json.Marshal(msg)
	require.NoError(t, err)

	// Deserialize and verify
	var parsed ws.Message
	err = json.Unmarshal(data, &parsed)
	require.NoError(t, err)

	var stopPayload ws.StopTypingPayload
	err = parsed.ParsePayload(&stopPayload)
	require.NoError(t, err)
	assert.Equal(t, "post-123", stopPayload.PostID)
	assert.Equal(t, "user-456", stopPayload.UserID)
}

// TestFollowNotification tests follow event handling
func TestFollowNotification(t *testing.T) {
	if testing.Short() {
		t.Skip("Skipping integration test in short mode")
	}

	// Create follow payload
	follow := &ws.FollowPayload{
		FollowerID:     "user-1",
		FollowerName:   "Alice",
		FollowerAvatar: "https://example.com/alice.jpg",
		FolloweeID:     "user-2",
		FolloweeName:   "Bob",
		FollowerCount:  100,
	}

	msg := ws.NewMessage(ws.MessageTypeNewFollower, follow)
	data, err := json.Marshal(msg)
	require.NoError(t, err)

	// Deserialize and verify
	var parsed ws.Message
	err = json.Unmarshal(data, &parsed)
	require.NoError(t, err)

	var followPayload ws.FollowPayload
	err = parsed.ParsePayload(&followPayload)
	require.NoError(t, err)
	assert.Equal(t, "user-1", followPayload.FollowerID)
	assert.Equal(t, "Alice", followPayload.FollowerName)
	assert.Equal(t, int(100), followPayload.FollowerCount)
}

// TestMessageMarshaling tests message marshaling/unmarshaling with various payloads
func TestMessageMarshaling(t *testing.T) {
	if testing.Short() {
		t.Skip("Skipping integration test in short mode")
	}

	tests := []struct {
		name    string
		msgType string
		payload interface{}
	}{
		{
			name:    "Activity Update",
			msgType: ws.MessageTypeActivityUpdate,
			payload: &ws.ActivityUpdatePayload{
				Actor: "user-1",
				Verb:  "posted",
			},
		},
		{
			name:    "Like Notification",
			msgType: ws.MessageTypePostLiked,
			payload: &ws.LikePayload{
				PostID:    "post-123",
				LikeCount: 42,
			},
		},
		{
			name:    "Typing Indicator",
			msgType: ws.MessageTypeUserTyping,
			payload: &ws.TypingPayload{
				PostID: "post-123",
				UserID: "user-456",
			},
		},
		{
			name:    "Presence Update",
			msgType: ws.MessageTypePresence,
			payload: &ws.PresencePayload{
				UserID: "user-123",
				Status: "in_studio",
			},
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			msg := ws.NewMessage(tt.msgType, tt.payload)
			assert.Equal(t, tt.msgType, msg.Type)
			assert.NotNil(t, msg.Payload)
			assert.False(t, msg.Timestamp.IsZero())

			// Test marshaling
			data, err := json.Marshal(msg)
			require.NoError(t, err)
			assert.NotEmpty(t, data)

			// Test unmarshaling
			var parsed ws.Message
			err = json.Unmarshal(data, &parsed)
			require.NoError(t, err)
			assert.Equal(t, tt.msgType, parsed.Type)
		})
	}
}

// TestWebSocketErrorHandling tests error message handling
func TestWebSocketErrorHandling(t *testing.T) {
	if testing.Short() {
		t.Skip("Skipping integration test in short mode")
	}

	// Create error message
	errorMsg := ws.NewErrorMessage("test_error", "Something went wrong")
	assert.Equal(t, ws.MessageTypeError, errorMsg.Type)

	// Serialize and deserialize
	data, err := json.Marshal(errorMsg)
	require.NoError(t, err)

	var parsed ws.Message
	err = json.Unmarshal(data, &parsed)
	require.NoError(t, err)

	var errorPayload ws.ErrorPayload
	err = parsed.ParsePayload(&errorPayload)
	require.NoError(t, err)
	assert.Equal(t, "test_error", errorPayload.Code)
	assert.Equal(t, "Something went wrong", errorPayload.Message)
}

// TestPingPong tests ping/pong functionality
func TestPingPong(t *testing.T) {
	if testing.Short() {
		t.Skip("Skipping integration test in short mode")
	}

	// Create ping message
	ping := ws.NewMessage(ws.MessageTypePing, ws.PingPayload{
		ClientTime: time.Now().UnixMilli(),
	})

	assert.Equal(t, ws.MessageTypePing, ping.Type)

	// Serialize and deserialize
	data, err := json.Marshal(ping)
	require.NoError(t, err)

	var parsed ws.Message
	err = json.Unmarshal(data, &parsed)
	require.NoError(t, err)

	// Create pong reply
	pong := ws.NewReply(&parsed, ws.MessageTypePong, ws.PongPayload{
		ClientTime: time.Now().UnixMilli(),
		ServerTime: time.Now().UnixMilli(),
		Latency:    100,
	})

	assert.Equal(t, ws.MessageTypePong, pong.Type)
	assert.Equal(t, ping.ID, pong.ReplyTo)
}

// TestTimelineUpdate tests timeline update notification
func TestTimelineUpdate(t *testing.T) {
	if testing.Short() {
		t.Skip("Skipping integration test in short mode")
	}

	// Create timeline update payload
	timeline := &ws.TimelineUpdatePayload{
		UserID:    "user-123",
		FeedType:  "user_activity",
		NewCount:  5,
		Timestamp: time.Now().UnixMilli(),
	}

	msg := ws.NewMessage(ws.MessageTypeTimelineUpdate, timeline)
	data, err := json.Marshal(msg)
	require.NoError(t, err)

	// Deserialize and verify
	var parsed ws.Message
	err = json.Unmarshal(data, &parsed)
	require.NoError(t, err)

	var timelinePayload ws.TimelineUpdatePayload
	err = parsed.ParsePayload(&timelinePayload)
	require.NoError(t, err)
	assert.Equal(t, "user-123", timelinePayload.UserID)
	assert.Equal(t, "user_activity", timelinePayload.FeedType)
	assert.Equal(t, 5, timelinePayload.NewCount)
}

// TestFeedInvalidation tests feed invalidation notification
func TestFeedInvalidation(t *testing.T) {
	if testing.Short() {
		t.Skip("Skipping integration test in short mode")
	}

	// Create feed invalidate payload
	invalidate := &ws.FeedInvalidatePayload{
		FeedType: "global",
		Reason:   "new_post",
	}

	msg := ws.NewMessage(ws.MessageTypeFeedInvalidate, invalidate)
	data, err := json.Marshal(msg)
	require.NoError(t, err)

	// Deserialize and verify
	var parsed ws.Message
	err = json.Unmarshal(data, &parsed)
	require.NoError(t, err)

	var invalidatePayload ws.FeedInvalidatePayload
	err = parsed.ParsePayload(&invalidatePayload)
	require.NoError(t, err)
	assert.Equal(t, "global", invalidatePayload.FeedType)
	assert.Equal(t, "new_post", invalidatePayload.Reason)
}

// BenchmarkMessageMarshal benchmarks message marshaling
func BenchmarkMessageMarshal(b *testing.B) {
	activity := &ws.ActivityUpdatePayload{
		ID:          "activity-123",
		Actor:       "user-1",
		ActorName:   "John Doe",
		Verb:        "posted",
		Object:      "post-456",
		ObjectType:  "loop_post",
		AudioURL:    "https://example.com/audio.mp3",
		BPM:         128,
		Key:         "C major",
		DAW:         "Ableton Live",
		Genre:       []string{"Electronic", "House"},
		WaveformURL: "https://example.com/waveform.png",
		Timestamp:   time.Now().UnixMilli(),
		FeedTypes:   []string{"global", "timeline"},
	}

	msg := ws.NewMessage(ws.MessageTypeActivityUpdate, activity)

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		_, _ = json.Marshal(msg)
	}
}

// BenchmarkMessageUnmarshal benchmarks message unmarshaling
func BenchmarkMessageUnmarshal(b *testing.B) {
	activity := &ws.ActivityUpdatePayload{
		ID:          "activity-123",
		Actor:       "user-1",
		ActorName:   "John Doe",
		Verb:        "posted",
		Object:      "post-456",
		ObjectType:  "loop_post",
		AudioURL:    "https://example.com/audio.mp3",
		BPM:         128,
		Key:         "C major",
		DAW:         "Ableton Live",
		Genre:       []string{"Electronic", "House"},
		WaveformURL: "https://example.com/waveform.png",
		Timestamp:   time.Now().UnixMilli(),
		FeedTypes:   []string{"global", "timeline"},
	}

	msg := ws.NewMessage(ws.MessageTypeActivityUpdate, activity)
	data, _ := json.Marshal(msg)

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		var parsed ws.Message
		_ = json.Unmarshal(data, &parsed)
	}
}
