package handlers

import (
	"github.com/zfogg/sidechain/backend/internal/audio"
	"github.com/zfogg/sidechain/backend/internal/recommendations"
	"github.com/zfogg/sidechain/backend/internal/search"
	"github.com/zfogg/sidechain/backend/internal/stream"
	"github.com/zfogg/sidechain/backend/internal/waveform"
	"github.com/zfogg/sidechain/backend/internal/websocket"
)

// TODO: Phase 4.5 - Comprehensive backend test coverage needed
// TODO: Phase 4.5.1.1-30 - Add tests for all feed endpoints (GetTimeline, GetGlobalFeed, CreatePost, etc.)
// TODO: Phase 4.5.2.1-7 - Add tests for auth endpoints (RegisterNativeUser, AuthenticateNativeUser, etc.)
// TODO: Phase 4.5.3.1-9 - Add tests for Stream.io client methods
// TODO: Phase 4.5.4.1-4 - Add tests for WebSocket/presence functionality
// TODO: Phase 4.5.5.1-5 - Add tests for S3 storage operations
// TODO: Phase 4.5.6.1-5 - Add tests for audio processing queue

// Handlers contains all HTTP handlers for the API
type Handlers struct {
	stream            stream.StreamClientInterface
	audioProcessor    *audio.Processor
	wsHandler         *websocket.Handler
	gorse             *recommendations.GorseRESTClient
	search            *search.Client
	waveformGenerator *waveform.Generator
	waveformStorage   *waveform.Storage
}

// NewHandlers creates a new handlers instance
func NewHandlers(streamClient stream.StreamClientInterface, audioProcessor *audio.Processor) *Handlers {
	return &Handlers{
		stream:         streamClient,
		audioProcessor: audioProcessor,
	}
}

// SetWebSocketHandler sets the WebSocket handler for real-time notifications
func (h *Handlers) SetWebSocketHandler(ws *websocket.Handler) {
	h.wsHandler = ws
}

// SetGorseClient sets the Gorse recommendation client
func (h *Handlers) SetGorseClient(gorse *recommendations.GorseRESTClient) {
	h.gorse = gorse
}

// SetSearchClient sets the Elasticsearch search client
func (h *Handlers) SetSearchClient(searchClient *search.Client) {
	h.search = searchClient
}

// SetWaveformTools sets the waveform generator and storage
func (h *Handlers) SetWaveformTools(generator *waveform.Generator, storage *waveform.Storage) {
	h.waveformGenerator = generator
	h.waveformStorage = storage
}
