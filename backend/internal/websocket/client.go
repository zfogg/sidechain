package websocket

import (
	"context"
	"encoding/json"
	"fmt"
	"sync"
	"time"

	"github.com/coder/websocket"
	"github.com/coder/websocket/wsjson"
	"github.com/zfogg/sidechain/backend/internal/logger"
	"go.uber.org/zap"
)

const (
	// Time allowed to write a message to the peer
	writeWait = 10 * time.Second

	// Time allowed to read the next pong message from the peer
	pongWait = 60 * time.Second

	// Send pings to peer with this period (must be less than pongWait)
	pingPeriod = (pongWait * 9) / 10

	// Maximum message size allowed from peer
	maxMessageSize = 512 * 1024 // 512KB

	// Send buffer size
	sendBufferSize = 256
)

// Client represents a single WebSocket connection
type Client struct {
	// The websocket connection
	conn *websocket.Conn

	// Hub reference
	hub *Hub

	// User information
	UserID   string
	Username string

	// Buffered channel of outbound messages
	send chan []byte

	// Connection metadata
	ConnectedAt time.Time
	LastPingAt  time.Time
	RemoteAddr  string
	UserAgent   string

	// Rate limiting
	rateLimiter *RateLimiter

	// Context for cancellation
	ctx    context.Context
	cancel context.CancelFunc

	// Mutex for connection state
	mu sync.RWMutex

	// Closed flag
	closed bool
}

// RateLimiter implements a simple token bucket rate limiter
type RateLimiter struct {
	tokens    float64
	maxTokens float64
	refill    float64
	lastTime  time.Time
	mu        sync.Mutex
}

// NewRateLimiter creates a new rate limiter
func NewRateLimiter(maxPerSecond int, burst int) *RateLimiter {
	return &RateLimiter{
		tokens:    float64(burst),
		maxTokens: float64(burst),
		refill:    float64(maxPerSecond),
		lastTime:  time.Now(),
	}
}

// Allow checks if an action is allowed and consumes a token
func (r *RateLimiter) Allow() bool {
	r.mu.Lock()
	defer r.mu.Unlock()

	now := time.Now()
	elapsed := now.Sub(r.lastTime).Seconds()
	r.lastTime = now

	// Refill tokens
	r.tokens += elapsed * r.refill
	if r.tokens > r.maxTokens {
		r.tokens = r.maxTokens
	}

	// Check and consume
	if r.tokens >= 1 {
		r.tokens--
		return true
	}
	return false
}

// NewClient creates a new Client
func NewClient(hub *Hub, conn *websocket.Conn, userID, username string) *Client {
	ctx, cancel := context.WithCancel(context.Background())
	config := hub.GetRateLimitConfig()

	return &Client{
		hub:         hub,
		conn:        conn,
		UserID:      userID,
		Username:    username,
		send:        make(chan []byte, sendBufferSize),
		ConnectedAt: time.Now(),
		rateLimiter: NewRateLimiter(config.MaxMessagesPerSecond, config.BurstSize),
		ctx:         ctx,
		cancel:      cancel,
	}
}

// ReadPump pumps messages from the WebSocket connection to the hub
func (c *Client) ReadPump() {
	defer func() {
		c.hub.Unregister(c)
		c.Close()
	}()

	// Set read limit
	c.conn.SetReadLimit(maxMessageSize)

	for {
		select {
		case <-c.ctx.Done():
			return
		default:
		}

		// Read message with timeout context
		readCtx, readCancel := context.WithTimeout(c.ctx, pongWait)
		_, data, err := c.conn.Read(readCtx)
		readCancel()

		if err != nil {
			if websocket.CloseStatus(err) == websocket.StatusNormalClosure ||
				websocket.CloseStatus(err) == websocket.StatusGoingAway {
				logger.Log.Info("Client disconnected normally", zap.String("user", c.UserID))
			} else if c.ctx.Err() == nil {
				// Only log errors if we're not shutting down
				logger.Log.Error("Read error for client", zap.String("user", c.UserID), zap.Error(err))
				c.hub.metrics.Errors.Add(1)
			}
			return
		}

		// Rate limiting
		if !c.rateLimiter.Allow() {
			c.SendError("rate_limited", "Too many messages, please slow down")
			c.hub.metrics.Errors.Add(1)
			continue
		}

		// Update metrics
		c.hub.metrics.MessagesReceived.Add(1)

		// Parse the message
		var message Message
		if err := json.Unmarshal(data, &message); err != nil {
			logger.Log.Warn("WebSocket JSON parse error",
				zap.String("user", c.UserID),
				zap.Error(err))
			c.SendError("invalid_json", "Failed to parse message")
			continue
		}

		// Handle the message
		c.handleMessage(&message)
	}
}

// WritePump pumps messages from the hub to the WebSocket connection
func (c *Client) WritePump() {
	ticker := time.NewTicker(pingPeriod)
	defer func() {
		ticker.Stop()
		c.Close()
	}()

	for {
		select {
		case <-c.ctx.Done():
			// Send close message
			c.conn.Close(websocket.StatusGoingAway, "server shutdown")
			return

		case message, ok := <-c.send:
			if !ok {
				// Hub closed the channel
				c.conn.Close(websocket.StatusNormalClosure, "closing")
				return
			}

			ctx, cancel := context.WithTimeout(c.ctx, writeWait)
			err := c.conn.Write(ctx, websocket.MessageText, message)
			cancel()

			if err != nil {
				logger.Log.Error("Write error for client", zap.String("user", c.UserID), zap.Error(err))
				c.hub.metrics.Errors.Add(1)
				return
			}

		case <-ticker.C:
			// Send ping
			c.mu.Lock()
			c.LastPingAt = time.Now()
			c.mu.Unlock()

			ctx, cancel := context.WithTimeout(c.ctx, writeWait)
			err := c.conn.Ping(ctx)
			cancel()

			if err != nil {
				logger.Log.Warn("Ping failed for client", zap.String("user", c.UserID), zap.Error(err))
				return
			}
		}
	}
}

// handleMessage routes incoming messages to appropriate handlers
func (c *Client) handleMessage(message *Message) {
	// Update timestamp if not set
	if message.Timestamp.IsZero() {
		message.Timestamp = FlexibleTime{Time: time.Now().UTC()}
	}

	// Handle built-in message types
	switch message.Type {
	case MessageTypePing, "heartbeat": // "heartbeat" is an alias for ping
		c.handlePing(message)
		return

	case MessageTypeAuth:
		// Auth is handled during connection, but client might re-auth
		c.handleAuth(message)
		return
	}

	// Check for registered handler
	if handler, ok := c.hub.GetHandler(message.Type); ok {
		if err := handler(c, message); err != nil {
			logger.Log.Error("Handler error",
				zap.String("type", message.Type),
				zap.Error(err))
			c.SendError("handler_error", fmt.Sprintf("Failed to process %s", message.Type))
		}
		return
	}

	// Unknown message type
	logger.Log.Warn("Unknown message type",
		zap.String("user", c.UserID),
		zap.String("type", message.Type))
	c.SendError("unknown_type", fmt.Sprintf("Unknown message type: %s", message.Type))
}

// handlePing responds to ping messages with pong
func (c *Client) handlePing(message *Message) {
	var ping PingPayload
	if err := message.ParsePayload(&ping); err != nil {
		ping.ClientTime = 0
	}

	serverTime := time.Now().UnixMilli()
	latency := serverTime - ping.ClientTime

	pong := NewMessage(MessageTypePong, PongPayload{
		ClientTime: ping.ClientTime,
		ServerTime: serverTime,
		Latency:    latency,
	})

	if message.ID != "" {
		pong.ReplyTo = message.ID
	}

	// Best-effort pong response - connection may be closing
	_ = c.Send(pong)
}

// handleAuth handles re-authentication requests
func (c *Client) handleAuth(message *Message) {
	// For now, just acknowledge - real auth happens at connection time
	c.Send(NewMessage(MessageTypeAuth, AuthPayload{
		UserID: c.UserID,
		Status: "authenticated",
	}))
}

// Send sends a message to this client
func (c *Client) Send(message *Message) error {
	c.mu.RLock()
	if c.closed {
		c.mu.RUnlock()
		return fmt.Errorf("client connection closed")
	}
	c.mu.RUnlock()

	data, err := json.Marshal(message)
	if err != nil {
		return err
	}

	select {
	case c.send <- data:
		return nil
	case <-c.ctx.Done():
		return fmt.Errorf("client shutting down")
	default:
		return fmt.Errorf("send buffer full")
	}
}

// SendJSON sends a typed message using wsjson
func (c *Client) SendJSON(ctx context.Context, v interface{}) error {
	return wsjson.Write(ctx, c.conn, v)
}

// SendError sends an error message to the client
func (c *Client) SendError(code, message string) {
	c.Send(NewErrorMessage(code, message))
}

// Close closes the client connection
func (c *Client) Close() {
	c.mu.Lock()
	defer c.mu.Unlock()

	if c.closed {
		return
	}
	c.closed = true

	// Cancel context
	c.cancel()

	// Close the WebSocket connection
	c.conn.Close(websocket.StatusNormalClosure, "closing")
}

// IsClosed returns whether the client connection is closed
func (c *Client) IsClosed() bool {
	c.mu.RLock()
	defer c.mu.RUnlock()
	return c.closed
}

// GetInfo returns client information
func (c *Client) GetInfo() ClientInfo {
	c.mu.RLock()
	defer c.mu.RUnlock()
	return ClientInfo{
		UserID:      c.UserID,
		Username:    c.Username,
		ConnectedAt: c.ConnectedAt,
		LastPingAt:  c.LastPingAt,
		RemoteAddr:  c.RemoteAddr,
		UserAgent:   c.UserAgent,
	}
}

// ClientInfo represents public client information
type ClientInfo struct {
	UserID      string    `json:"user_id"`
	Username    string    `json:"username"`
	ConnectedAt time.Time `json:"connected_at"`
	LastPingAt  time.Time `json:"last_ping_at"`
	RemoteAddr  string    `json:"remote_addr"`
	UserAgent   string    `json:"user_agent"`
}
