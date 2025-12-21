package handlers

import "github.com/zfogg/sidechain/backend/internal/kernel"

// TODO: - Comprehensive backend test coverage needed
// TODO: -30 - Add tests for all feed endpoints (GetTimeline, GetGlobalFeed, CreatePost, etc.)
// TODO: -7 - Add tests for auth endpoints (RegisterNativeUser, AuthenticateNativeUser, etc.)
// TODO: -9 - Add tests for Stream.io client methods
// TODO: -4 - Add tests for WebSocket/presence functionality
// TODO: -5 - Add tests for S3 storage operations
// TODO: -5 - Add tests for audio processing queue

// Handlers contains all HTTP handlers for the API.
// Uses dependency injection via container for all service dependencies.
type Handlers struct {
	kernel *kernel.Kernel
}

// NewHandlers creates a new handlers instance with dependency injection.
// All service dependencies are accessed through the container.
func NewHandlers(c *kernel.Kernel) *Handlers {
	return &Handlers{
		kernel: c,
	}
}

// Container returns the underlying dependency injection container.
// Used for testing and access to all services.
func (h *Handlers) Kernel() *kernel.Kernel {
	return h.kernel
}
