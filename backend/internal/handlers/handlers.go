package handlers

import (
	"github.com/zfogg/sidechain/backend/internal/audio"
	"github.com/zfogg/sidechain/backend/internal/stream"
	"github.com/zfogg/sidechain/backend/internal/websocket"
)

// Handlers contains all HTTP handlers for the API
type Handlers struct {
	stream         *stream.Client
	audioProcessor *audio.Processor
	wsHandler      *websocket.Handler
}

// NewHandlers creates a new handlers instance
func NewHandlers(streamClient *stream.Client, audioProcessor *audio.Processor) *Handlers {
	return &Handlers{
		stream:         streamClient,
		audioProcessor: audioProcessor,
	}
}

// SetWebSocketHandler sets the WebSocket handler for real-time notifications
func (h *Handlers) SetWebSocketHandler(ws *websocket.Handler) {
	h.wsHandler = ws
}
