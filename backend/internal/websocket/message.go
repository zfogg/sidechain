package websocket

import (
	"encoding/json"
	"fmt"
	"time"
)

// FlexibleTime handles both Unix millisecond timestamps and RFC3339 strings
type FlexibleTime struct {
	time.Time
}

// UnmarshalJSON implements custom unmarshaling for timestamps
func (ft *FlexibleTime) UnmarshalJSON(b []byte) error {
	// Try to unmarshal as Unix milliseconds (integer)
	var ms int64
	if err := json.Unmarshal(b, &ms); err == nil {
		ft.Time = time.UnixMilli(ms)
		return nil
	}

	// Fall back to RFC3339 string format
	var str string
	if err := json.Unmarshal(b, &str); err != nil {
		return fmt.Errorf("timestamp must be Unix milliseconds (integer) or RFC3339 string")
	}

	t, err := time.Parse(time.RFC3339, str)
	if err != nil {
		return err
	}
	ft.Time = t
	return nil
}

// MarshalJSON implements custom marshaling (always output as RFC3339)
func (ft FlexibleTime) MarshalJSON() ([]byte, error) {
	return json.Marshal(ft.Time)
}

// Message types for WebSocket communication
const (
	// System messages
	MessageTypeSystem = "system"
	MessageTypePing   = "ping"
	MessageTypePong   = "pong"
	MessageTypeError  = "error"
	MessageTypeAuth   = "auth"

	// Feed/Activity messages
	MessageTypeNewPost       = "new_post"
	MessageTypePostLiked     = "post_liked"
	MessageTypePostUnliked   = "post_unliked"
	MessageTypeNewComment    = "new_comment"
	MessageTypeCommentLiked  = "comment_liked"
	MessageTypeCommentUnliked = "comment_unliked"
	MessageTypeNewReaction   = "new_reaction"

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
	MessageTypeCommentCountUpdate  = "comment_count_update"
	MessageTypeFollowerCountUpdate = "follower_count_update"
	MessageTypeFeedInvalidate      = "feed_invalidate"      // Invalidate feed cache
	MessageTypeTimelineUpdate      = "timeline_update"      // Activity timeline changed
	MessageTypeNotificationCountUpdate = "notification_count_update" // Unread notification count

	// Typing indicators
	MessageTypeUserTyping     = "user_typing"      // User started typing a comment
	MessageTypeUserStopTyping = "user_stop_typing" // User stopped typing a comment

	// Collaborative editing (Task 4.20)
	MessageTypeOperation    = "operation"
	MessageTypeOperationAck = "operation_ack"
)

// Message represents a WebSocket message
type Message struct {
	// Type identifies the message type for routing
	Type string `json:"type"`

	// Payload contains the message-specific data
	Payload interface{} `json:"payload,omitempty"`

	// ID is a unique message identifier for acknowledgment
	ID string `json:"id,omitempty"`

	// ReplyTo references the original message ID for responses
	ReplyTo string `json:"reply_to,omitempty"`

	// Timestamp when the message was created (accepts Unix ms or RFC3339)
	Timestamp FlexibleTime `json:"timestamp"`
}

// NewMessage creates a new message with the current timestamp
func NewMessage(msgType string, payload interface{}) *Message {
	return &Message{
		Type:      msgType,
		Payload:   payload,
		Timestamp: FlexibleTime{Time: time.Now().UTC()},
	}
}

// NewMessageWithID creates a new message with a specific ID
func NewMessageWithID(msgType string, id string, payload interface{}) *Message {
	return &Message{
		Type:      msgType,
		ID:        id,
		Payload:   payload,
		Timestamp: FlexibleTime{Time: time.Now().UTC()},
	}
}

// NewReply creates a reply message to an original message
func NewReply(original *Message, msgType string, payload interface{}) *Message {
	return &Message{
		Type:      msgType,
		ReplyTo:   original.ID,
		Payload:   payload,
		Timestamp: FlexibleTime{Time: time.Now().UTC()},
	}
}

// NewErrorMessage creates an error message
func NewErrorMessage(code string, message string) *Message {
	return &Message{
		Type: MessageTypeError,
		Payload: ErrorPayload{
			Code:    code,
			Message: message,
		},
		Timestamp: FlexibleTime{Time: time.Now().UTC()},
	}
}

// ErrorPayload represents an error message payload
type ErrorPayload struct {
	Code    string `json:"code"`
	Message string `json:"message"`
}

// PingPayload represents a ping message payload
type PingPayload struct {
	ClientTime int64 `json:"client_time"`
}

// PongPayload represents a pong message payload
type PongPayload struct {
	ClientTime int64 `json:"client_time"`
	ServerTime int64 `json:"server_time"`
	Latency    int64 `json:"latency_ms"`
}

// AuthPayload represents authentication message payload
type AuthPayload struct {
	Token  string `json:"token,omitempty"`
	UserID string `json:"user_id,omitempty"`
	Status string `json:"status,omitempty"` // "success", "failed", "expired"
	Error  string `json:"error,omitempty"`
}

// PresencePayload represents presence update payload
type PresencePayload struct {
	UserID    string `json:"user_id"`
	Status    string `json:"status"` // "online", "offline", "in_studio"
	DAW       string `json:"daw,omitempty"`
	Timestamp int64  `json:"timestamp"`
}

// NewPostPayload represents a new post notification
type NewPostPayload struct {
	PostID      string   `json:"post_id"`
	UserID      string   `json:"user_id"`
	Username    string   `json:"username"`
	DisplayName string   `json:"display_name,omitempty"`
	AvatarURL   string   `json:"avatar_url,omitempty"`
	AudioURL    string   `json:"audio_url,omitempty"`
	BPM         int      `json:"bpm,omitempty"`
	Key         string   `json:"key,omitempty"`
	Genre       []string `json:"genre,omitempty"`
}

// LikePayload represents a like/unlike event
type LikePayload struct {
	PostID    string `json:"post_id"`
	UserID    string `json:"user_id"`
	Username  string `json:"username"`
	LikeCount int    `json:"like_count"`
	Emoji     string `json:"emoji,omitempty"`
}

// CommentPayload represents a new comment notification
type CommentPayload struct {
	CommentID   string `json:"comment_id"`
	PostID      string `json:"post_id"`
	UserID      string `json:"user_id"`
	Username    string `json:"username"`
	DisplayName string `json:"display_name,omitempty"`
	AvatarURL   string `json:"avatar_url,omitempty"`
	Body        string `json:"body"`
	CreatedAt   int64  `json:"created_at"`
}

// CommentLikePayload represents a comment like/unlike event
type CommentLikePayload struct {
	CommentID string `json:"comment_id"`
	PostID    string `json:"post_id"`
	UserID    string `json:"user_id"`
	Username  string `json:"username"`
	LikeCount int    `json:"like_count"`
}

// FollowPayload represents a follow/unfollow event
type FollowPayload struct {
	FollowerID     string `json:"follower_id"`
	FollowerName   string `json:"follower_name"`
	FollowerAvatar string `json:"follower_avatar,omitempty"`
	FolloweeID     string `json:"followee_id"`
	FolloweeName   string `json:"followee_name"`
	FollowerCount  int    `json:"follower_count,omitempty"`
}

// FollowRequestAcceptedPayload is sent when a follow request is accepted
type FollowRequestAcceptedPayload struct {
	AcceptorID   string `json:"acceptor_id"`   // The private account user who accepted
	AcceptorName string `json:"acceptor_name"` // Username of the acceptor
	RequesterID  string `json:"requester_id"`  // The user whose request was accepted
}

// NotificationPayload represents a notification
type NotificationPayload struct {
	ID        string                 `json:"id"`
	Type      string                 `json:"notification_type"`
	Title     string                 `json:"title"`
	Body      string                 `json:"body"`
	Data      map[string]interface{} `json:"data,omitempty"`
	IsRead    bool                   `json:"is_read"`
	CreatedAt int64                  `json:"created_at"`
}

// SystemPayload represents system event payloads
type SystemPayload struct {
	Event   string                 `json:"event"`
	Message string                 `json:"message,omitempty"`
	Data    map[string]interface{} `json:"data,omitempty"`
}

// ParsePayload unmarshals the payload into a specific type
func (m *Message) ParsePayload(target interface{}) error {
	// If payload is already the right type, return
	if m.Payload == nil {
		return nil
	}

	// Re-marshal and unmarshal to properly type the payload
	data, err := json.Marshal(m.Payload)
	if err != nil {
		return err
	}
	return json.Unmarshal(data, target)
}

// FeedInvalidatePayload signals clients to refresh a specific feed
type FeedInvalidatePayload struct {
	FeedType string `json:"feed_type"` // "timeline", "global", "trending", "notification"
	Reason   string `json:"reason,omitempty"` // "new_post", "follow", "like", etc.
}

// TimelineUpdatePayload indicates activity timeline has new content
type TimelineUpdatePayload struct {
	UserID    string `json:"user_id"`       // User whose timeline changed
	FeedType  string `json:"feed_type"`     // Which feed was updated
	NewCount  int    `json:"new_count"`     // Number of new activities
	Timestamp int64  `json:"timestamp"`
}

// NotificationCountPayload indicates unread notification count changed
type NotificationCountPayload struct {
	UnreadCount int `json:"unread_count"`
	UnseenCount int `json:"unseen_count"`
	Timestamp   int64 `json:"timestamp"`
}

// TypingPayload indicates a user is typing a comment
type TypingPayload struct {
	PostID      string `json:"post_id"`       // Post being commented on
	UserID      string `json:"user_id"`       // User typing
	Username    string `json:"username"`      // Display name for UI
	DisplayName string `json:"display_name,omitempty"`
	AvatarURL   string `json:"avatar_url,omitempty"`
	Timestamp   int64  `json:"timestamp"`
}

// StopTypingPayload indicates a user stopped typing
type StopTypingPayload struct {
	PostID    string `json:"post_id"` // Post being commented on
	UserID    string `json:"user_id"` // User who stopped typing
	Timestamp int64  `json:"timestamp"`
}

// OperationPayload represents a collaborative edit operation
type OperationPayload struct {
	DocumentID string `json:"document_id"` // Format: "channel:channelId:field" e.g., "channel:abc123:description"
	Operation  struct {
		Type      string `json:"type"`       // "Insert" or "Delete"
		ClientID  int    `json:"client_id"`
		Timestamp int64  `json:"timestamp"` // Client timestamp
		Position  int    `json:"position"`
		Content   string `json:"content,omitempty"` // Text being inserted (empty for Delete)
	} `json:"operation"`
}

// OperationAckPayload is sent back to clients after server transformation
type OperationAckPayload struct {
	DocumentID       string                 `json:"document_id"`
	ClientID         int                    `json:"client_id"`
	ServerTimestamp  int64                  `json:"server_timestamp"`
	NewPosition      int                    `json:"new_position"`
	CurrentContent   string                 `json:"current_content"` // Full document state after operation
	TransformedOp    *OperationPayload      `json:"transformed_operation"`
	Success          bool                   `json:"success"`
	Error            string                 `json:"error,omitempty"`
}
