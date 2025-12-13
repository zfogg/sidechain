// Package websocket provides presence tracking for real-time user status.
package websocket

import (
	"context"
	"log"
	"sync"
	"time"

	"github.com/zfogg/sidechain/backend/internal/database"
	"github.com/zfogg/sidechain/backend/internal/models"
	"github.com/zfogg/sidechain/backend/internal/stream"
)

// PresenceStatus represents the current status of a user
type PresenceStatus string

const (
	StatusOnline   PresenceStatus = "online"
	StatusOffline  PresenceStatus = "offline"
	StatusInStudio PresenceStatus = "in_studio"
)

// UserPresence tracks a single user's presence state
type UserPresence struct {
	UserID       string         `json:"user_id"`
	Username     string         `json:"username"`
	Status       PresenceStatus `json:"status"`
	DAW          string         `json:"daw,omitempty"`
	LastActivity time.Time      `json:"last_activity"`
	ConnectedAt  time.Time      `json:"connected_at"`
}

// PresenceManager tracks user presence and broadcasts updates to followers
type PresenceManager struct {
	hub          *Hub
	streamClient *stream.Client

	// In-memory presence storage
	presence map[string]*UserPresence
	mu       sync.RWMutex

	// Configuration
	timeoutDuration time.Duration // How long before a user is considered offline

	// Shutdown handling
	ctx    context.Context
	cancel context.CancelFunc
}

// PresenceConfig holds configuration for the presence manager
type PresenceConfig struct {
	TimeoutDuration time.Duration // Default: 5 minutes
}

// DefaultPresenceConfig returns sensible defaults
func DefaultPresenceConfig() PresenceConfig {
	return PresenceConfig{
		TimeoutDuration: 5 * time.Minute,
	}
}

// NewPresenceManager creates a new presence manager
func NewPresenceManager(hub *Hub, streamClient *stream.Client, config PresenceConfig) *PresenceManager {
	ctx, cancel := context.WithCancel(context.Background())

	if config.TimeoutDuration == 0 {
		config.TimeoutDuration = 5 * time.Minute
	}

	pm := &PresenceManager{
		hub:             hub,
		streamClient:    streamClient,
		presence:        make(map[string]*UserPresence),
		timeoutDuration: config.TimeoutDuration,
		ctx:             ctx,
		cancel:          cancel,
	}

	return pm
}

// Start begins the presence manager's background tasks
func (pm *PresenceManager) Start() {
	log.Println("👤 Presence manager starting...")

	// Start timeout checker
	go pm.runTimeoutChecker()

	// Register message handlers with the hub
	pm.registerHandlers()

	log.Println("👤 Presence manager started")
}

// Stop gracefully shuts down the presence manager
func (pm *PresenceManager) Stop() {
	log.Println("👤 Presence manager stopping...")
	pm.cancel()

	// Mark all users as offline
	pm.mu.Lock()
	for userID := range pm.presence {
		pm.setOfflineInternal(userID)
	}
	pm.mu.Unlock()

	log.Println("👤 Presence manager stopped")
}

// registerHandlers sets up message handlers for presence-related messages
func (pm *PresenceManager) registerHandlers() {
	// Handle presence updates from clients
	pm.hub.RegisterHandler(MessageTypePresence, func(client *Client, msg *Message) error {
		var payload PresencePayload
		if err := msg.ParsePayload(&payload); err != nil {
			return err
		}

		// Validate and process the presence update
		status := PresenceStatus(payload.Status)
		if status != StatusOnline && status != StatusInStudio {
			status = StatusOnline
		}

		pm.UpdatePresence(client.UserID, client.Username, status, payload.DAW)
		return nil
	})

	// Handle "in studio" status updates
	pm.hub.RegisterHandler(MessageTypeUserInStudio, func(client *Client, msg *Message) error {
		var payload PresencePayload
		if err := msg.ParsePayload(&payload); err != nil {
			return err
		}

		pm.UpdatePresence(client.UserID, client.Username, StatusInStudio, payload.DAW)
		return nil
	})
}

// OnClientConnect is called when a client connects
func (pm *PresenceManager) OnClientConnect(client *Client) {
	pm.UpdatePresence(client.UserID, client.Username, StatusOnline, "")
}

// OnClientDisconnect is called when a client disconnects
func (pm *PresenceManager) OnClientDisconnect(client *Client) {
	// Check if the user has other active connections
	if pm.hub.GetUserConnectionCount(client.UserID) <= 1 {
		// This was their last connection, mark as offline
		pm.SetOffline(client.UserID)
	}
}

// UpdatePresence updates a user's presence and notifies followers
func (pm *PresenceManager) UpdatePresence(userID, username string, status PresenceStatus, daw string) {
	pm.mu.Lock()

	existing := pm.presence[userID]
	isNewOnline := existing == nil || existing.Status == StatusOffline

	now := time.Now()

	if existing == nil {
		pm.presence[userID] = &UserPresence{
			UserID:       userID,
			Username:     username,
			Status:       status,
			DAW:          daw,
			LastActivity: now,
			ConnectedAt:  now,
		}
	} else {
		existing.Status = status
		existing.LastActivity = now
		if daw != "" {
			existing.DAW = daw
		}
		if existing.Username == "" {
			existing.Username = username
		}
	}

	presence := pm.presence[userID]
	pm.mu.Unlock()

	// Update database (non-blocking)
	go pm.updateDatabasePresence(userID, status == StatusOnline || status == StatusInStudio)

	// Broadcast to followers if this is a status change
	if isNewOnline {
		go pm.broadcastToFollowers(userID, MessageTypeUserOnline, PresencePayload{
			UserID:    userID,
			Status:    string(presence.Status),
			DAW:       presence.DAW,
			Timestamp: now.UnixMilli(),
		})
	} else if status == StatusInStudio && (existing == nil || existing.Status != StatusInStudio) {
		go pm.broadcastToFollowers(userID, MessageTypeUserInStudio, PresencePayload{
			UserID:    userID,
			Status:    string(StatusInStudio),
			DAW:       daw,
			Timestamp: now.UnixMilli(),
		})
	}
}

// SetOffline marks a user as offline
func (pm *PresenceManager) SetOffline(userID string) {
	pm.mu.Lock()
	pm.setOfflineInternal(userID)
	pm.mu.Unlock()
}

// setOfflineInternal marks a user as offline (must hold lock)
func (pm *PresenceManager) setOfflineInternal(userID string) {
	if presence, ok := pm.presence[userID]; ok {
		wasOnline := presence.Status != StatusOffline
		presence.Status = StatusOffline
		presence.LastActivity = time.Now()

		if wasOnline {
			// Update database (non-blocking)
			go pm.updateDatabasePresence(userID, false)

			// Broadcast to followers
			go pm.broadcastToFollowers(userID, MessageTypeUserOffline, PresencePayload{
				UserID:    userID,
				Status:    string(StatusOffline),
				Timestamp: time.Now().UnixMilli(),
			})
		}
	}
}

// GetPresence returns a user's current presence
func (pm *PresenceManager) GetPresence(userID string) *UserPresence {
	pm.mu.RLock()
	defer pm.mu.RUnlock()

	if presence, ok := pm.presence[userID]; ok {
		// Return a copy
		return &UserPresence{
			UserID:       presence.UserID,
			Username:     presence.Username,
			Status:       presence.Status,
			DAW:          presence.DAW,
			LastActivity: presence.LastActivity,
			ConnectedAt:  presence.ConnectedAt,
		}
	}
	return nil
}

// GetOnlinePresence returns presence for multiple users (only online ones)
func (pm *PresenceManager) GetOnlinePresence(userIDs []string) map[string]*UserPresence {
	pm.mu.RLock()
	defer pm.mu.RUnlock()

	result := make(map[string]*UserPresence)
	for _, userID := range userIDs {
		if presence, ok := pm.presence[userID]; ok && presence.Status != StatusOffline {
			result[userID] = &UserPresence{
				UserID:       presence.UserID,
				Username:     presence.Username,
				Status:       presence.Status,
				DAW:          presence.DAW,
				LastActivity: presence.LastActivity,
				ConnectedAt:  presence.ConnectedAt,
			}
		}
	}
	return result
}

// GetOnlineCount returns the count of online users from a list
func (pm *PresenceManager) GetOnlineCount(userIDs []string) int {
	pm.mu.RLock()
	defer pm.mu.RUnlock()

	count := 0
	for _, userID := range userIDs {
		if presence, ok := pm.presence[userID]; ok && presence.Status != StatusOffline {
			count++
		}
	}
	return count
}

// GetAllOnline returns all currently online users
func (pm *PresenceManager) GetAllOnline() []*UserPresence {
	pm.mu.RLock()
	defer pm.mu.RUnlock()

	result := make([]*UserPresence, 0)
	for _, presence := range pm.presence {
		if presence.Status != StatusOffline {
			result = append(result, &UserPresence{
				UserID:       presence.UserID,
				Username:     presence.Username,
				Status:       presence.Status,
				DAW:          presence.DAW,
				LastActivity: presence.LastActivity,
				ConnectedAt:  presence.ConnectedAt,
			})
		}
	}
	return result
}

// Heartbeat updates the last activity time for a user
func (pm *PresenceManager) Heartbeat(userID string) {
	pm.mu.Lock()
	defer pm.mu.Unlock()

	if presence, ok := pm.presence[userID]; ok {
		presence.LastActivity = time.Now()
	}
}

// runTimeoutChecker periodically checks for timed-out users
func (pm *PresenceManager) runTimeoutChecker() {
	ticker := time.NewTicker(time.Minute)
	defer ticker.Stop()

	for {
		select {
		case <-pm.ctx.Done():
			return
		case <-ticker.C:
			pm.checkTimeouts()
		}
	}
}

// checkTimeouts marks users as offline if they haven't sent a heartbeat
func (pm *PresenceManager) checkTimeouts() {
	pm.mu.Lock()
	defer pm.mu.Unlock()

	cutoff := time.Now().Add(-pm.timeoutDuration)

	for userID, presence := range pm.presence {
		if presence.Status != StatusOffline && presence.LastActivity.Before(cutoff) {
			// Also check if they have active WebSocket connections
			if !pm.hub.IsUserOnline(userID) {
				log.Printf("👤 Presence timeout for user %s (last activity: %v)", userID, presence.LastActivity)
				pm.setOfflineInternal(userID)
			} else {
				// They have connections but no heartbeat, update activity
				presence.LastActivity = time.Now()
			}
		}
	}
}

// broadcastToFollowers sends a presence update to all of a user's followers
func (pm *PresenceManager) broadcastToFollowers(userID string, msgType string, payload PresencePayload) {
	// Check if user has activity status visibility enabled (Feature #9)
	var showActivityStatus bool
	if err := database.DB.Model(&models.User{}).Select("show_activity_status").
		Where("id = ?", userID).Scan(&showActivityStatus).Error; err != nil {
		showActivityStatus = true // Default to showing on error
	}
	if !showActivityStatus {
		log.Printf("👤 Skipping presence broadcast for user %s (activity status hidden)", userID)
		return
	}

	if pm.streamClient == nil {
		// If no stream client, just log
		log.Printf("👤 Presence update for %s: %s (no stream client to notify followers)", userID, payload.Status)
		return
	}

	// Get followers from getstream.io
	followers, err := pm.streamClient.GetFollowers(userID, 1000, 0) // Get up to 1000 followers
	if err != nil {
		log.Printf("Error getting followers for presence broadcast: %v", err)
		return
	}

	if len(followers) == 0 {
		return
	}

	// Build message
	msg := NewMessage(msgType, payload)

	// Send to each online follower
	for _, follower := range followers {
		if pm.hub.IsUserOnline(follower.UserID) {
			pm.hub.SendToUser(follower.UserID, msg)
		}
	}

	log.Printf("👤 Broadcasted %s to %d followers of user %s", msgType, len(followers), userID)
}

// updateDatabasePresence updates the user's online status in the database
func (pm *PresenceManager) updateDatabasePresence(userID string, isOnline bool) {
	if database.DB == nil {
		return
	}

	now := time.Now()
	updates := map[string]interface{}{
		"is_online":      isOnline,
		"last_active_at": now,
	}

	if err := database.DB.Model(&models.User{}).Where("id = ?", userID).Updates(updates).Error; err != nil {
		log.Printf("Error updating user presence in database: %v", err)
	}
}
