// Package websocket provides WebSocket infrastructure for real-time communication.
// Uses github.com/coder/websocket - the modern, context-aware WebSocket library for Go.
package websocket

import (
	"context"
	"encoding/json"
	"fmt"
	"log"
	"sync"
	"sync/atomic"
	"time"
)

// Hub maintains the set of active clients and broadcasts messages to clients.
type Hub struct {
	// Registered clients by user ID for targeted messaging
	clients map[string]map[*Client]struct{}

	// All clients for broadcasting
	allClients map[*Client]struct{}

	// Register requests from clients
	register chan *Client

	// Unregister requests from clients
	unregister chan *Client

	// Broadcast messages to all clients
	broadcast chan *Message

	// Send message to specific user
	unicast chan *UnicastMessage

	// Mutex for client map access
	mu sync.RWMutex

	// Metrics
	metrics *Metrics

	// Shutdown handling
	ctx    context.Context
	cancel context.CancelFunc
	wg     sync.WaitGroup

	// Message handlers
	handlers map[string]MessageHandler

	// Rate limiter config
	rateLimitConfig RateLimitConfig
}

// Metrics tracks WebSocket statistics
type Metrics struct {
	TotalConnections   atomic.Int64
	ActiveConnections  atomic.Int64
	MessagesReceived   atomic.Int64
	MessagesSent       atomic.Int64
	Errors             atomic.Int64
	ConnectionsDropped atomic.Int64
}

// RateLimitConfig defines rate limiting parameters
type RateLimitConfig struct {
	// MaxMessagesPerSecond per client
	MaxMessagesPerSecond int
	// BurstSize allows short bursts above the rate
	BurstSize int
	// Window for rate calculation
	Window time.Duration
}

// DefaultRateLimitConfig returns sensible defaults
func DefaultRateLimitConfig() RateLimitConfig {
	return RateLimitConfig{
		MaxMessagesPerSecond: 10,
		BurstSize:            20,
		Window:               time.Second,
	}
}

// UnicastMessage is a message targeted at a specific user
type UnicastMessage struct {
	UserID  string
	Message *Message
}

// MessageHandler processes incoming messages of a specific type
type MessageHandler func(client *Client, message *Message) error

// NewHub creates a new Hub instance
func NewHub() *Hub {
	ctx, cancel := context.WithCancel(context.Background())
	return &Hub{
		clients:         make(map[string]map[*Client]struct{}),
		allClients:      make(map[*Client]struct{}),
		register:        make(chan *Client, 256),
		unregister:      make(chan *Client, 256),
		broadcast:       make(chan *Message, 256),
		unicast:         make(chan *UnicastMessage, 256),
		metrics:         &Metrics{},
		ctx:             ctx,
		cancel:          cancel,
		handlers:        make(map[string]MessageHandler),
		rateLimitConfig: DefaultRateLimitConfig(),
	}
}

// RegisterHandler registers a handler for a specific message type
func (h *Hub) RegisterHandler(msgType string, handler MessageHandler) {
	h.mu.Lock()
	defer h.mu.Unlock()
	h.handlers[msgType] = handler
	log.Printf("📨 Registered handler for message type: %s", msgType)
}

// GetHandler returns the handler for a message type
func (h *Hub) GetHandler(msgType string) (MessageHandler, bool) {
	h.mu.RLock()
	defer h.mu.RUnlock()
	handler, ok := h.handlers[msgType]
	return handler, ok
}

// Run starts the hub's main event loop
func (h *Hub) Run() {
	log.Println("🔌 WebSocket hub starting...")

	for {
		select {
		case <-h.ctx.Done():
			log.Println("🔌 WebSocket hub shutting down...")
			h.shutdown()
			return

		case client := <-h.register:
			h.registerClient(client)

		case client := <-h.unregister:
			h.unregisterClient(client)

		case message := <-h.broadcast:
			h.broadcastMessage(message)

		case unicast := <-h.unicast:
			h.sendToUser(unicast.UserID, unicast.Message)
		}
	}
}

// registerClient adds a client to the hub
func (h *Hub) registerClient(client *Client) {
	h.mu.Lock()
	defer h.mu.Unlock()

	// Add to user-specific map
	if h.clients[client.UserID] == nil {
		h.clients[client.UserID] = make(map[*Client]struct{})
	}
	h.clients[client.UserID][client] = struct{}{}

	// Add to all clients
	h.allClients[client] = struct{}{}

	// Update metrics
	h.metrics.TotalConnections.Add(1)
	h.metrics.ActiveConnections.Add(1)

	log.Printf("✅ Client connected: user=%s, active=%d", client.UserID, h.metrics.ActiveConnections.Load())
}

// unregisterClient removes a client from the hub
func (h *Hub) unregisterClient(client *Client) {
	h.mu.Lock()
	defer h.mu.Unlock()

	if _, ok := h.allClients[client]; ok {
		delete(h.allClients, client)

		// Remove from user-specific map
		if clients, ok := h.clients[client.UserID]; ok {
			delete(clients, client)
			if len(clients) == 0 {
				delete(h.clients, client.UserID)
			}
		}

		// Close the client's send channel
		close(client.send)

		// Update metrics
		h.metrics.ActiveConnections.Add(-1)

		log.Printf("❌ Client disconnected: user=%s, active=%d", client.UserID, h.metrics.ActiveConnections.Load())
	}
}

// broadcastMessage sends a message to all connected clients
func (h *Hub) broadcastMessage(message *Message) {
	h.mu.RLock()
	defer h.mu.RUnlock()

	data, err := json.Marshal(message)
	if err != nil {
		log.Printf("Error marshaling broadcast message: %v", err)
		return
	}

	for client := range h.allClients {
		select {
		case client.send <- data:
			h.metrics.MessagesSent.Add(1)
		default:
			// Client's buffer is full, mark for removal
			h.metrics.ConnectionsDropped.Add(1)
			go func(c *Client) {
				h.unregister <- c
			}(client)
		}
	}
}

// sendToUser sends a message to all connections for a specific user
func (h *Hub) sendToUser(userID string, message *Message) {
	h.mu.RLock()
	clients, ok := h.clients[userID]
	h.mu.RUnlock()

	if !ok || len(clients) == 0 {
		return
	}

	data, err := json.Marshal(message)
	if err != nil {
		log.Printf("Error marshaling unicast message: %v", err)
		return
	}

	h.mu.RLock()
	defer h.mu.RUnlock()

	for client := range clients {
		select {
		case client.send <- data:
			h.metrics.MessagesSent.Add(1)
		default:
			// Client's buffer is full, mark for removal
			h.metrics.ConnectionsDropped.Add(1)
			go func(c *Client) {
				h.unregister <- c
			}(client)
		}
	}
}

// Broadcast sends a message to all connected clients
func (h *Hub) Broadcast(message *Message) {
	select {
	case h.broadcast <- message:
	case <-h.ctx.Done():
	}
}

// SendToUser sends a message to a specific user (all their connections)
func (h *Hub) SendToUser(userID string, message *Message) {
	select {
	case h.unicast <- &UnicastMessage{UserID: userID, Message: message}:
	case <-h.ctx.Done():
	}
}

// Register adds a client to the hub
func (h *Hub) Register(client *Client) {
	select {
	case h.register <- client:
	case <-h.ctx.Done():
	}
}

// Unregister removes a client from the hub
func (h *Hub) Unregister(client *Client) {
	select {
	case h.unregister <- client:
	case <-h.ctx.Done():
	}
}

// IsUserOnline checks if a user has any active connections
func (h *Hub) IsUserOnline(userID string) bool {
	h.mu.RLock()
	defer h.mu.RUnlock()
	clients, ok := h.clients[userID]
	return ok && len(clients) > 0
}

// GetUserConnectionCount returns the number of connections for a user
func (h *Hub) GetUserConnectionCount(userID string) int {
	h.mu.RLock()
	defer h.mu.RUnlock()
	if clients, ok := h.clients[userID]; ok {
		return len(clients)
	}
	return 0
}

// GetOnlineUsers returns a list of all online user IDs
func (h *Hub) GetOnlineUsers() []string {
	h.mu.RLock()
	defer h.mu.RUnlock()

	users := make([]string, 0, len(h.clients))
	for userID := range h.clients {
		users = append(users, userID)
	}
	return users
}

// GetMetrics returns current WebSocket metrics
func (h *Hub) GetMetrics() MetricsSnapshot {
	return MetricsSnapshot{
		TotalConnections:   h.metrics.TotalConnections.Load(),
		ActiveConnections:  h.metrics.ActiveConnections.Load(),
		MessagesReceived:   h.metrics.MessagesReceived.Load(),
		MessagesSent:       h.metrics.MessagesSent.Load(),
		Errors:             h.metrics.Errors.Load(),
		ConnectionsDropped: h.metrics.ConnectionsDropped.Load(),
	}
}

// MetricsSnapshot is a point-in-time snapshot of metrics
type MetricsSnapshot struct {
	TotalConnections   int64 `json:"total_connections"`
	ActiveConnections  int64 `json:"active_connections"`
	MessagesReceived   int64 `json:"messages_received"`
	MessagesSent       int64 `json:"messages_sent"`
	Errors             int64 `json:"errors"`
	ConnectionsDropped int64 `json:"connections_dropped"`
}

// String implements Stringer for MetricsSnapshot
func (m MetricsSnapshot) String() string {
	return fmt.Sprintf(
		"connections=%d/%d messages=rx:%d/tx:%d errors=%d dropped=%d",
		m.ActiveConnections, m.TotalConnections,
		m.MessagesReceived, m.MessagesSent,
		m.Errors, m.ConnectionsDropped,
	)
}

// Shutdown gracefully shuts down the hub
func (h *Hub) Shutdown(ctx context.Context) error {
	log.Println("🔌 Initiating WebSocket hub shutdown...")

	// Cancel the hub's context to stop the main loop
	h.cancel()

	// Wait for shutdown to complete or context to expire
	done := make(chan struct{})
	go func() {
		h.wg.Wait()
		close(done)
	}()

	select {
	case <-done:
		log.Println("🔌 WebSocket hub shutdown complete")
		return nil
	case <-ctx.Done():
		return fmt.Errorf("shutdown timeout: %w", ctx.Err())
	}
}

// shutdown closes all client connections
func (h *Hub) shutdown() {
	h.mu.Lock()
	defer h.mu.Unlock()

	// Send shutdown message to all clients
	shutdownMsg := &Message{
		Type:      MessageTypeSystem,
		Payload:   map[string]interface{}{"event": "server_shutdown"},
		Timestamp: FlexibleTime{Time: time.Now().UTC()},
	}
	data, _ := json.Marshal(shutdownMsg)

	for client := range h.allClients {
		// Try to send shutdown message
		select {
		case client.send <- data:
		default:
		}
		// Close the send channel
		close(client.send)
	}

	// Clear all maps
	h.clients = make(map[string]map[*Client]struct{})
	h.allClients = make(map[*Client]struct{})

	log.Printf("🔌 Closed %d connections during shutdown", h.metrics.ActiveConnections.Load())
}

// SetRateLimitConfig updates the rate limiting configuration
func (h *Hub) SetRateLimitConfig(config RateLimitConfig) {
	h.mu.Lock()
	defer h.mu.Unlock()
	h.rateLimitConfig = config
}

// GetRateLimitConfig returns the current rate limit configuration
func (h *Hub) GetRateLimitConfig() RateLimitConfig {
	h.mu.RLock()
	defer h.mu.RUnlock()
	return h.rateLimitConfig
}
