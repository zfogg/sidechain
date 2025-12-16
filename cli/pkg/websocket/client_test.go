package websocket

import (
	"testing"
)

func TestNewClient(t *testing.T) {
	cfg := DefaultConfig()
	client := NewClient(cfg)

	if client == nil {
		t.Fatal("NewClient returned nil")
	}
	if client.config.Host != cfg.Host {
		t.Errorf("Config host mismatch: got %s, want %s", client.config.Host, cfg.Host)
	}
	if client.getState() != StateDisconnected {
		t.Errorf("Initial state should be StateDisconnected, got %v", client.getState())
	}
	if len(client.listeners) != 0 {
		t.Errorf("Listeners should be empty, got %d", len(client.listeners))
	}
}

func TestDefaultConfig(t *testing.T) {
	cfg := DefaultConfig()

	if cfg.Host != "localhost" || cfg.Port != 8787 || cfg.Path == "" {
		t.Errorf("DefaultConfig has incorrect values: %+v", cfg)
	}
	if cfg.ConnectTimeoutMs != 15000 || cfg.HeartbeatIntervalMs != 30000 {
		t.Errorf("DefaultConfig timeouts incorrect: %+v", cfg)
	}
	if cfg.MaxReconnectAttempts != -1 {
		t.Errorf("MaxReconnectAttempts should be -1 (unlimited), got %d", cfg.MaxReconnectAttempts)
	}
}

func TestProductionConfig(t *testing.T) {
	cfg := ProductionConfig("api.example.com")

	if cfg.Host != "api.example.com" || cfg.Port != 443 {
		t.Errorf("ProductionConfig host/port incorrect: %s:%d", cfg.Host, cfg.Port)
	}
	if !cfg.UseTLS {
		t.Error("ProductionConfig should use TLS")
	}
	if cfg.ReconnectMaxDelayMs != 60000 {
		t.Errorf("ProductionConfig max reconnect delay should be 60000, got %d", cfg.ReconnectMaxDelayMs)
	}
}

func TestSetAuthToken(t *testing.T) {
	client := NewClient(DefaultConfig())
	token := "test-jwt-token-123"

	client.SetAuthToken(token)

	if client.token != token {
		t.Errorf("Token not set correctly: got %s, want %s", client.token, token)
	}
}

func TestIsConnected(t *testing.T) {
	client := NewClient(DefaultConfig())

	if client.IsConnected() {
		t.Error("Newly created client should not be connected")
	}

	client.setState(StateConnected)
	if !client.IsConnected() {
		t.Error("Client should be connected after setState(StateConnected)")
	}
}

func TestGetStats(t *testing.T) {
	client := NewClient(DefaultConfig())

	client.recordMessageSent()
	client.recordMessageSent()
	client.recordMessageReceived()
	client.recordError("test error")

	stats := client.GetStats()

	if stats.MessagesSent != 2 {
		t.Errorf("MessagesSent should be 2, got %d", stats.MessagesSent)
	}
	if stats.MessagesReceived != 1 {
		t.Errorf("MessagesReceived should be 1, got %d", stats.MessagesReceived)
	}
	if stats.LastError != "test error" {
		t.Errorf("LastError mismatch: got %s", stats.LastError)
	}
}

func TestOn_Subscription(t *testing.T) {
	client := NewClient(DefaultConfig())

	// Subscribe to multiple message types
	client.On(MessageTypeNewPost, func(payload interface{}) {})
	client.On(MessageTypePostLiked, func(payload interface{}) {})

	client.listenersMu.RLock()
	newPostCallbacks := len(client.listeners[MessageTypeNewPost])
	likeCallbacks := len(client.listeners[MessageTypePostLiked])
	client.listenersMu.RUnlock()

	if newPostCallbacks != 1 {
		t.Errorf("Should have 1 NewPost callback, got %d", newPostCallbacks)
	}
	if likeCallbacks != 1 {
		t.Errorf("Should have 1 Like callback, got %d", likeCallbacks)
	}
}

func TestOn_MultipleListeners(t *testing.T) {
	client := NewClient(DefaultConfig())

	client.On(MessageTypeNewPost, func(payload interface{}) {})
	client.On(MessageTypeNewPost, func(payload interface{}) {})
	client.On(MessageTypePostLiked, func(payload interface{}) {})

	client.listenersMu.RLock()
	newPostCallbacks := len(client.listeners[MessageTypeNewPost])
	likeCallbacks := len(client.listeners[MessageTypePostLiked])
	client.listenersMu.RUnlock()

	if newPostCallbacks != 2 {
		t.Errorf("NewPost should have 2 callbacks, got %d", newPostCallbacks)
	}
	if likeCallbacks != 1 {
		t.Errorf("PostLiked should have 1 callback, got %d", likeCallbacks)
	}
}

func TestMessageTypes_Constants(t *testing.T) {
	types := []MessageType{
		MessageTypeNewPost,
		MessageTypePostLiked,
		MessageTypeUserFollowed,
		MessageTypePresenceUpdate,
		MessageTypeHeartbeat,
		MessageTypeError,
	}

	for _, msgType := range types {
		if msgType == "" {
			t.Error("MessageType constant is empty")
		}
	}
}

func TestConnectionStates(t *testing.T) {
	client := NewClient(DefaultConfig())

	states := []ConnectionState{
		StateDisconnected,
		StateConnecting,
		StateConnected,
		StateReconnecting,
		StateError,
	}

	for _, state := range states {
		client.setState(state)
		if client.getState() != state {
			t.Errorf("State mismatch: set %v, got %v", state, client.getState())
		}
	}
}

func TestReconnectDelayBackoff(t *testing.T) {
	client := NewClient(DefaultConfig())
	client.reconnectDelay = client.config.ReconnectBaseDelayMs

	// Simulate reconnect attempts
	baseDelay := float64(client.config.ReconnectBaseDelayMs)
	for i := 0; i < 3; i++ {
		client.reconnectDelay = int(min(float64(client.reconnectDelay)*2, float64(client.config.ReconnectMaxDelayMs)))
	}

	expectedDelay := int(min(baseDelay*8, float64(client.config.ReconnectMaxDelayMs)))
	if client.reconnectDelay != expectedDelay {
		t.Errorf("Backoff delay incorrect: got %d, want %d", client.reconnectDelay, expectedDelay)
	}
}

func TestRecordStats(t *testing.T) {
	client := NewClient(DefaultConfig())

	// Test multiple stat records
	for i := 0; i < 5; i++ {
		client.recordMessageSent()
		client.recordMessageReceived()
	}
	client.recordError("timeout error")
	client.recordConnected()
	client.recordDisconnected()

	stats := client.GetStats()
	if stats.MessagesSent != 5 || stats.MessagesReceived != 5 {
		t.Errorf("Stats mismatch: sent=%d, received=%d", stats.MessagesSent, stats.MessagesReceived)
	}
	if stats.LastError != "timeout error" {
		t.Errorf("Error not recorded: got %s", stats.LastError)
	}
	if stats.ConnectedAt.IsZero() || stats.DisconnectedAt.IsZero() {
		t.Error("Connection times not recorded")
	}
}

func TestBuildURI(t *testing.T) {
	tests := []struct {
		name     string
		config   Config
		token    string
		hasTLS   bool
	}{
		{
			name: "development_no_token",
			config: Config{
				Host: "localhost",
				Port: 8787,
				Path: "/api/v1/ws",
				UseTLS: false,
			},
			token: "",
			hasTLS: false,
		},
		{
			name: "production_with_token",
			config: Config{
				Host: "api.example.com",
				Port: 443,
				Path: "/api/v1/ws",
				UseTLS: true,
			},
			token: "jwt-token",
			hasTLS: true,
		},
	}

	for _, tt := range tests {
		client := NewClient(tt.config)
		client.SetAuthToken(tt.token)

		if client.config.UseTLS != tt.hasTLS {
			t.Errorf("%s: TLS config mismatch", tt.name)
		}
	}
}

func min(a, b float64) float64 {
	if a < b {
		return a
	}
	return b
}
