package websocket

import (
	"context"
	"errors"
	"fmt"
	"log"
	"net/http"
	"strings"
	"time"

	"github.com/coder/websocket"
	"github.com/gin-gonic/gin"
	"github.com/golang-jwt/jwt/v5"
	"github.com/zfogg/sidechain/backend/internal/database"
	"github.com/zfogg/sidechain/backend/internal/models"
)

// Handler handles WebSocket HTTP upgrade requests
type Handler struct {
	hub             *Hub
	jwtSecret       []byte
	presenceManager *PresenceManager
}

// NewHandler creates a new WebSocket handler
func NewHandler(hub *Hub, jwtSecret []byte) *Handler {
	return &Handler{
		hub:       hub,
		jwtSecret: jwtSecret,
	}
}

// SetPresenceManager sets the presence manager for the handler
func (h *Handler) SetPresenceManager(pm *PresenceManager) {
	h.presenceManager = pm
}

// GetPresenceManager returns the presence manager
func (h *Handler) GetPresenceManager() *PresenceManager {
	return h.presenceManager
}

// HandleWebSocket handles WebSocket upgrade requests
// Authentication is done via JWT token in query param: ?token=...
// Or via Authorization header: Bearer <token>
func (h *Handler) HandleWebSocket(c *gin.Context) {
	// Extract and validate JWT token
	user, err := h.authenticateRequest(c)
	if err != nil {
		log.Printf("WebSocket auth failed: %v", err)
		c.JSON(http.StatusUnauthorized, gin.H{
			"error":   "authentication_failed",
			"message": err.Error(),
		})
		return
	}

	// Upgrade the HTTP connection to WebSocket
	conn, err := websocket.Accept(c.Writer, c.Request, &websocket.AcceptOptions{
		// In production, set specific origins
		InsecureSkipVerify: true, // TODO: Configure CORS properly for production
		CompressionMode:    websocket.CompressionContextTakeover,
	})
	if err != nil {
		log.Printf("WebSocket upgrade failed: %v", err)
		return
	}

	// Create client
	client := NewClient(h.hub, conn, user.ID, user.Username)
	client.RemoteAddr = c.ClientIP()
	client.UserAgent = c.GetHeader("User-Agent")

	// Register client with hub
	h.hub.Register(client)

	// Notify presence manager of connection
	if h.presenceManager != nil {
		h.presenceManager.OnClientConnect(client)
	}

	// Send welcome message
	client.Send(NewMessage(MessageTypeSystem, SystemPayload{
		Event:   "connected",
		Message: "Welcome to Sidechain!",
		Data: map[string]interface{}{
			"user_id":     user.ID,
			"username":    user.Username,
			"server_time": time.Now().UTC().UnixMilli(),
			"session_id":  fmt.Sprintf("%p", client),
		},
	}))

	// Start client read/write pumps
	go client.WritePump()
	client.ReadPump() // This blocks until client disconnects

	// Client disconnected - notify presence manager
	if h.presenceManager != nil {
		h.presenceManager.OnClientDisconnect(client)
	}
}

// authenticateRequest extracts and validates the JWT token from the request
func (h *Handler) authenticateRequest(c *gin.Context) (*models.User, error) {
	tokenString := ""

	// First check query parameter
	if token := c.Query("token"); token != "" {
		tokenString = token
	}

	// Then check Authorization header
	if auth := c.GetHeader("Authorization"); auth != "" {
		// Support "Bearer <token>" format
		if strings.HasPrefix(auth, "Bearer ") {
			tokenString = strings.TrimPrefix(auth, "Bearer ")
		} else {
			tokenString = auth
		}
	}

	if tokenString == "" {
		return nil, errors.New("no authentication token provided")
	}

	// Parse and validate JWT
	token, err := jwt.Parse(tokenString, func(token *jwt.Token) (interface{}, error) {
		// Validate signing method
		if _, ok := token.Method.(*jwt.SigningMethodHMAC); !ok {
			return nil, fmt.Errorf("unexpected signing method: %v", token.Header["alg"])
		}
		return h.jwtSecret, nil
	})

	if err != nil {
		return nil, fmt.Errorf("invalid token: %w", err)
	}

	claims, ok := token.Claims.(jwt.MapClaims)
	if !ok || !token.Valid {
		return nil, errors.New("invalid token claims")
	}

	// Check token expiration
	exp, ok := claims["exp"].(float64)
	if !ok {
		return nil, errors.New("token missing expiration")
	}
	if time.Unix(int64(exp), 0).Before(time.Now()) {
		return nil, errors.New("token expired")
	}

	// Extract user ID
	userID, ok := claims["user_id"].(string)
	if !ok || userID == "" {
		return nil, errors.New("invalid user_id in token")
	}

	// Fetch user from database
	var user models.User
	if err := database.DB.First(&user, "id = ?", userID).Error; err != nil {
		return nil, fmt.Errorf("user not found: %w", err)
	}

	return &user, nil
}

// HandleMetrics returns WebSocket metrics (for monitoring)
func (h *Handler) HandleMetrics(c *gin.Context) {
	metrics := h.hub.GetMetrics()
	c.JSON(http.StatusOK, gin.H{
		"websocket":    metrics,
		"online_users": h.hub.GetOnlineUsers(),
		"timestamp":    time.Now().UTC(),
	})
}

// HandleOnlineStatus checks if specific users are online
func (h *Handler) HandleOnlineStatus(c *gin.Context) {
	var req struct {
		UserIDs []string `json:"user_ids" binding:"required"`
	}

	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	statuses := make(map[string]bool)
	for _, userID := range req.UserIDs {
		statuses[userID] = h.hub.IsUserOnline(userID)
	}

	c.JSON(http.StatusOK, gin.H{
		"statuses":  statuses,
		"timestamp": time.Now().UTC(),
	})
}

// HandlePresenceStatus returns detailed presence information for users
func (h *Handler) HandlePresenceStatus(c *gin.Context) {
	var req struct {
		UserIDs []string `json:"user_ids" binding:"required"`
	}

	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	if h.presenceManager == nil {
		// Fallback to basic online status
		statuses := make(map[string]interface{})
		for _, userID := range req.UserIDs {
			if h.hub.IsUserOnline(userID) {
				statuses[userID] = map[string]interface{}{
					"status": "online",
				}
			}
		}
		c.JSON(http.StatusOK, gin.H{
			"presence":  statuses,
			"timestamp": time.Now().UTC(),
		})
		return
	}

	// Get detailed presence from manager
	presence := h.presenceManager.GetOnlinePresence(req.UserIDs)

	// Convert to JSON-friendly format
	result := make(map[string]interface{})
	for userID, p := range presence {
		result[userID] = map[string]interface{}{
			"status":        p.Status,
			"daw":           p.DAW,
			"last_activity": p.LastActivity.UnixMilli(),
		}
	}

	c.JSON(http.StatusOK, gin.H{
		"presence":     result,
		"online_count": len(presence),
		"timestamp":    time.Now().UTC(),
	})
}

// HandleFriendsInStudio returns count of followed users currently in studio
func (h *Handler) HandleFriendsInStudio(c *gin.Context) {
	// Get current user from context
	userID, exists := c.Get("user_id")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "not_authenticated"})
		return
	}

	if h.presenceManager == nil || h.presenceManager.streamClient == nil {
		c.JSON(http.StatusOK, gin.H{
			"count":     0,
			"friends":   []interface{}{},
			"timestamp": time.Now().UTC(),
		})
		return
	}

	// Get following list
	following, err := h.presenceManager.streamClient.GetFollowing(userID.(string), 500, 0)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed_to_get_following"})
		return
	}

	// Extract user IDs
	followingIDs := make([]string, len(following))
	for i, f := range following {
		followingIDs[i] = f.UserID
	}

	// Get their presence
	onlinePresence := h.presenceManager.GetOnlinePresence(followingIDs)

	// Filter to in_studio only
	inStudio := make([]map[string]interface{}, 0)
	for _, p := range onlinePresence {
		if p.Status == StatusInStudio {
			inStudio = append(inStudio, map[string]interface{}{
				"user_id":  p.UserID,
				"username": p.Username,
				"daw":      p.DAW,
			})
		}
	}

	c.JSON(http.StatusOK, gin.H{
		"count":     len(inStudio),
		"friends":   inStudio,
		"timestamp": time.Now().UTC(),
	})
}

// RegisterDefaultHandlers registers the default message handlers
func (h *Handler) RegisterDefaultHandlers() {
	// Presence update handler
	h.hub.RegisterHandler(MessageTypePresence, func(client *Client, msg *Message) error {
		var presence PresencePayload
		if err := msg.ParsePayload(&presence); err != nil {
			return err
		}

		// Update presence for user
		presence.UserID = client.UserID
		presence.Timestamp = time.Now().UnixMilli()

		// Broadcast presence to followers (simplified - in production, get followers from Stream.io)
		h.hub.Broadcast(NewMessage(MessageTypePresence, presence))
		return nil
	})

	// Playback started handler (for social listening feature)
	h.hub.RegisterHandler(MessageTypePlaybackStarted, func(client *Client, msg *Message) error {
		// Broadcast to followers that user started listening
		h.hub.Broadcast(NewMessage(MessageTypePlaybackStarted, map[string]interface{}{
			"user_id":   client.UserID,
			"username":  client.Username,
			"post_id":   msg.Payload,
			"timestamp": time.Now().UnixMilli(),
		}))
		return nil
	})

	// Playback stopped handler
	h.hub.RegisterHandler(MessageTypePlaybackStopped, func(client *Client, msg *Message) error {
		h.hub.Broadcast(NewMessage(MessageTypePlaybackStopped, map[string]interface{}{
			"user_id":   client.UserID,
			"timestamp": time.Now().UnixMilli(),
		}))
		return nil
	})

	log.Println("📨 Registered default WebSocket message handlers")
}

// NotifyNewPost sends a new post notification to followers
func (h *Handler) NotifyNewPost(followerIDs []string, payload *NewPostPayload) {
	msg := NewMessage(MessageTypeNewPost, payload)
	for _, followerID := range followerIDs {
		h.hub.SendToUser(followerID, msg)
	}
}

// NotifyLike sends a like notification to the post owner
func (h *Handler) NotifyLike(ownerID string, payload *LikePayload) {
	h.hub.SendToUser(ownerID, NewMessage(MessageTypePostLiked, payload))
}

// NotifyFollow sends a follow notification
func (h *Handler) NotifyFollow(followeeID string, payload *FollowPayload) {
	h.hub.SendToUser(followeeID, NewMessage(MessageTypeNewFollower, payload))
}

// NotifyNotification sends a generic notification
func (h *Handler) NotifyNotification(userID string, payload *NotificationPayload) {
	h.hub.SendToUser(userID, NewMessage(MessageTypeNotification, payload))
}

// UpdateNotificationCount sends updated notification counts
func (h *Handler) UpdateNotificationCount(userID string, unread, unseen int) {
	h.hub.SendToUser(userID, NewMessage(MessageTypeNotificationCount, NotificationCountPayload{
		UnreadCount: unread,
		UnseenCount: unseen,
	}))
}

// BroadcastLikeCountUpdate broadcasts like count update to all viewers
func (h *Handler) BroadcastLikeCountUpdate(postID string, likeCount int) {
	h.hub.Broadcast(NewMessage(MessageTypeLikeCountUpdate, map[string]interface{}{
		"post_id":    postID,
		"like_count": likeCount,
		"timestamp":  time.Now().UnixMilli(),
	}))
}

// Shutdown gracefully shuts down the WebSocket handler
func (h *Handler) Shutdown(ctx context.Context) error {
	return h.hub.Shutdown(ctx)
}

// GetHub returns the hub for external access
func (h *Handler) GetHub() *Hub {
	return h.hub
}
