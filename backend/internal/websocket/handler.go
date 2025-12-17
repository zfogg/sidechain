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
	"github.com/zfogg/sidechain/backend/internal/crdt"
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

// HandleWebSocketHTTP is a raw http.Handler for WebSocket upgrades
// This bypasses Gin's ResponseWriter wrapper which can interfere with connection hijacking
func (h *Handler) HandleWebSocketHTTP(w http.ResponseWriter, r *http.Request) {
	// Extract and validate JWT token from query param or header
	user, err := h.authenticateHTTPRequest(r)
	if err != nil {
		log.Printf("WebSocket auth failed: %v", err)
		http.Error(w, `{"error":"authentication_failed","message":"`+err.Error()+`"}`, http.StatusUnauthorized)
		return
	}

	// Upgrade the HTTP connection to WebSocket using raw http.ResponseWriter
	conn, err := websocket.Accept(w, r, &websocket.AcceptOptions{
		InsecureSkipVerify: true,
		CompressionMode:    websocket.CompressionDisabled,
	})
	if err != nil {
		log.Printf("WebSocket upgrade failed: %v", err)
		return
	}

	// Get client IP from X-Forwarded-For or RemoteAddr
	clientIP := r.Header.Get("X-Forwarded-For")
	if clientIP == "" {
		clientIP = r.Header.Get("X-Real-IP")
	}
	if clientIP == "" {
		clientIP = r.RemoteAddr
	}

	// Create client
	client := NewClient(h.hub, conn, user.ID, user.Username)
	client.RemoteAddr = clientIP
	client.UserAgent = r.Header.Get("User-Agent")

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

	// Cleanup after disconnect
	if h.presenceManager != nil {
		h.presenceManager.OnClientDisconnect(client)
	}
}

// authenticateHTTPRequest extracts and validates JWT from raw HTTP request
func (h *Handler) authenticateHTTPRequest(r *http.Request) (*models.User, error) {
	// Try query param first
	tokenString := r.URL.Query().Get("token")

	// Fall back to Authorization header
	if tokenString == "" {
		authHeader := r.Header.Get("Authorization")
		if strings.HasPrefix(authHeader, "Bearer ") {
			tokenString = strings.TrimPrefix(authHeader, "Bearer ")
		}
	}

	if tokenString == "" {
		log.Printf("[WS] No token provided in request")
		return nil, errors.New("no token provided")
	}

	// Log token length for debugging (don't log full token for security)
	log.Printf("[WS] Authenticating with token (len=%d, prefix=%.10s...)", len(tokenString), tokenString)

	// Parse and validate the token
	token, err := jwt.Parse(tokenString, func(token *jwt.Token) (interface{}, error) {
		if _, ok := token.Method.(*jwt.SigningMethodHMAC); !ok {
			return nil, fmt.Errorf("unexpected signing method: %v", token.Header["alg"])
		}
		return h.jwtSecret, nil
	})

	if err != nil {
		log.Printf("[WS] JWT parse error: %v", err)
		log.Printf("[WS] Token string (first 100 chars): %.100s", tokenString)
		log.Printf("[WS] JWT secret length at validation: %d", len(h.jwtSecret))
		return nil, fmt.Errorf("jwt parse failed: %w", err)
	}

	if !token.Valid {
		log.Printf("JWT invalid: claims=%v", token.Claims)
		return nil, errors.New("jwt not valid")
	}

	claims, ok := token.Claims.(jwt.MapClaims)
	if !ok {
		return nil, errors.New("invalid token claims")
	}

	userID, ok := claims["user_id"].(string)
	if !ok {
		log.Printf("user_id not in token claims: %v", claims)
		return nil, errors.New("user_id not found in token")
	}

	// Look up user in database
	var user models.User
	if err := database.DB.First(&user, "id = ?", userID).Error; err != nil {
		log.Printf("user not found in database: id=%s, error=%v", userID, err)
		return nil, fmt.Errorf("user not found: %w", err)
	}

	return &user, nil
}

// HandleWebSocket handles WebSocket upgrade requests (Gin wrapper)
// This wraps HandleWebSocketHTTP for use with Gin routes
func (h *Handler) HandleWebSocket(c *gin.Context) {
	h.HandleWebSocketHTTP(c.Writer, c.Request)
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

		// Broadcast presence to followers (simplified - in production, get followers from getstream.io)
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

	// Operational Transform handler for collaborative editing (Task 4.20)
	h.hub.RegisterHandler(MessageTypeOperation, func(client *Client, msg *Message) error {
		var payload OperationPayload
		if err := msg.ParsePayload(&payload); err != nil {
			log.Printf("Failed to parse operation payload: %v", err)
			client.Send(NewErrorMessage("invalid_operation", "Failed to parse operation: "+err.Error()))
			return err
		}

		// Validate operation
		if payload.DocumentID == "" {
			client.Send(NewErrorMessage("invalid_operation", "Document ID is required"))
			return fmt.Errorf("missing document_id")
		}

		if payload.Operation.Type != "Insert" && payload.Operation.Type != "Delete" {
			client.Send(NewErrorMessage("invalid_operation", "Operation type must be Insert or Delete"))
			return fmt.Errorf("invalid operation type: %s", payload.Operation.Type)
		}

		// Convert to CRDT operation
		op := &crdt.Operation{
			Type:            payload.Operation.Type,
			ClientID:        payload.Operation.ClientID,
			ClientTimestamp: payload.Operation.Timestamp,
			Position:        payload.Operation.Position,
			Content:         payload.Operation.Content,
		}

		// For Delete, content contains the deleted text, calculate length
		if op.Type == "Delete" {
			op.Length = len(op.Content)
		}

		// Apply operation through OT (handles transformation)
		transformedOp, err := h.hub.ot.ApplyOperation(payload.DocumentID, op)
		if err != nil {
			log.Printf("Failed to apply operation: %v", err)
			client.Send(NewErrorMessage("operation_failed", "Failed to apply operation: "+err.Error()))
			return err
		}

		// Get updated document content
		currentContent := h.hub.ot.GetDocumentContent(payload.DocumentID)

		// Send acknowledgment to sender
		ackPayload := OperationAckPayload{
			DocumentID:      payload.DocumentID,
			ClientID:        payload.Operation.ClientID,
			ServerTimestamp: transformedOp.ServerTimestamp,
			NewPosition:     transformedOp.Position,
			CurrentContent:  currentContent,
			TransformedOp:   &payload,
			Success:         true,
		}

		// Convert TransformedOp back to payload format for client
		transformedPayload := &OperationPayload{
			DocumentID: payload.DocumentID,
		}
		transformedPayload.Operation.Type = transformedOp.Type
		transformedPayload.Operation.ClientID = transformedOp.ClientID
		transformedPayload.Operation.Timestamp = transformedOp.ServerTimestamp
		transformedPayload.Operation.Position = transformedOp.Position
		transformedPayload.Operation.Content = transformedOp.Content
		ackPayload.TransformedOp = transformedPayload

		client.Send(NewMessage(MessageTypeOperationAck, ackPayload))

		// Broadcast operation to all other clients for this document
		// This allows other clients to apply the same transformation
		h.hub.Broadcast(NewMessage(MessageTypeOperation, payload))

		log.Printf("Applied operation on document %s from client %d (type=%s, position=%d, timestamp=%d)",
			payload.DocumentID, payload.Operation.ClientID, payload.Operation.Type, payload.Operation.Position, transformedOp.ServerTimestamp)

		return nil
	})

	// Typing indicator handler - broadcast to all users that someone is typing
	h.hub.RegisterHandler(MessageTypeUserTyping, func(client *Client, msg *Message) error {
		var payload TypingPayload
		if err := msg.ParsePayload(&payload); err != nil {
			return err
		}

		// Ensure payload has correct user info
		payload.UserID = client.UserID
		payload.Username = client.Username
		payload.Timestamp = time.Now().UnixMilli()

		// Broadcast to all users
		h.hub.Broadcast(NewMessage(MessageTypeUserTyping, payload))
		return nil
	})

	// Stop typing handler - broadcast to all users that someone stopped typing
	h.hub.RegisterHandler(MessageTypeUserStopTyping, func(client *Client, msg *Message) error {
		var payload StopTypingPayload
		if err := msg.ParsePayload(&payload); err != nil {
			return err
		}

		// Ensure payload has correct user info
		payload.UserID = client.UserID
		payload.Timestamp = time.Now().UnixMilli()

		// Broadcast to all users
		h.hub.Broadcast(NewMessage(MessageTypeUserStopTyping, payload))
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

// NotifyFollowRequestAccepted sends a notification when a follow request is accepted
func (h *Handler) NotifyFollowRequestAccepted(requesterID, acceptorID, acceptorUsername string) {
	payload := &FollowRequestAcceptedPayload{
		AcceptorID:   acceptorID,
		AcceptorName: acceptorUsername,
		RequesterID:  requesterID,
	}
	h.hub.SendToUser(requesterID, NewMessage(MessageTypeFollowRequestAccepted, payload))
}

// NotifyNewComment sends a new comment notification to the post owner
func (h *Handler) NotifyNewComment(ownerID string, payload *CommentPayload) {
	h.hub.SendToUser(ownerID, NewMessage(MessageTypeNewComment, payload))
}

// NotifyCommentLike sends a comment like notification to the post owner
func (h *Handler) NotifyCommentLike(ownerID string, payload *CommentLikePayload) {
	h.hub.SendToUser(ownerID, NewMessage(MessageTypeCommentLiked, payload))
}

// NotifyCommentUnlike sends a comment unlike notification to the comment owner
func (h *Handler) NotifyCommentUnlike(ownerID string, payload *CommentLikePayload) {
	h.hub.SendToUser(ownerID, NewMessage(MessageTypeCommentUnliked, payload))
}

// BroadcastLikeCountUpdate broadcasts like count update to all viewers
func (h *Handler) BroadcastLikeCountUpdate(postID string, likeCount int) {
	h.hub.Broadcast(NewMessage(MessageTypeLikeCountUpdate, map[string]interface{}{
		"post_id":    postID,
		"like_count": likeCount,
		"timestamp":  time.Now().UnixMilli(),
	}))
}

// BroadcastCommentCountUpdate broadcasts comment count update to all viewers
func (h *Handler) BroadcastCommentCountUpdate(postID string, commentCount int) {
	h.hub.Broadcast(NewMessage(MessageTypeCommentCountUpdate, map[string]interface{}{
		"post_id":       postID,
		"comment_count": commentCount,
		"timestamp":     time.Now().UnixMilli(),
	}))
}

// BroadcastEngagementMetrics broadcasts both like and comment counts for a post
func (h *Handler) BroadcastEngagementMetrics(postID string, likeCount, commentCount int) {
	h.hub.Broadcast(NewMessage("engagement_metrics", map[string]interface{}{
		"post_id":       postID,
		"like_count":    likeCount,
		"comment_count": commentCount,
		"timestamp":     time.Now().UnixMilli(),
	}))
}

// BroadcastFeedInvalidation signals all users that a specific feed needs refresh
func (h *Handler) BroadcastFeedInvalidation(feedType, reason string) {
	payload := &FeedInvalidatePayload{
		FeedType: feedType,
		Reason:   reason,
	}
	h.hub.Broadcast(NewMessage(MessageTypeFeedInvalidate, payload))
}

// BroadcastTimelineUpdate notifies followers that activity timeline has changed
func (h *Handler) BroadcastTimelineUpdate(userID, feedType string, newCount int) {
	payload := &TimelineUpdatePayload{
		UserID:    userID,
		FeedType:  feedType,
		NewCount:  newCount,
		Timestamp: time.Now().UnixMilli(),
	}
	h.hub.Broadcast(NewMessage(MessageTypeTimelineUpdate, payload))
}

// BroadcastActivity broadcasts a new activity to all connected users
// This enables real-time updates of posts, likes, follows, comments without polling
func (h *Handler) BroadcastActivity(activity *ActivityUpdatePayload) {
	if activity == nil {
		return
	}
	activity.Timestamp = time.Now().UnixMilli()
	h.hub.Broadcast(NewMessage(MessageTypeActivityUpdate, activity))
}

// BroadcastActivityToFollowers broadcasts an activity only to followers of the actor
// This is more targeted than broadcasting to all users
func (h *Handler) BroadcastActivityToFollowers(actorID string, activity *ActivityUpdatePayload) {
	if activity == nil {
		return
	}
	activity.Timestamp = time.Now().UnixMilli()
	// For now, broadcast to all users. In the future, we could query followers from the database
	// and only send to those users for better performance with many users
	h.hub.Broadcast(NewMessage(MessageTypeActivityUpdate, activity))
}

// NotifyNotificationCountUpdate sends updated notification count to a specific user
func (h *Handler) NotifyNotificationCountUpdate(userID string, unreadCount, unseenCount int) {
	payload := &NotificationCountPayload{
		UnreadCount: unreadCount,
		UnseenCount: unseenCount,
		Timestamp:   time.Now().UnixMilli(),
	}
	h.hub.SendToUser(userID, NewMessage(MessageTypeNotificationCountUpdate, payload))
}

// BroadcastUserTyping notifies all users that someone is typing a comment on a post
func (h *Handler) BroadcastUserTyping(postID, userID, username, displayName, avatarURL string) {
	payload := &TypingPayload{
		PostID:      postID,
		UserID:      userID,
		Username:    username,
		DisplayName: displayName,
		AvatarURL:   avatarURL,
		Timestamp:   time.Now().UnixMilli(),
	}
	h.hub.Broadcast(NewMessage(MessageTypeUserTyping, payload))
}

// BroadcastUserStopTyping notifies all users that someone stopped typing
func (h *Handler) BroadcastUserStopTyping(postID, userID string) {
	payload := &StopTypingPayload{
		PostID:    postID,
		UserID:    userID,
		Timestamp: time.Now().UnixMilli(),
	}
	h.hub.Broadcast(NewMessage(MessageTypeUserStopTyping, payload))
}

// Shutdown gracefully shuts down the WebSocket handler
func (h *Handler) Shutdown(ctx context.Context) error {
	return h.hub.Shutdown(ctx)
}

// GetHub returns the hub for external access
func (h *Handler) GetHub() *Hub {
	return h.hub
}
