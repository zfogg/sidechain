package websocket

import (
	"sync"
)

var (
	instance *Client
	mu       sync.Once
)

// GetClient returns the singleton WebSocket client instance
func GetClient(config ...Config) *Client {
	mu.Do(func() {
		var cfg Config
		if len(config) > 0 {
			cfg = config[0]
		} else {
			cfg = DefaultConfig()
		}
		instance = NewClient(cfg)
	})
	return instance
}
