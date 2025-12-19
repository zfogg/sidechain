// Package backend provides the Sidechain API server.

// This package contains the main application entry point. The actual API
// documentation is organized into subpackages:

// - internal/handlers: HTTP request handlers for all API endpoints
// - internal/models: Data models and database schemas
// - internal/auth: Authentication and authorization services
// - internal/stream: Stream.io integration for social features
// - internal/websocket: WebSocket server for real-time updates
// - internal/audio: Audio processing and transcoding
// - internal/storage: File storage (S3) operations
// - internal/database: Database connection and migrations
// - internal/email: Email service integration
// - internal/middleware: HTTP middleware (rate limiting, etc.)
// - internal/queue: Background job processing
// - internal/recommendations: Recommendation engine integration
// - internal/search: Search functionality
// - internal/storage: Storage interface implementations
// - internal/stories: Story management

// See the individual package documentation for detailed API reference.
package main
