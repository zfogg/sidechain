package websocket

import (
	"context"
	"encoding/json"
	"fmt"
	"math"
	"math/rand"
	"net/url"
	"sync"
	"sync/atomic"
	"time"

	"github.com/gorilla/websocket"
	"github.com/zfogg/sidechain/cli/pkg/logger"
)

// MessageType represents the type of WebSocket message
type MessageType string

const (
	MessageTypeNewPost             MessageType = "new_post"
	MessageTypePostLiked           MessageType = "post_liked"
	MessageTypePostCommented       MessageType = "post_commented"
	MessageTypeUserFollowed        MessageType = "user_followed"
	MessageTypeCommentLiked        MessageType = "comment_liked"
	MessageTypePostSaved           MessageType = "post_saved"
	MessageTypeNotification        MessageType = "notification"
	MessageTypePresenceUpdate      MessageType = "presence_update"
	MessageTypePlayCountUpdate     MessageType = "play_count_update"
	MessageTypeLikeCountUpdate     MessageType = "like_count_update"
	MessageTypeFollowerCountUpdate MessageType = "follower_count_update"
	MessageTypeHeartbeat           MessageType = "heartbeat"
	MessageTypePong                MessageType = "pong"
	MessageTypeError               MessageType = "error"
)

// WebSocketMessage represents a message from the server
type WebSocketMessage struct {
	Type    MessageType `json:"type"`
	Payload interface{} `json:"payload"`
}

// Config holds WebSocket client configuration
type Config struct {
	Host                  string
	Port                  int
	Path                  string
	UseTLS                bool
	ConnectTimeoutMs      int
	HeartbeatIntervalMs   int
	ReconnectBaseDelayMs  int
	ReconnectMaxDelayMs   int
	MaxReconnectAttempts  int
}

// DefaultConfig returns a development configuration
func DefaultConfig() Config {
	return Config{
		Host:                 "localhost",
		Port:                 8787,
		Path:                 "/api/v1/ws",
		UseTLS:               false,
		ConnectTimeoutMs:     15000,
		HeartbeatIntervalMs:  30000,
		ReconnectBaseDelayMs: 2000,
		ReconnectMaxDelayMs:  30000,
		MaxReconnectAttempts: -1, // unlimited
	}
}

// ProductionConfig returns a production configuration
func ProductionConfig(host string) Config {
	cfg := DefaultConfig()
	cfg.Host = host
	cfg.Port = 443
	cfg.UseTLS = true
	cfg.ReconnectMaxDelayMs = 60000
	return cfg
}

// Client manages WebSocket connections
type Client struct {
	config             Config
	conn               *websocket.Conn
	token              string
	state              atomic.Value // ConnectionState
	mu                 sync.RWMutex
	reconnectAttempts  int
	reconnectDelay     int
	listeners          map[MessageType][]func(interface{})
	listenersMu        sync.RWMutex
	done               chan struct{}
	ctx                context.Context
	cancel             context.CancelFunc
	messageQueue       chan WebSocketMessage
	statsLock          sync.RWMutex
	stats              ConnectionStats
}

// ConnectionState represents the state of the WebSocket connection
type ConnectionState int

const (
	StateDisconnected ConnectionState = iota
	StateConnecting
	StateConnected
	StateReconnecting
	StateError
)

// ConnectionStats holds connection statistics
type ConnectionStats struct {
	MessagesReceived int64
	MessagesSent     int64
	ReconnectCount   int
	LastError        string
	ConnectedAt      time.Time
	DisconnectedAt   time.Time
}

// NewClient creates a new WebSocket client
func NewClient(config Config) *Client {
	ctx, cancel := context.WithCancel(context.Background())
	client := &Client{
		config:       config,
		listeners:    make(map[MessageType][]func(interface{})),
		done:         make(chan struct{}),
		ctx:          ctx,
		cancel:       cancel,
		messageQueue: make(chan WebSocketMessage, 100),
		reconnectDelay: config.ReconnectBaseDelayMs,
	}
	client.state.Store(StateDisconnected)
	return client
}

// SetAuthToken sets the JWT token for authentication
func (c *Client) SetAuthToken(token string) {
	c.mu.Lock()
	defer c.mu.Unlock()
	c.token = token
}

// Connect establishes the WebSocket connection
func (c *Client) Connect(token string) error {
	c.SetAuthToken(token)

	c.setState(StateConnecting)

	conn, err := c.dial()
	if err != nil {
		c.setState(StateError)
		c.recordError(err.Error())
		return err
	}

	c.mu.Lock()
	c.conn = conn
	c.mu.Unlock()

	c.setState(StateConnected)
	c.reconnectAttempts = 0
	c.reconnectDelay = c.config.ReconnectBaseDelayMs
	c.recordConnected()

	// Start read and heartbeat loops
	go c.readLoop()
	go c.heartbeatLoop()

	logger.Debug("WebSocket connected", "host", c.config.Host, "port", c.config.Port)
	return nil
}

// Disconnect closes the WebSocket connection
func (c *Client) Disconnect() error {
	c.cancel()

	c.mu.Lock()
	if c.conn != nil {
		c.conn.Close()
		c.conn = nil
	}
	c.mu.Unlock()

	c.setState(StateDisconnected)
	c.recordDisconnected()

	logger.Debug("WebSocket disconnected")
	return nil
}

// IsConnected returns true if the connection is established
func (c *Client) IsConnected() bool {
	return c.getState() == StateConnected
}

// On subscribes to a message type
func (c *Client) On(msgType MessageType, callback func(interface{})) func() {
	c.listenersMu.Lock()
	c.listeners[msgType] = append(c.listeners[msgType], callback)
	c.listenersMu.Unlock()

	// Return unsubscribe function
	return func() {
		c.listenersMu.Lock()
		defer c.listenersMu.Unlock()

		listeners := c.listeners[msgType]
		for i, cb := range listeners {
			if &cb == &callback {
				c.listeners[msgType] = append(listeners[:i], listeners[i+1:]...)
				break
			}
		}
	}
}

// Send sends a message to the server
func (c *Client) Send(msgType MessageType, payload interface{}) error {
	c.mu.RLock()
	conn := c.conn
	c.mu.RUnlock()

	if conn == nil {
		return fmt.Errorf("not connected")
	}

	msg := WebSocketMessage{
		Type:    msgType,
		Payload: payload,
	}

	data, err := json.Marshal(msg)
	if err != nil {
		return err
	}

	if err := conn.WriteMessage(websocket.TextMessage, data); err != nil {
		return err
	}

	c.recordMessageSent()
	return nil
}

// GetStats returns connection statistics
func (c *Client) GetStats() ConnectionStats {
	c.statsLock.RLock()
	defer c.statsLock.RUnlock()
	return c.stats
}

// Private methods

func (c *Client) dial() (*websocket.Conn, error) {
	scheme := "ws"
	if c.config.UseTLS {
		scheme = "wss"
	}

	u := url.URL{
		Scheme: scheme,
		Host:   fmt.Sprintf("%s:%d", c.config.Host, c.config.Port),
		Path:   c.config.Path,
	}

	// Add authentication token
	if c.token != "" {
		q := u.Query()
		q.Set("token", c.token)
		u.RawQuery = q.Encode()
	}

	timeout := time.Duration(c.config.ConnectTimeoutMs) * time.Millisecond
	conn, _, err := websocket.DefaultDialer.DialContext(c.ctx, u.String(), nil)
	if err != nil {
		// Add timeout manually if context doesn't have one
		dialCtx, cancel := context.WithTimeout(c.ctx, timeout)
		defer cancel()
		conn, _, err = websocket.DefaultDialer.DialContext(dialCtx, u.String(), nil)
	}

	return conn, err
}

func (c *Client) readLoop() {
	defer func() {
		c.handleDisconnect()
	}()

	for {
		select {
		case <-c.ctx.Done():
			return
		default:
		}

		c.mu.RLock()
		conn := c.conn
		c.mu.RUnlock()

		if conn == nil {
			return
		}

		var msg WebSocketMessage
		if err := conn.ReadJSON(&msg); err != nil {
			c.recordError(err.Error())
			logger.Error("WebSocket read error", "error", err)
			return
		}

		c.recordMessageReceived()

		// Emit to listeners
		c.listenersMu.RLock()
		callbacks := c.listeners[msg.Type]
		c.listenersMu.RUnlock()

		for _, callback := range callbacks {
			go callback(msg.Payload)
		}

		// Also emit to generic "message" type listeners
		c.listenersMu.RLock()
		callbacks = c.listeners[""] // Empty type = all messages
		c.listenersMu.RUnlock()

		for _, callback := range callbacks {
			go callback(msg)
		}
	}
}

func (c *Client) heartbeatLoop() {
	ticker := time.NewTicker(time.Duration(c.config.HeartbeatIntervalMs) * time.Millisecond)
	defer ticker.Stop()

	for {
		select {
		case <-c.ctx.Done():
			return
		case <-ticker.C:
			if c.IsConnected() {
				if err := c.Send(MessageTypeHeartbeat, nil); err != nil {
					logger.Debug("Failed to send heartbeat", "error", err)
				}
			}
		}
	}
}

func (c *Client) handleDisconnect() {
	c.mu.Lock()
	if c.conn != nil {
		c.conn.Close()
		c.conn = nil
	}
	c.mu.Unlock()

	c.setState(StateReconnecting)
	c.recordDisconnected()

	// Attempt reconnection with exponential backoff
	for {
		if c.config.MaxReconnectAttempts >= 0 && c.reconnectAttempts >= c.config.MaxReconnectAttempts {
			c.setState(StateError)
			logger.Error("Max reconnection attempts reached")
			return
		}

		// Calculate backoff delay with jitter
		backoff := time.Duration(c.reconnectDelay) * time.Millisecond
		jitter := time.Duration(rand.Intn(1000)) * time.Millisecond
		waitTime := backoff + jitter

		logger.Debug("Reconnecting WebSocket", "attempt", c.reconnectAttempts+1, "wait_ms", waitTime.Milliseconds())

		select {
		case <-c.ctx.Done():
			return
		case <-time.After(waitTime):
		}

		conn, err := c.dial()
		if err != nil {
			c.reconnectAttempts++
			// Exponential backoff: 2x each time, capped at max
			c.reconnectDelay = int(math.Min(
				float64(c.reconnectDelay*2),
				float64(c.config.ReconnectMaxDelayMs),
			))
			continue
		}

		c.mu.Lock()
		c.conn = conn
		c.mu.Unlock()

		c.setState(StateConnected)
		c.reconnectAttempts = 0
		c.reconnectDelay = c.config.ReconnectBaseDelayMs
		c.recordConnected()

		logger.Debug("WebSocket reconnected")

		// Restart read and heartbeat loops
		go c.readLoop()
		go c.heartbeatLoop()
		return
	}
}

func (c *Client) setState(state ConnectionState) {
	c.state.Store(state)
}

func (c *Client) getState() ConnectionState {
	return c.state.Load().(ConnectionState)
}

func (c *Client) recordMessageReceived() {
	c.statsLock.Lock()
	c.stats.MessagesReceived++
	c.statsLock.Unlock()
}

func (c *Client) recordMessageSent() {
	c.statsLock.Lock()
	c.stats.MessagesSent++
	c.statsLock.Unlock()
}

func (c *Client) recordError(errMsg string) {
	c.statsLock.Lock()
	c.stats.LastError = errMsg
	c.statsLock.Unlock()
}

func (c *Client) recordConnected() {
	c.statsLock.Lock()
	c.stats.ConnectedAt = time.Now()
	c.statsLock.Unlock()
}

func (c *Client) recordDisconnected() {
	c.statsLock.Lock()
	c.stats.DisconnectedAt = time.Now()
	c.statsLock.Unlock()
}
