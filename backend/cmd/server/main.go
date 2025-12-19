package main

import (
	"context"
	"log"
	"net/http"
	"os"
	"os/signal"
	"strings"
	"syscall"
	"time"

	"github.com/gin-contrib/cors"
	"github.com/gin-contrib/gzip"
	"github.com/gin-gonic/gin"
	"github.com/prometheus/client_golang/prometheus/promhttp"
	"github.com/joho/godotenv"
	"go.uber.org/zap"
	"github.com/zfogg/sidechain/backend/internal/alerts"
	"github.com/zfogg/sidechain/backend/internal/audio"
	"github.com/zfogg/sidechain/backend/internal/auth"
	"github.com/zfogg/sidechain/backend/internal/cache"
	"github.com/zfogg/sidechain/backend/internal/challenges"
	"github.com/zfogg/sidechain/backend/internal/config"
	"github.com/zfogg/sidechain/backend/internal/database"
	"github.com/zfogg/sidechain/backend/internal/email"
	"github.com/zfogg/sidechain/backend/internal/handlers"
	"github.com/zfogg/sidechain/backend/internal/logger"
	"github.com/zfogg/sidechain/backend/internal/metrics"
	"github.com/zfogg/sidechain/backend/internal/middleware"
	"github.com/zfogg/sidechain/backend/internal/models" // Used for NotificationPreferencesChecker
	"github.com/zfogg/sidechain/backend/internal/recommendations"
	"github.com/zfogg/sidechain/backend/internal/search"
	"github.com/zfogg/sidechain/backend/internal/seed"
	"github.com/zfogg/sidechain/backend/internal/storage"
	"github.com/zfogg/sidechain/backend/internal/stories"
	"github.com/zfogg/sidechain/backend/internal/stream"
	"github.com/zfogg/sidechain/backend/internal/waveform"
	"github.com/zfogg/sidechain/backend/internal/websocket"
)

func main() {
	// Initialize structured logging (before everything else)
	logLevel := os.Getenv("LOG_LEVEL")
	if logLevel == "" {
		logLevel = "info"
	}
	logFile := os.Getenv("LOG_FILE")
	if logFile == "" {
		logFile = "server.log"
	}

	if err := logger.Initialize(logLevel, logFile); err != nil {
		log.Fatalf("Failed to initialize logger: %v", err)
	}
	defer logger.Close()

	logger.Log.Info("=== Sidechain server starting ===")

	// Load environment variables
	if err := godotenv.Load(); err != nil {
		logger.Log.Warn("Warning: .env file not found, using system environment variables")
	}

	// Initialize Redis cache (optional but recommended)
	redisHost := os.Getenv("REDIS_HOST")
	redisPort := os.Getenv("REDIS_PORT")
	redisPassword := os.Getenv("REDIS_PASSWORD")

	var redisClient *cache.RedisClient
	var err error
	if redisHost != "" || redisPort != "" {
		// User explicitly configured Redis
		if redisHost == "" {
			redisHost = "localhost"
		}
		if redisPort == "" {
			redisPort = "6379"
		}

		redisClient, err = cache.NewRedisClient(redisHost, redisPort, redisPassword)
		if err != nil {
			logger.Log.Warn("Failed to connect to Redis, distributed caching will be disabled",
				zap.Error(err),
			)
			redisClient = nil
		}
		defer func() {
			if redisClient != nil {
				_ = redisClient.Close()
			}
		}()
	} else {
		logger.Log.Info("Redis not configured (REDIS_HOST not set), distributed caching disabled")
	}

	// Initialize database
	if err := database.Initialize(); err != nil {
		logger.FatalWithFields("Failed to initialize database", err)
	}

	// Run migrations
	if err := database.Migrate(); err != nil {
		logger.FatalWithFields("Failed to run migrations", err)
	}

	// Auto-seed development data if in development mode and database is empty
	if os.Getenv("ENVIRONMENT") == "development" || os.Getenv("ENVIRONMENT") == "" {
		var userCount int64
		if err := database.DB.Model(&models.User{}).Count(&userCount).Error; err != nil {
			logger.WarnWithFields("Failed to count users, skipping auto-seed to prevent data corruption", err)
		} else if userCount == 0 {
			logger.Log.Info("🌱 Development mode: Database is empty, auto-seeding development data...")
			seeder := seed.NewSeeder(database.DB)
			if err := seeder.SeedDev(); err != nil {
				logger.WarnWithFields("Auto-seed failed (non-fatal), use: go run cmd/seed/main.go dev", err)
			} else {
				logger.Log.Info("✅ Development data seeded successfully!")
			}
		} else {
			logger.Log.Info("Database already populated, skipping auto-seed",
				zap.Int64("user_count", userCount),
			)
		}
	}

	// Initialize Elasticsearch search client
	baseSearchClient, err := search.NewClient()

	if err != nil {
		logger.WarnWithFields("Failed to initialize Elasticsearch, using PostgreSQL fallback", err)
	} else {
		// Create indices on startup
		if err := baseSearchClient.InitializeIndices(context.Background()); err != nil {
			logger.WarnWithFields("Failed to initialize Elasticsearch indices, search may be limited", err)
		} else {
			logger.Log.Info("✅ Elasticsearch indices initialized successfully")
		}
	}


	// Initialize alert system (Phase 7.2)
	alertManager := alerts.NewAlertManager()
	alertEvaluator := alerts.NewEvaluator(alertManager)
	alertEvaluator.InitializeDefaultRules()
	logger.Log.Info("✅ Alert system initialized with default rules")

	// Start alert evaluation loop (check every minute)
	stopEvaluation := alertEvaluator.StartEvaluationLoop(1 * time.Minute)
	defer close(stopEvaluation)

	// Set global references for handlers
	handlers.SetAlertManager(alertManager)
	handlers.SetAlertEvaluator(alertEvaluator)

	// Initialize Stream.io client
	streamClient, err := stream.NewClient()
	if err != nil {
		logger.FatalWithFields("Failed to initialize Stream.io client", err)
	}

	// Initialize notification preferences checker and set on stream client
	notifPrefsChecker := models.NewNotificationPreferencesChecker(database.DB)
	streamClient.SetNotificationPreferencesChecker(notifPrefsChecker)
	logger.Log.Info("✅ Notification preferences checker initialized")

	// Initialize auth service with OAuth configuration
	jwtSecret := []byte(os.Getenv("JWT_SECRET"))
	if len(jwtSecret) == 0 {
		logger.FatalWithFields("JWT_SECRET environment variable is required", nil)
	}

	// Load OAuth configuration from environment variables
	// REQUIRED: OAUTH_REDIRECT_URL, GOOGLE_CLIENT_ID, GOOGLE_CLIENT_SECRET, DISCORD_CLIENT_ID, DISCORD_CLIENT_SECRET
	oauthConfig, err := config.LoadOAuthConfig()
	if err != nil {
		logger.FatalWithFields("Failed to load OAuth configuration", err)
	}

	authService := auth.NewService(
		jwtSecret,
		streamClient,
		&auth.OAuthConfigData{
			GoogleConfig:  oauthConfig.GoogleConfig,
			DiscordConfig: oauthConfig.DiscordConfig,
		},
	)

	// Initialize S3 uploader
	s3Uploader, err := storage.NewS3Uploader(
		os.Getenv("AWS_REGION"),
		os.Getenv("AWS_BUCKET"),
		os.Getenv("CDN_BASE_URL"),
	)
	if err != nil {
		logger.WarnWithFields("Failed to initialize S3 uploader, audio uploads will fail", err)
		s3Uploader = nil
	}

	// Initialize email service (AWS SES)
	var emailService *email.EmailService
	sesFromEmail := os.Getenv("SES_FROM_EMAIL")
	sesFromName := os.Getenv("SES_FROM_NAME")
	baseURL := os.Getenv("BASE_URL")

	if sesFromEmail != "" && baseURL != "" {
		emailService, err = email.NewEmailService(
			os.Getenv("AWS_REGION"),
			sesFromEmail,
			sesFromName,
			baseURL,
		)
		if err != nil {
			logger.WarnWithFields("Failed to initialize email service, password reset emails will not be sent", err)
			emailService = nil
		} else {
			logger.Log.Info("✅ Email service (AWS SES) initialized successfully",
				zap.String("from_email", sesFromEmail),
			)
		}
	} else {
		logger.Log.Warn("Email service not configured: set SES_FROM_EMAIL, SES_FROM_NAME, and BASE_URL in .env")
	}

	// Check S3 access (skip for development)
	if s3Uploader != nil {
		if err := s3Uploader.CheckBucketAccess(context.Background()); err != nil {
			logger.WarnWithFields("S3 bucket access failed, audio uploads will fail", err)
		}
	}

	// Check FFmpeg availability
	if err := audio.CheckFFmpegAvailable(); err != nil {
		logger.WarnWithFields("FFmpeg not available, audio processing will be limited", err)
	}

	// Initialize audio processor and start the background queue
	audioProcessor := audio.NewProcessor(s3Uploader)
	audioProcessor.Start()
	defer audioProcessor.Stop()

	// Initialize WebSocket hub and handler
	wsHub := websocket.NewHub()
	wsHandler := websocket.NewHandler(wsHub, jwtSecret)
	wsHandler.RegisterDefaultHandlers()

	// Initialize presence manager
	presenceManager := websocket.NewPresenceManager(wsHub, streamClient, websocket.DefaultPresenceConfig())
	wsHandler.SetPresenceManager(presenceManager)
	presenceManager.Start()
	defer presenceManager.Stop()

	// Initialize story cleanup service (runs every hour)
	storyCleanup := stories.NewCleanupService(s3Uploader, 1*time.Hour)
	storyCleanup.Start()
	defer storyCleanup.Stop()

	// Initialize MIDI challenge notification service (runs every 15 minutes) (R.2.2.4.4)
	challengeNotifications := challenges.NewNotificationService(streamClient, 15*time.Minute)
	challengeNotifications.Start()
	defer challengeNotifications.Stop()

	// Start WebSocket hub in background
	go wsHub.Run()

	// Initialize Gorse recommendation client for user recommendations
	gorseURL := os.Getenv("GORSE_URL")
	if gorseURL == "" {
		gorseURL = "http://localhost:8087"
	}
	gorseAPIKey := os.Getenv("GORSE_API_KEY")
	if gorseAPIKey == "" {
		gorseAPIKey = "sidechain_gorse_api_key"
	}
	gorseClient := recommendations.NewGorseRESTClient(gorseURL, gorseAPIKey, database.DB)
	logger.Log.Info("Gorse recommendation client initialized",
		zap.String("url", gorseURL),
	)

	// Set up post completion callback to sync to Gorse (Task 2.1)
	audioProcessor.SetPostCompleteCallback(func(postID string) {
		logger.Log.Debug("Post completed processing, syncing to Gorse",
			logger.WithPostID(postID),
		)
		if err := gorseClient.SyncItem(postID); err != nil {
			logger.Log.Warn("Failed to sync post to Gorse",
				zap.Error(err),
				logger.WithPostID(postID),
			)
		} else {
			logger.Log.Debug("Successfully synced post to Gorse",
				logger.WithPostID(postID),
			)
		}
	})

	// Configure batch sync interval (Task 3.4)
	syncIntervalStr := os.Getenv("GORSE_SYNC_INTERVAL")
	syncInterval := 1 * time.Hour // Default: 1 hour
	if syncIntervalStr != "" {
		if parsed, err := time.ParseDuration(syncIntervalStr); err == nil {
			syncInterval = parsed
		} else {
			logger.Log.Warn("Invalid GORSE_SYNC_INTERVAL, using default",
				zap.String("value", syncIntervalStr),
				zap.Duration("default", syncInterval),
			)
		}
	}

	// Start background batch sync (Task 3.1, 3.2, 3.3)
	syncCtx, syncCancel := context.WithCancel(context.Background())
	defer syncCancel()

	// Initial sync on startup (Task 3.3)
	go func() {
		logger.Log.Info("🔄 Starting initial Gorse batch sync...")
		if err := gorseClient.BatchSyncUsers(); err != nil {
			logger.WarnWithFields("Initial user sync failed", err)
		} else {
			logger.Log.Info("✅ Initial user sync completed")
		}

		if err := gorseClient.BatchSyncItems(); err != nil {
			logger.WarnWithFields("Initial item sync failed", err)
		} else {
			logger.Log.Info("✅ Initial item sync completed")
		}

		if err := gorseClient.BatchSyncUserItems(); err != nil {
			logger.WarnWithFields("Initial user-items sync failed", err)
		} else {
			logger.Log.Info("✅ Initial user-items sync completed")
		}

		if err := gorseClient.BatchSyncFeedback(); err != nil {
			logger.WarnWithFields("Initial feedback sync failed", err)
		} else {
			logger.Log.Info("✅ Initial feedback sync completed")
		}

		logger.Log.Info("✅ Initial Gorse batch sync completed")
	}()

	// Periodic batch sync (Task 3.1)
	go func() {
		ticker := time.NewTicker(syncInterval)
		defer ticker.Stop()

		logger.Log.Info("🔄 Gorse batch sync scheduled",
			zap.Duration("interval", syncInterval),
		)

		for {
			select {
			case <-ticker.C:
				logger.Log.Debug("🔄 Starting scheduled Gorse batch sync...")

				if err := gorseClient.BatchSyncUsers(); err != nil {
					logger.WarnWithFields("Batch user sync failed", err)
				}

				if err := gorseClient.BatchSyncItems(); err != nil {
					logger.WarnWithFields("Batch item sync failed", err)
				}

				if err := gorseClient.BatchSyncUserItems(); err != nil {
					logger.WarnWithFields("Batch user-items sync failed", err)
				}

				if err := gorseClient.BatchSyncFeedback(); err != nil {
					logger.WarnWithFields("Batch feedback sync failed", err)
				}

				logger.Log.Debug("✅ Scheduled Gorse batch sync completed")

			case <-syncCtx.Done():
				logger.Log.Info("🛑 Gorse batch sync stopped")
				return
			}
		}
	}()

	// Daily CTR metrics logging (Task 8.3)
	go func() {
		ticker := time.NewTicker(24 * time.Hour)
		defer ticker.Stop()

		// Log CTR metrics immediately on startup
		logger.Log.Debug("📊 Logging initial CTR metrics...")
		if err := recommendations.LogCTRMetrics(database.DB); err != nil {
			logger.WarnWithFields("Failed to log initial CTR metrics", err)
		}

		for {
			select {
			case <-ticker.C:
				logger.Log.Debug("📊 Logging daily CTR metrics...")
				if err := recommendations.LogCTRMetrics(database.DB); err != nil {
					logger.WarnWithFields("Failed to log daily CTR metrics", err)
				}
			case <-syncCtx.Done():
				logger.Log.Info("🛑 CTR metrics logging stopped")
				return
			}
		}
	}()

	// Initialize handlers
	h := handlers.NewHandlers(streamClient, audioProcessor)
	h.SetWebSocketHandler(wsHandler) // Enable real-time follow notifications
	h.SetGorseClient(gorseClient)    // Enable user follow recommendations
	if baseSearchClient != nil {
		h.SetSearchClient(baseSearchClient) // Enable search functionality
	}

	// Initialize waveform generation tools for audio visualization
	waveformGenerator := waveform.NewGenerator()
	waveformStorage, err := waveform.NewStorage(
		os.Getenv("AWS_REGION"),
		os.Getenv("AWS_BUCKET"),
		os.Getenv("CDN_BASE_URL"),
	)
	if err != nil {
		logger.WarnWithFields("Failed to initialize waveform storage, waveforms will not be generated", err)
		waveformStorage = nil
	} else {
		h.SetWaveformTools(waveformGenerator, waveformStorage)
		logger.Log.Info("✅ Waveform tools initialized successfully")
	}

	authHandlers := handlers.NewAuthHandlers(authService, s3Uploader, streamClient)
	authHandlers.SetJWTSecret(jwtSecret)
	authHandlers.SetEmailService(emailService) // Enable password reset emails
	if baseSearchClient != nil {
		authHandlers.SetSearchClient(baseSearchClient) // Enable search functionality
	}

	// Initialize Prometheus metrics
	metrics.Initialize()
	logger.Log.Info("Prometheus metrics initialized")

	// Setup Gin router - use gin.New() instead of gin.Default() to control middleware
	r := gin.New()

	// NOTE: WebSocket routes (/api/v1/ws, /api/v1/ws/connect) are handled OUTSIDE of Gin
	// by a custom http.Handler wrapper that bypasses Gin's ResponseWriter.
	// This is necessary because Gin's ResponseWriter wrapper interferes with WebSocket
	// connection hijacking. See the http.Server Handler setup below.

	// Configure CORS middleware FIRST (before all other middleware)
	// This ensures OPTIONS preflight requests are handled correctly
	corsConfig := cors.DefaultConfig()

	// Get allowed origins from environment or use sensible defaults
	allowedOrigins := os.Getenv("ALLOWED_ORIGINS")
	if allowedOrigins != "" {
		// Parse comma-separated origins
		corsConfig.AllowOrigins = strings.FieldsFunc(allowedOrigins, func(r rune) bool { return r == ',' })
		// Trim whitespace from each origin
		for i, origin := range corsConfig.AllowOrigins {
			corsConfig.AllowOrigins[i] = strings.TrimSpace(origin)
		}
	} else {
		// Default: allow frontend and localhost for development
		if os.Getenv("ENVIRONMENT") == "development" || os.Getenv("ENVIRONMENT") == "" {
			corsConfig.AllowOrigins = []string{
				"http://localhost:3000",
				"http://localhost:5173", // Vite dev server
				"https://www.sidechain.live",
				"https://sidechain.live",
			}
		} else {
			// Production: only allow the frontend
			corsConfig.AllowOrigins = []string{
				"https://www.sidechain.live",
				"https://sidechain.live",
			}
		}
	}

	corsConfig.AllowMethods = []string{"GET", "POST", "PUT", "DELETE", "OPTIONS", "PATCH"}
	corsConfig.AllowHeaders = []string{"Origin", "Content-Length", "Content-Type", "Authorization", "X-Requested-With", "Accept"}
	corsConfig.AllowCredentials = true
	corsConfig.MaxAge = 86400 // 24 hours
	r.Use(cors.New(corsConfig))

	logger.Log.Info("✅ CORS configured for origins",
		zap.Strings("allowed_origins", corsConfig.AllowOrigins),
	)

	// Standard Gin middleware (after CORS)
	r.Use(middleware.RequestIDMiddleware())   // Add request ID tracking
	r.Use(middleware.MetricsMiddleware())     // Prometheus metrics collection
	r.Use(middleware.GinLoggerMiddleware())   // Structured logging
	r.Use(gin.Recovery())

	// Gzip compression middleware (compress responses > 1KB)
	r.Use(gzip.Gzip(gzip.DefaultCompression, gzip.WithExcludedPaths([]string{
		"/api/v1/ws",         // Don't compress WebSocket connections
		"/api/v1/ws/connect", // Don't compress WebSocket connections
		"/metrics",           // Don't compress Prometheus metrics (binary format issue)
	})))

	// Health check endpoint
	r.GET("/health", func(c *gin.Context) {
		c.JSON(http.StatusOK, gin.H{
			"status":    "ok",
			"timestamp": time.Now().UTC(),
			"service":   "sidechain-backend",
		})
	})

	// Prometheus metrics endpoint
	r.GET("/metrics", gin.WrapH(promhttp.Handler()))

	// API routes
	api := r.Group("/api/v1")
	api.Use(middleware.RateLimit()) // Global rate limit: 100 req/min per IP
	api.Use(middleware.AdminImpersonationMiddleware()) // Admin user impersonation support
	{
		// Authentication routes (public) - stricter rate limit
		authGroup := api.Group("/auth")
		authGroup.Use(middleware.RateLimitAuth()) // Auth rate limit: 10 req/min per IP
		{
			// Native auth
			authGroup.POST("/register", authHandlers.Register)
			authGroup.POST("/login", authHandlers.Login)

			// Password reset (public)
			authGroup.POST("/reset-password", authHandlers.RequestPasswordReset)
			authGroup.POST("/reset-password/confirm", authHandlers.ResetPassword)

			// OAuth flows
			authGroup.GET("/google", authHandlers.GoogleOAuth)
			authGroup.GET("/google/callback", authHandlers.GoogleCallback)
			authGroup.GET("/discord", authHandlers.DiscordOAuth)
			authGroup.GET("/discord/callback", authHandlers.DiscordCallback)

			// OAuth polling endpoint for plugin flow
			authGroup.GET("/oauth/poll", authHandlers.OAuthPoll)

			// User info (protected)
			authGroup.GET("/me", authHandlers.AuthMiddleware(), authHandlers.Me)

			// getstream.io Chat token generation (protected)
			authGroup.GET("/stream-token", authHandlers.AuthMiddleware(), authHandlers.GetStreamToken)

			// Sync user to Stream Chat (for DM creation) (protected)
			authGroup.POST("/sync-user/:user_id", authHandlers.AuthMiddleware(), authHandlers.SyncUserToStream)

			// Two-Factor Authentication
			twoFactor := authGroup.Group("/2fa")
			{
				// Public: Complete 2FA login (after initial login returns requires_2fa)
				twoFactor.POST("/login", authHandlers.Verify2FALogin)

				// Protected: 2FA management endpoints
				twoFactor.GET("/status", authHandlers.AuthMiddleware(), h.Get2FAStatus)
				twoFactor.POST("/enable", authHandlers.AuthMiddleware(), h.Enable2FA)
				twoFactor.POST("/verify", authHandlers.AuthMiddleware(), h.Verify2FA)
				twoFactor.POST("/disable", authHandlers.AuthMiddleware(), h.Disable2FA)
				twoFactor.POST("/backup-codes", authHandlers.AuthMiddleware(), h.RegenerateBackupCodes)
			}
		}

		// Audio routes - with upload rate limiting
		audioGroup := api.Group("/audio")
		{
			audioGroup.Use(authHandlers.AuthMiddleware())
			audioGroup.POST("/upload", middleware.RateLimitUpload(), h.UploadAudio)
			audioGroup.GET("/status/:job_id", h.GetAudioProcessingStatus)
			audioGroup.GET("/:id", h.GetAudio)
		}

		// Feed routes
		feed := api.Group("/feed")
		{
			feed.Use(authHandlers.AuthMiddleware())
			feed.GET("/timeline", middleware.ResponseCacheMiddleware(30*time.Second), h.GetTimeline)
			feed.GET("/timeline/enriched", middleware.ResponseCacheMiddleware(30*time.Second), h.GetEnrichedTimeline)
			feed.GET("/timeline/aggregated", middleware.ResponseCacheMiddleware(30*time.Second), h.GetAggregatedTimeline)
			feed.GET("/unified", middleware.ResponseCacheMiddleware(30*time.Second), h.GetUnifiedTimeline) // Main feed with Gorse recommendations
			feed.GET("/global", middleware.ResponseCacheMiddleware(60*time.Second), h.GetGlobalFeed)
			feed.GET("/global/enriched", middleware.ResponseCacheMiddleware(60*time.Second), h.GetEnrichedGlobalFeed)
			feed.GET("/trending", middleware.ResponseCacheMiddleware(5*time.Minute), h.GetTrendingFeed)
			feed.GET("/trending/grouped", middleware.ResponseCacheMiddleware(5*time.Minute), h.GetTrendingFeedGrouped)
			feed.POST("/post", middleware.CacheInvalidationMiddleware("response:/api/v1/feed/*"), h.CreatePost)
		}

		// Notification routes
		notifications := api.Group("/notifications")
		{
			notifications.Use(authHandlers.AuthMiddleware())
			notifications.GET("", h.GetNotifications)
			notifications.GET("/counts", h.GetNotificationCounts)
			notifications.POST("/read", h.MarkNotificationsRead)
			notifications.POST("/seen", h.MarkNotificationsSeen)
			// Notification preferences (TODO #17)
			notifications.GET("/preferences", h.GetNotificationPreferences)
			notifications.PUT("/preferences", h.UpdateNotificationPreferences)
		}

		// Social routes
		social := api.Group("/social")
		{
			social.Use(authHandlers.AuthMiddleware())
			social.POST("/follow", h.FollowUser)
			social.POST("/unfollow", h.UnfollowUser)
			social.POST("/like", h.LikePost)
			social.DELETE("/like", h.UnlikePost)
			social.POST("/react", h.EmojiReact)
		}

		// Public profile picture URL endpoint (no auth required)
		// Returns the profile picture URL for JUCE to download directly via HTTPS
		api.GET("/users/:id/profile-picture", authHandlers.GetProfilePictureURL)

		// Public user profile endpoints (no auth required)
		api.GET("/users/:id/profile", h.GetUserProfile)
		api.GET("/users/:id/posts", h.GetUserPosts)
		api.GET("/users/:id/followers", h.GetUserFollowers)
		api.GET("/users/:id/following", h.GetUserFollowing)
		api.GET("/users/:id/reposts", h.GetUserReposts)
		api.GET("/users/:id/pinned", h.GetPinnedPosts)
		api.GET("/users/:id/muted", h.IsUserMuted)

		// Authenticated user routes
		users := api.Group("/users")
		{
			users.Use(authHandlers.AuthMiddleware())
			users.GET("/me", h.GetMyProfile)
			users.PUT("/me", h.UpdateMyProfile)
			users.PUT("/username", h.ChangeUsername)
			users.POST("/upload-profile-picture", middleware.RateLimitUpload(), authHandlers.UploadProfilePicture)

			// Recommended users to follow (collaborative filtering via Gorse)
			users.GET("/recommended", h.GetUsersToFollow)

			// Follow request endpoints (for private accounts)
			users.GET("/me/follow-requests", h.GetFollowRequests)
			users.GET("/me/follow-requests/count", h.GetFollowRequestCount)
			users.GET("/me/pending-requests", h.GetPendingFollowRequests)

			// Current user's saved posts (P0 Social Feature) - must be before /:id routes
			users.GET("/me/saved", h.GetSavedPosts)
			// Current user's archived posts - must be before /:id routes
			users.GET("/me/archived", h.GetArchivedPosts)
			// Current user's muted users - must be before /:id routes
			users.GET("/me/muted", h.GetMutedUsers)

			// User profile interaction endpoints (require auth)
			users.GET("/:id/activity", h.GetUserActivitySummary)
			users.GET("/:id/similar", h.GetSimilarUsers)
			users.POST("/:id/follow", h.FollowUserByID)
			users.DELETE("/:id/follow", h.UnfollowUserByID)
			users.GET("/:id/follow-request-status", h.CheckFollowRequestStatus)
			// Mute endpoints
			users.POST("/:id/mute", h.MuteUser)
			users.DELETE("/:id/mute", h.UnmuteUser)
		}

		// Settings routes
		settings := api.Group("/settings")
		{
			settings.Use(authHandlers.AuthMiddleware())
			settings.GET("/activity-status", h.GetActivityStatusSettings)
			settings.PUT("/activity-status", h.UpdateActivityStatusSettings)
		}

		// Follow request management routes
		followRequests := api.Group("/follow-requests")
		{
			followRequests.Use(authHandlers.AuthMiddleware())
			followRequests.POST("/:id/accept", h.AcceptFollowRequest)
			followRequests.POST("/:id/reject", h.RejectFollowRequest)
			followRequests.DELETE("/:id", h.CancelFollowRequest)
		}

		// Search routes (public, optional auth for privacy filtering - Phase 5.1)
		search := api.Group("/search")
		{
			search.Use(authHandlers.AuthMiddlewareOptional())
			search.Use(middleware.RateLimitSearch()) // Phase 6.3: Rate limit search requests
			search.GET("/users", h.SearchUsers)
			search.GET("/posts", h.SearchPosts)       // Phase 1.2: Search posts with Elasticsearch
			search.GET("/stories", h.SearchStories)   // Phase 1.3: Search stories by creator username
			search.GET("/advanced", h.AdvancedSearch) // Phase 1.6: Unified search across all types
			search.GET("/genres", h.GetAvailableGenres)
			search.GET("/keys", h.GetAvailableKeys)

			// Autocomplete endpoints (Phase 1.4, 1.5)
			autocomplete := search.Group("/autocomplete")
			{
				autocomplete.GET("/users", h.AutocompleteUsers)   // Phase 1.4: Username suggestions
				autocomplete.GET("/genres", h.AutocompleteGenres) // Phase 1.5: Genre suggestions
			}
		}


	// Metrics routes (Phase 7.1)

	// Alert routes (Phase 7.2)
	alert := api.Group("/alerts")
	{
		alert.GET("", h.GetAlerts)                  // Get all alerts
		alert.GET("/active", h.GetActiveAlerts)      // Get active alerts
		alert.GET("/type/:type", h.GetAlertsByType) // Get alerts by type
		alert.PUT("/:id/resolve", h.ResolveAlert)   // Resolve an alert
		alert.GET("/stats", h.GetAlertStats)        // Get alert statistics

		// Alert rules management
		rules := alert.Group("/rules")
		{
			rules.GET("", h.GetRules)        // Get all rules
			rules.POST("", h.CreateRule)     // Create new rule
			rules.PUT("/:id", h.UpdateRule)  // Update rule
		}
	}
	metrics := api.Group("/metrics")
	{
		metrics.GET("", h.GetAllMetrics)            // Get all metrics
		metrics.GET("/search", h.GetSearchMetrics)  // Get search-specific metrics
		metrics.POST("/reset", h.ResetMetrics)      // Reset metrics (admin only)
	}
		// Activity routes - Timeline from followed users and global activity
		activity := api.Group("/activity")
		{
			activity.Use(authHandlers.AuthMiddleware())
			activity.GET("/following", h.GetFollowingActivityTimeline)
			activity.GET("/global", h.GetGlobalActivityTimeline)
		}

		// Discovery routes
		discover := api.Group("/discover")
		{
			discover.Use(authHandlers.AuthMiddleware())
			discover.GET("/trending", h.GetTrendingUsers)
			discover.GET("/featured", h.GetFeaturedProducers)
			discover.GET("/suggested", h.GetSuggestedUsers)
			discover.GET("/genres", h.GetAvailableGenres)
			discover.GET("/genre/:genre", h.GetUsersByGenre)
		}

		// Recommendation routes
		recommendations := api.Group("/recommendations")
		{
			recommendations.Use(authHandlers.AuthMiddleware())
			recommendations.GET("/for-you", h.GetForYouFeed)
			recommendations.GET("/similar-posts/:post_id", h.GetSimilarPosts)
			recommendations.GET("/similar-users/:user_id", h.GetRecommendedUsers)
			// Negative feedback endpoints (Task 5)
			recommendations.POST("/dislike/:post_id", h.NotInterestedInPost) // "Not interested" button
			recommendations.POST("/skip/:post_id", h.SkipPost)               // Track when user scrolls past quickly
			recommendations.POST("/hide/:post_id", h.HidePost)               // "Hide this post" button
			// Discovery endpoints (Task 7)
			recommendations.GET("/popular", h.GetPopular)              // Globally popular posts
			recommendations.GET("/latest", h.GetLatest)                // Recently added posts
			recommendations.GET("/discovery-feed", h.GetDiscoveryFeed) // Blended discovery feed (Task 7.4)
			// CTR tracking (Task 8)
			recommendations.POST("/click", h.TrackRecommendationClick) // Track recommendation clicks (Task 8.2)
			recommendations.GET("/metrics/ctr", h.GetCTRMetrics)       // Get CTR metrics (Task 8.3)
		}

		// Post routes (for comments, deletion, reporting, saving, reposting)
		posts := api.Group("/posts")
		{
			posts.Use(authHandlers.AuthMiddleware())
			posts.POST("/:id/comments", h.CreateComment)
			posts.GET("/:id/comments", h.GetComments)
			posts.DELETE("/:id", h.DeletePost)
			posts.POST("/:id/report", h.ReportPost)
			posts.POST("/:id/download", h.DownloadPost)
			posts.POST("/:id/play", h.TrackPlay)   // Play tracking for analytics and recommendations
			posts.POST("/:id/view", h.ViewPost)    // View tracking for analytics and recommendations
			// Save/Bookmark routes (P0 Social Feature)
			posts.POST("/:id/save", h.SavePost)
			posts.DELETE("/:id/save", h.UnsavePost)
			posts.GET("/:id/saved", h.IsPostSaved)
			// Repost routes (P0 Social Feature)
			posts.POST("/:id/repost", h.CreateRepost)
			posts.DELETE("/:id/repost", h.UndoRepost)
			posts.GET("/:id/reposts", h.GetReposts)
			posts.GET("/:id/reposted", h.IsPostReposted)
			// Archive routes (hide without delete)
			posts.POST("/:id/archive", h.ArchivePost)
			posts.POST("/:id/unarchive", h.UnarchivePost)
			posts.GET("/:id/archived", h.IsPostArchived)
			// Comment controls
			posts.PUT("/:id/comment-audience", h.UpdateCommentAudience)
			// Pin posts to profile
			posts.POST("/:id/pin", h.PinPost)
			posts.DELETE("/:id/pin", h.UnpinPost)
			posts.PUT("/:id/pin-order", h.UpdatePinOrder)
			posts.GET("/:id/pinned", h.IsPostPinned)
		}

		// Comment routes
		comments := api.Group("/comments")
		{
			comments.Use(authHandlers.AuthMiddleware())
			comments.GET("/:id/replies", h.GetCommentReplies)
			comments.PUT("/:id", h.UpdateComment)
			comments.DELETE("/:id", h.DeleteComment)
			comments.POST("/:id/like", h.LikeComment)
			comments.DELETE("/:id/like", h.UnlikeComment)
			comments.POST("/:id/report", h.ReportComment)
		}

		// Error tracking routes (Task 4.19: Error tracking and reporting)
		errorTrackingHandler := handlers.NewErrorTrackingHandler()
		errors := api.Group("/errors")
		{
			errors.Use(authHandlers.AuthMiddleware())
			errors.POST("/batch", errorTrackingHandler.RecordErrors)
			errors.GET("/stats", errorTrackingHandler.GetErrorStats)
			errors.GET("/:error_id", errorTrackingHandler.GetErrorDetails)
			errors.PUT("/:error_id/resolve", errorTrackingHandler.ResolveError)
		}

		// Story routes (7.5) - with upload rate limiting for creation
		stories := api.Group("/stories")
		{
			stories.Use(authHandlers.AuthMiddleware())
			stories.POST("", middleware.RateLimitUpload(), h.CreateStory)
			stories.GET("", h.GetStories)
			stories.GET("/:id", h.GetStory)
			stories.POST("/:id/view", h.ViewStory)
			stories.GET("/:id/views", h.GetStoryViews)
			stories.POST("/:id/download", h.DownloadStory) // 19.1 - Story download
			stories.DELETE("/:id", h.DeleteStory)
			// Remix routes for stories (R.3.2)
			stories.POST("/:id/remix", h.CreateRemixPost)
			stories.GET("/:id/remixes", h.GetStoryRemixes)
			stories.GET("/:id/remix-source", h.GetRemixSource)
		}

		// User stories route (7.5.1.3.3)
		users.GET("/:id/stories", h.GetUserStories)

		// Story highlight routes (7.5.6)
		highlights := api.Group("/highlights")
		{
			highlights.Use(authHandlers.AuthMiddleware())
			highlights.POST("", h.CreateHighlight)
			highlights.GET("/:id", h.GetHighlight)
			highlights.PUT("/:id", h.UpdateHighlight)
			highlights.DELETE("/:id", h.DeleteHighlight)
			highlights.POST("/:id/stories", h.AddStoryToHighlight)
			highlights.DELETE("/:id/stories/:story_id", h.RemoveStoryFromHighlight)
		}

		// User highlights route (7.5.6.2)
		users.GET("/:id/highlights", h.GetHighlights)

		// MIDI pattern routes (R.3.3)
		midi := api.Group("/midi")
		{
			midi.Use(authHandlers.AuthMiddleware())
			midi.POST("", h.CreateMIDIPattern)
			midi.GET("", h.ListMIDIPatterns)
			midi.GET("/:id", h.GetMIDIPattern)
			midi.GET("/:id/file", h.GetMIDIPatternFile)
			midi.PATCH("/:id", h.UpdateMIDIPattern)
			midi.DELETE("/:id", h.DeleteMIDIPattern)
		}

		// Project file routes (R.3.4)
		projectFiles := api.Group("/project-files")
		{
			projectFiles.Use(authHandlers.AuthMiddleware())
			projectFiles.POST("", h.CreateProjectFile)
			projectFiles.GET("", h.ListProjectFiles)
			projectFiles.GET("/:id", h.GetProjectFile)
			projectFiles.GET("/:id/download", h.DownloadProjectFile)
			projectFiles.DELETE("/:id", h.DeleteProjectFile)
		}

		// Post project file route (R.3.4)
		posts.GET("/:id/project-file", h.GetPostProjectFile)

		// Remix routes (R.3.2 Remix Chains)
		posts.POST("/:id/remix", h.CreateRemixPost)
		posts.GET("/:id/remix-chain", h.GetRemixChain)
		posts.GET("/:id/remixes", h.GetPostRemixes)
		posts.GET("/:id/remix-source", h.GetRemixSource)

		// Playlist routes (R.3.1 Collaborative Playlists)
		playlists := api.Group("/playlists")
		{
			playlists.Use(authHandlers.AuthMiddleware())
			playlists.POST("", h.CreatePlaylist)
			playlists.GET("", h.GetPlaylists)
			playlists.GET("/:id", h.GetPlaylist)
			playlists.PUT("/:id", h.UpdatePlaylist)
			playlists.POST("/:id/entries", h.AddPlaylistEntry)
			playlists.DELETE("/:id/entries/:entry_id", h.DeletePlaylistEntry)
			playlists.POST("/:id/collaborators", h.AddPlaylistCollaborator)
			playlists.DELETE("/:id/collaborators/:user_id", h.DeletePlaylistCollaborator)
		}

		// MIDI Challenge routes (R.2.2 MIDI Battle Royale)
		midiChallenges := api.Group("/midi-challenges")
		{
			// Public routes (no auth required for viewing)
			midiChallenges.GET("", h.GetMIDIChallenges)
			midiChallenges.GET("/:id", h.GetMIDIChallenge)
			midiChallenges.GET("/:id/entries", h.GetMIDIChallengeEntries)

			// Protected routes (auth required)
			midiChallenges.Use(authHandlers.AuthMiddleware())
			midiChallenges.POST("", h.CreateMIDIChallenge)
			midiChallenges.POST("/:id/entries", h.CreateMIDIChallengeEntry)
			midiChallenges.POST("/:id/entries/:entry_id/vote", h.VoteMIDIChallengeEntry)
		}

		// Sound routes (Feature #15 - Sound/Sample Pages)
		soundHandlers := handlers.NewSoundHandlers()
		sounds := api.Group("/sounds")
		{
			// Public: Get trending sounds
			sounds.GET("/trending", soundHandlers.GetTrendingSounds)
			sounds.GET("/search", soundHandlers.SearchSounds)

			// Protected routes
			sounds.Use(authHandlers.AuthMiddleware())
			sounds.GET("/:id", soundHandlers.GetSound)
			sounds.GET("/:id/posts", soundHandlers.GetSoundPosts)
			sounds.PATCH("/:id", soundHandlers.UpdateSound)
		}

		// Post sound route (Feature #15)
		posts.GET("/:id/sound", soundHandlers.GetPostSound)

		// WebSocket auxiliary routes (the main ws routes are registered before middleware)
		ws := api.Group("/ws")
		{
			// Note: Main WebSocket connection endpoints (GET "" and GET "/connect") are registered
			// at the top of main() BEFORE middleware to avoid connection hijacking issues

			// Metrics endpoint (protected)
			ws.GET("/metrics", authHandlers.AuthMiddleware(), wsHandler.HandleMetrics)

			// Online status check (protected)
			ws.POST("/online", authHandlers.AuthMiddleware(), wsHandler.HandleOnlineStatus)

			// Presence endpoints (protected)
			ws.POST("/presence", authHandlers.AuthMiddleware(), wsHandler.HandlePresenceStatus)
			ws.GET("/friends-in-studio", authHandlers.AuthMiddleware(), wsHandler.HandleFriendsInStudio)
		}
	}

	// Server configuration
	port := os.Getenv("PORT")
	if port == "" {
		port = "8787"
	}

	// Create a custom handler that routes WebSocket requests to raw HTTP handler
	// This bypasses Gin's ResponseWriter wrapper which interferes with connection hijacking
	handler := http.HandlerFunc(func(w http.ResponseWriter, req *http.Request) {
		// Route WebSocket upgrade requests directly to raw HTTP handler
		if req.URL.Path == "/api/v1/ws" || req.URL.Path == "/api/v1/ws/connect" {
			wsHandler.HandleWebSocketHTTP(w, req)
			return
		}
		// All other requests go through Gin
		r.ServeHTTP(w, req)
	})

	srv := &http.Server{
		Addr:    ":" + port,
		Handler: handler,
	}

	// Start server in a goroutine
	go func() {
		logger.Log.Info("🚀 Sidechain backend starting",
			zap.String("port", port),
		)
		if err := srv.ListenAndServe(); err != nil && err != http.ErrServerClosed {
			logger.FatalWithFields("Failed to start server", err)
		}
	}()

	// Wait for interrupt signal to gracefully shutdown the server
	quit := make(chan os.Signal, 1)
	signal.Notify(quit, syscall.SIGINT, syscall.SIGTERM)
	<-quit
	logger.Log.Info("Shutting down server...")

	// Give outstanding requests 30 seconds to complete
	ctx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
	defer cancel()

	// Shutdown WebSocket connections gracefully
	if err := wsHandler.Shutdown(ctx); err != nil {
		logger.WarnWithFields("WebSocket shutdown warning", err)
	}

	if err := srv.Shutdown(ctx); err != nil {
		logger.ErrorWithFields("Server forced to shutdown", err)
	}

	logger.Log.Info("Server exited")
}
