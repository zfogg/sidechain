package service

import (
	"context"
	"fmt"
	"os"
	"os/signal"
	"syscall"
	"time"

	"github.com/zfogg/sidechain/cli/pkg/api"
	"github.com/zfogg/sidechain/cli/pkg/formatter"
	"github.com/zfogg/sidechain/cli/pkg/logger"
	"github.com/zfogg/sidechain/cli/pkg/websocket"
)

// NotificationWatcherService watches for real-time notifications
type NotificationWatcherService struct {
	wsClient *websocket.Client
}

// NewNotificationWatcherService creates a new notification watcher service
func NewNotificationWatcherService() *NotificationWatcherService {
	return &NotificationWatcherService{
		wsClient: websocket.GetClient(),
	}
}

// Notification payload types
type NotificationPayload struct {
	ID          string      `json:"id"`
	Type        string      `json:"type"`
	UserID      string      `json:"user_id"`
	User        *api.User   `json:"user,omitempty"`
	PostID      string      `json:"post_id,omitempty"`
	CommentID   string      `json:"comment_id,omitempty"`
	Message     string      `json:"message,omitempty"`
	CreatedAt   time.Time   `json:"created_at"`
	IsRead      bool        `json:"is_read"`
}

// WatchNotifications watches for real-time notifications
func (nw *NotificationWatcherService) WatchNotifications(ctx context.Context) error {
	logger.Debug("Starting notification watcher")

	// Get current user to authenticate
	currentUser, err := api.GetCurrentUser()
	if err != nil {
		return fmt.Errorf("failed to get current user: %w", err)
	}

	// Get JWT token for WebSocket authentication
	token, err := nw.getAuthToken()
	if err != nil {
		return fmt.Errorf("failed to get auth token: %w", err)
	}

	// Connect to WebSocket
	if err := nw.wsClient.Connect(token); err != nil {
		return fmt.Errorf("failed to connect to notification stream: %w", err)
	}
	defer nw.wsClient.Disconnect()

	// Display header
	fmt.Printf("\n")
	formatter.PrintInfo("🔔 Watching for real-time notifications")
	fmt.Printf("Connected as: @%s\n", currentUser.Username)
	fmt.Printf("Press Ctrl+C to stop\n")
	fmt.Printf("%s\n\n", repeatChar("─", 60))

	// Setup signal handler for graceful shutdown
	sigChan := make(chan os.Signal, 1)
	signal.Notify(sigChan, syscall.SIGINT, syscall.SIGTERM)

	// Subscribe to notification events
	unsubNotif := nw.wsClient.On(websocket.MessageTypeNotification, nw.handleNotificationEvent)
	unsubNewPost := nw.wsClient.On(websocket.MessageTypeNewPost, nw.handleNewPostEvent)
	unsubLike := nw.wsClient.On(websocket.MessageTypePostLiked, nw.handlePostLikedEvent)
	unsubComment := nw.wsClient.On(websocket.MessageTypePostCommented, nw.handleCommentEvent)
	unsubFollow := nw.wsClient.On(websocket.MessageTypeUserFollowed, nw.handleFollowEvent)
	unsubPresence := nw.wsClient.On(websocket.MessageTypePresenceUpdate, nw.handlePresenceEvent)

	defer func() {
		unsubNotif()
		unsubNewPost()
		unsubLike()
		unsubComment()
		unsubFollow()
		unsubPresence()
	}()

	// Wait for signal or context cancellation
	select {
	case <-sigChan:
		fmt.Printf("\n")
		formatter.PrintSuccess("Notification watcher stopped")
		return nil
	case <-ctx.Done():
		return ctx.Err()
	}
}

// Event handlers

func (nw *NotificationWatcherService) handleNotificationEvent(payload interface{}) {
	notif, ok := payload.(map[string]interface{})
	if !ok {
		logger.Error("Invalid notification payload type")
		return
	}

	typeStr := getString(notif, "type", "notification")
	message := getString(notif, "message", "New notification")

	nw.displayEvent("📬", typeStr, message)
}

func (nw *NotificationWatcherService) handleNewPostEvent(payload interface{}) {
	data, ok := payload.(map[string]interface{})
	if !ok {
		return
	}

	post := getNestedMap(data, "post")
	user := getNestedMap(post, "user")
	username := getString(user, "username", "Unknown")

	message := fmt.Sprintf("New post from @%s", username)
	nw.displayEvent("🎵", "new_post", message)
}

func (nw *NotificationWatcherService) handlePostLikedEvent(payload interface{}) {
	data, ok := payload.(map[string]interface{})
	if !ok {
		return
	}

	user := getNestedMap(data, "user")
	username := getString(user, "username", "Unknown")

	message := fmt.Sprintf("@%s liked your post", username)
	nw.displayEvent("❤️", "post_liked", message)
}

func (nw *NotificationWatcherService) handleCommentEvent(payload interface{}) {
	data, ok := payload.(map[string]interface{})
	if !ok {
		return
	}

	user := getNestedMap(data, "user")
	username := getString(user, "username", "Unknown")
	comment := getString(data, "content", "")

	message := fmt.Sprintf("@%s commented: %s", username, truncateString(comment, 40))
	nw.displayEvent("💬", "post_commented", message)
}

func (nw *NotificationWatcherService) handleFollowEvent(payload interface{}) {
	data, ok := payload.(map[string]interface{})
	if !ok {
		return
	}

	user := getNestedMap(data, "user")
	username := getString(user, "username", "Unknown")

	message := fmt.Sprintf("@%s followed you", username)
	nw.displayEvent("👥", "user_followed", message)
}

func (nw *NotificationWatcherService) handlePresenceEvent(payload interface{}) {
	data, ok := payload.(map[string]interface{})
	if !ok {
		return
	}

	user := getNestedMap(data, "user")
	username := getString(user, "username", "Unknown")
	status := getString(data, "status", "online")

	emoji := "🟢"
	if status == "offline" {
		emoji = "⚫"
	} else if status == "away" {
		emoji = "🟡"
	}

	message := fmt.Sprintf("@%s is now %s", username, status)
	nw.displayEvent(emoji, "presence_update", message)
}

// Display helpers

func (nw *NotificationWatcherService) displayEvent(emoji, eventType, message string) {
	timestamp := time.Now().Format("15:04:05")
	fmt.Printf("[%s] %s %s\n", timestamp, emoji, message)
}

// Utility functions

func (nw *NotificationWatcherService) getAuthToken() (string, error) {
	// Get JWT token from auth service or config
	// For now, we'll try to get it from the current user's session
	// In production, this should come from a secure token store
	return "", nil // Will be implemented when auth token storage is available
}

func getString(m map[string]interface{}, key string, defaultVal string) string {
	if v, ok := m[key]; ok {
		if s, ok := v.(string); ok {
			return s
		}
	}
	return defaultVal
}

func getNestedMap(m map[string]interface{}, key string) map[string]interface{} {
	if v, ok := m[key]; ok {
		if nested, ok := v.(map[string]interface{}); ok {
			return nested
		}
	}
	return make(map[string]interface{})
}

func truncateString(s string, maxLen int) string {
	if len(s) > maxLen {
		return s[:maxLen-3] + "..."
	}
	return s
}

func repeatChar(char string, count int) string {
	result := ""
	for i := 0; i < count; i++ {
		result += char
	}
	return result
}
