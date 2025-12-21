package main

import (
	"context"
	"fmt"
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
	"github.com/joho/godotenv"
	"github.com/prometheus/client_golang/prometheus/promhttp"
	"github.com/zfogg/sidechain/backend/internal/alerts"
	"github.com/zfogg/sidechain/backend/internal/audio"
	"github.com/zfogg/sidechain/backend/internal/auth"
	"github.com/zfogg/sidechain/backend/internal/cache"
	"github.com/zfogg/sidechain/backend/internal/challenges"
	"github.com/zfogg/sidechain/backend/internal/config"
	"github.com/zfogg/sidechain/backend/internal/database"
	"github.com/zfogg/sidechain/backend/internal/email"
	"github.com/zfogg/sidechain/backend/internal/handlers"
	"github.com/zfogg/sidechain/backend/internal/kernel"
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
	"github.com/zfogg/sidechain/backend/internal/telemetry"
	"github.com/zfogg/sidechain/backend/internal/waveform"
	"github.com/zfogg/sidechain/backend/internal/websocket"
	"go.opentelemetry.io/otel/sdk/trace"
	"go.uber.org/zap"
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

	// Initialize OpenTelemetry (if enabled)
	var tracerProvider *trace.TracerProvider
	if os.Getenv("OTEL_ENABLED") == "true" {
		cfg := telemetry.Config{
			ServiceName:  getEnvOrDefault("OTEL_SERVICE_NAME", "sidechain-backend"),
			Environment:  getEnvOrDefault("OTEL_ENVIRONMENT", "development"),
			OTLPEndpoint: getEnvOrDefault("OTEL_EXPORTER_OTLP_ENDPOINT", "localhost:4318"),
			Enabled:      true,
			SamplingRate: getEnvFloat("OTEL_TRACE_SAMPLER_RATE", 1.0),
		}

		var tracerErr error
		tracerProvider, tracerErr = telemetry.InitTracer(cfg)
		if tracerErr != nil {
			logger.Log.Warn("Failed to initialize OpenTelemetry", zap.Error(tracerErr))
		} else {
			logger.Log.Info("✅ OpenTelemetry tracing enabled",
				zap.String("service", cfg.ServiceName),
				zap.Float64("sampling_rate", cfg.SamplingRate),
				zap.String("endpoint", cfg.OTLPEndpoint),
			)
			defer func() {
				if tracerProvider != nil {
					shutdownErr := tracerProvider.Shutdown(context.Background())
					if shutdownErr != nil {
						logger.Log.Error("Failed to shutdown tracer provider", zap.Error(shutdownErr))
					}
				}
			}()
		}
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

		// Check if Elasticsearch indices need reindexing (automatic schema migration)
		go func() {
			ctx, cancel := context.WithTimeout(context.Background(), 2*time.Minute)
			defer cancel()

			needsReindex, err := baseSearchClient.CheckIndexVersion(ctx)
			if err != nil {
				logger.Log.Warn("Failed to check Elasticsearch index version",
					zap.Error(err),
				)
				return
			}

			if needsReindex {
				logger.Log.Info("🔄 Elasticsearch index schema outdated, starting automatic reindex...")

				// Delete old indices
				for _, indexName := range []string{search.IndexPosts, search.IndexUsers, search.IndexStories} {
					if err := baseSearchClient.DeleteIndex(ctx, indexName); err != nil {
						logger.Log.Warn("Failed to delete old index",
							zap.String("index", indexName),
							zap.Error(err),
						)
					}
				}

				// Recreate indices with new schema
				if err := baseSearchClient.InitializeIndices(ctx); err != nil {
					logger.Log.Error("Failed to initialize Elasticsearch indices during reindex",
						zap.Error(err),
					)
					return
				}

				// Update version metadata in all indices
				for _, indexName := range []string{search.IndexPosts, search.IndexUsers, search.IndexStories} {
					if err := baseSearchClient.UpdateIndexVersion(ctx, indexName); err != nil {
						logger.Log.Warn("Failed to update index version",
							zap.String("index", indexName),
							zap.Error(err),
						)
					}
				}

				// Backfill data from database
				backfillStart := time.Now()
				if err := baseSearchClient.BackfillAllIndices(ctx); err != nil {
					logger.Log.Error("Failed to backfill Elasticsearch indices",
						zap.Error(err),
					)
					return
				}

				logger.Log.Info("✅ Automatic Elasticsearch reindex completed",
					zap.Duration("duration", time.Since(backfillStart)),
				)
			} else {
				logger.Log.Info("✅ Elasticsearch indices up to date")
			}
		}()

		// Initialize Elasticsearch reconciliation service (runs every hour)
		reconciliationInterval := 1 * time.Hour
		if intervalStr := os.Getenv("ELASTICSEARCH_RECONCILIATION_INTERVAL"); intervalStr != "" {
			if parsed, err := time.ParseDuration(intervalStr); err == nil {
				reconciliationInterval = parsed
			}
		}
		reconciliationService := search.NewReconciliationService(baseSearchClient, reconciliationInterval)
		reconciliationService.Start()
		defer reconciliationService.Stop()
	}

	// Initialize alert system
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

	// Presence management is now handled entirely by GetStream.io

	// Initialize story cleanup service (runs every hour)
	storyCleanup := stories.NewCleanupService(s3Uploader, 1*time.Hour)
	storyCleanup.Start()
	defer storyCleanup.Stop()

	// Initialize MIDI challenge notification service (runs every 15 minutes)
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

	// Set up post completion callback to sync to Gorse and Elasticsearch
	audioProcessor.SetPostCompleteCallback(func(postID string) {
		logger.Log.Debug("Post completed processing, syncing to Gorse and Elasticsearch",
			logger.WithPostID(postID),
		)

		// Sync to Gorse
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

		// Sync to Elasticsearch
		if baseSearchClient != nil {
			go func() {
				ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
				defer cancel()

				var post models.AudioPost
				if err := database.DB.First(&post, "id = ?", postID).Error; err != nil {
					logger.Log.Warn("Failed to fetch post for ES indexing",
						zap.Error(err),
						logger.WithPostID(postID),
					)
					return
				}

				var user models.User
				if err := database.DB.First(&user, "id = ?", post.UserID).Error; err != nil {
					logger.Log.Warn("Failed to fetch user for ES indexing",
						zap.Error(err),
						logger.WithPostID(postID),
					)
					return
				}

				postDoc := search.AudioPostToSearchDoc(post, user.Username)
				if err := baseSearchClient.IndexPost(ctx, post.ID, postDoc); err != nil {
					logger.Log.Warn("Failed to index post to Elasticsearch after upload",
						zap.Error(err),
						logger.WithPostID(postID),
					)
				} else {
					logger.Log.Debug("Successfully indexed post to Elasticsearch after upload",
						logger.WithPostID(postID),
					)
				}
			}()
		}
	})

	// Configure batch sync interval
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

	// Start background batch sync
	syncCtx, syncCancel := context.WithCancel(context.Background())
	defer syncCancel()

	// Initial sync on startup
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

	// Periodic batch sync
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

	// Daily CTR metrics logging
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
		logger.Log.Info("✅ Waveform tools initialized successfully")
	}

	// ============================================================
	// SETUP DEPENDENCY INJECTION CONTAINER
	// ============================================================
	appKernel := kernel.New()

	// Register all services in the container
	appKernel.
		WithDB(database.DB).
		WithLogger(logger.Log).
		WithStreamClient(streamClient).
		WithAuthService(authService).
		WithAudioProcessor(audioProcessor).
		WithWebSocketHandler(wsHandler).
		WithAlertManager(alertManager).
		WithAlertEvaluator(alertEvaluator)

	// Register optional services
	if redisClient != nil {
		appKernel.WithCache(redisClient)
	}
	if s3Uploader != nil {
		appKernel.WithS3Uploader(s3Uploader)
	}
	if baseSearchClient != nil {
		appKernel.WithSearchClient(baseSearchClient)
	}
	if gorseClient != nil {
		appKernel.WithGorseClient(gorseClient)
	}
	if waveformGenerator != nil {
		appKernel.WithWaveformGenerator(waveformGenerator)
	}
	if waveformStorage != nil {
		appKernel.WithWaveformStorage(waveformStorage)
	}

	// Validate container has all required dependencies
	if err := appKernel.Validate(); err != nil {
		logger.FatalWithFields("Container validation failed", err)
	}
	logger.Log.Info("✅ Dependency injection container initialized")

	// Register cleanup hooks for graceful shutdown
	appKernel.
		OnCleanup(func(ctx context.Context) error {
			if audioProcessor != nil {
				audioProcessor.Stop()
			}
			return nil
		}).
		OnCleanup(func(ctx context.Context) error {
			if redisClient != nil {
				return redisClient.Close()
			}
			return nil
		})

	// Initialize handlers with dependency injection container
	h := handlers.NewHandlers(appKernel)
	authHandlers := handlers.NewAuthHandlers(appKernel)

	// Initialize Prometheus metrics
	metrics.Initialize()
	logger.Log.Info("Prometheus metrics initialized")

	// Setup Gin router - use gin.New instead of gin.Default to control middleware
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
		// Trim whitespace from each origin and validate
		validOrigins := []string{}
		for _, origin := range corsConfig.AllowOrigins {
			origin = strings.TrimSpace(origin)

			// Security: Reject dangerous CORS patterns
			if origin == "*" {
				logger.Log.Warn("⚠️ CORS misconfiguration: wildcard origin '*' not allowed for security",
					zap.String("env_var", "ALLOWED_ORIGINS"),
				)
				continue
			}
			if strings.Contains(origin, "*") {
				logger.Log.Warn("⚠️ CORS misconfiguration: origins with wildcards not allowed",
					zap.String("rejected_origin", origin),
				)
				continue
			}
			if !strings.HasPrefix(origin, "http://") && !strings.HasPrefix(origin, "https://") {
				logger.Log.Warn("⚠️ CORS misconfiguration: origin must use http://or https://",
					zap.String("rejected_origin", origin),
				)
				continue
			}
			validOrigins = append(validOrigins, origin)
		}

		// If all origins were rejected, fall back to safe defaults
		if len(validOrigins) == 0 {
			logger.Log.Error("⚠️ CORS configuration had no valid origins, using safe defaults")
			validOrigins = []string{"https://www.sidechain.live", "https://sidechain.live"}
		}
		corsConfig.AllowOrigins = validOrigins
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
	r.Use(middleware.RequestIDMiddleware()) // Add request ID tracking
	r.Use(middleware.MetricsMiddleware())   // Prometheus metrics collection
	r.Use(middleware.GinLoggerMiddleware()) // Structured logging

	// Add tracing middleware (if enabled)
	if os.Getenv("OTEL_ENABLED") == "true" {
		r.Use(middleware.TracingMiddleware("sidechain-backend"))
		logger.Log.Info("✅ OpenTelemetry tracing middleware registered")
	}

	r.Use(gin.Recovery())

	// Gzip compression middleware (compress responses > 1KB)
	r.Use(gzip.Gzip(gzip.DefaultCompression, gzip.WithExcludedPaths([]string{
		"/api/v1/ws",         // Don't compress WebSocket connections
		"/api/v1/ws/connect", // Don't compress WebSocket connections
		"/metrics",           // Don't compress Prometheus metrics (binary format issue)
		"/internal/metrics",  // Don't compress internal Prometheus metrics
	})))

	// Health check endpoint
	r.GET("/health", func(c *gin.Context) {
		c.JSON(http.StatusOK, gin.H{
			"status":    "ok",
			"timestamp": time.Now().UTC(),
			"service":   "sidechain-backend",
		})
	})

	// Test endpoint to trigger 500 error for Grafana dashboard testing
	r.GET("/test-error", func(c *gin.Context) {
		c.JSON(http.StatusInternalServerError, gin.H{
			"error":   "test_error",
			"message": "This is a test error for Grafana dashboard verification",
		})
	})

	// Test endpoint to initialize cache metrics for Grafana dashboard
	r.GET("/test-cache-init", func(c *gin.Context) {
		// Initialize cache metrics so they appear in Prometheus
		middleware.RecordCacheHit("response_cache")
		middleware.RecordCacheMiss("response_cache")
		// Reset to 0 so we start fresh but metrics exist
		m := metrics.Get()
		m.CacheHitsTotal.Reset()
		m.CacheMissesTotal.Reset()
		c.JSON(http.StatusOK, gin.H{
			"status":  "initialized",
			"message": "Cache metrics initialized for Grafana dashboard",
		})
	})

	// Prometheus metrics endpoint (admin only - requires authentication)
	r.GET("/metrics",
		authHandlers.AuthMiddleware(),
		middleware.RequireAdmin(),
		gin.WrapH(promhttp.Handler()),
	)

	// Internal Prometheus metrics endpoint (unauthenticated, for Prometheus scraping)
	// This endpoint should only be accessible from the Docker network
	r.GET("/internal/metrics",
		gin.WrapH(promhttp.Handler()),
	)

	// API routes
	api := r.Group("/api/v1")
	api.Use(middleware.RateLimit())                    // Global rate limit: 100 req/min per IP
	api.Use(middleware.AdminImpersonationMiddleware()) // Admin user impersonation support
	{
		// Authentication routes (public) - stricter rate limit
		authGroup := api.Group("/auth")
		authGroup.Use(middleware.RateLimitAuth()) // Auth rate limit: 10 req/min per IP
		{
			// Native auth
			authGroup.POST("/register", authHandlers.Register)
			authGroup.POST("/login", authHandlers.Login)

			// Token refresh (accepts old token, returns new one)
			authGroup.POST("/refresh", authHandlers.RefreshToken)

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

			// OAuth token exchange endpoint for web flow (secure session-based token retrieval - SEC-5)
			authGroup.POST("/oauth-token", authHandlers.ExchangeOAuthToken)

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
			social.POST("/follow", middleware.CacheInvalidationMiddleware("response:/api/v1/feed/*"), h.FollowUser)
			social.POST("/unfollow", middleware.CacheInvalidationMiddleware("response:/api/v1/feed/*"), h.UnfollowUser)
			social.POST("/like", h.LikePost)
			social.DELETE("/like", h.UnlikePost)
			social.POST("/react", h.EmojiReact)
		}

		// Public profile picture URL endpoint (no auth required)
		// Returns the profile picture URL for JUCE to download directly via HTTPS
		api.GET("/users/:id/profile-picture", authHandlers.GetProfilePictureURL)

		// Public user profile endpoints (optional auth for personalized data like is_following)
		// Use AuthMiddlewareOptional to set user context if token is provided
		publicUsers := api.Group("/users")
		publicUsers.Use(authHandlers.AuthMiddlewareOptional())
		{
			publicUsers.GET("/:id/profile", h.GetUserProfile)
			publicUsers.GET("/:id/posts", h.GetUserPosts)
			publicUsers.GET("/:id/followers", h.GetUserFollowers)
			publicUsers.GET("/:id/following", h.GetUserFollowing)
			publicUsers.GET("/:id/reposts", h.GetUserReposts)
			publicUsers.GET("/:id/pinned", h.GetPinnedPosts)
			publicUsers.GET("/:id/muted", h.IsUserMuted)
		}

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
			users.POST("/:id/follow", middleware.CacheInvalidationMiddleware("response:/api/v1/feed/*"), h.FollowUserByID)
			users.DELETE("/:id/follow", middleware.CacheInvalidationMiddleware("response:/api/v1/feed/*"), h.UnfollowUserByID)
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
			followRequests.POST("/:id/accept", middleware.CacheInvalidationMiddleware("response:/api/v1/feed/*"), h.AcceptFollowRequest)
			followRequests.POST("/:id/reject", middleware.CacheInvalidationMiddleware("response:/api/v1/feed/*"), h.RejectFollowRequest)
			followRequests.DELETE("/:id", middleware.CacheInvalidationMiddleware("response:/api/v1/feed/*"), h.CancelFollowRequest)
		}

		// Search routes (public, optional auth for privacy filtering - )
		search := api.Group("/search")
		{
			search.Use(authHandlers.AuthMiddlewareOptional())
			search.Use(middleware.RateLimitSearch()) // Rate limit search requests
			// Cache search results for 2 minutes (Elasticsearch results are cached)
			search.Use(middleware.ResponseCacheMiddleware(2 * time.Minute))
			search.GET("/users", h.SearchUsers)
			search.GET("/posts", h.SearchPosts)       // Search posts with Elasticsearch
			search.GET("/stories", h.SearchStories)   // Search stories by creator username
			search.GET("/advanced", h.AdvancedSearch) // Unified search across all types
			search.GET("/genres", h.GetAvailableGenres)
			search.GET("/keys", h.GetAvailableKeys)

			// Autocomplete endpoints
			autocomplete := search.Group("/autocomplete")
			{
				autocomplete.GET("/users", h.AutocompleteUsers)   // Username suggestions
				autocomplete.GET("/genres", h.AutocompleteGenres) // Genre suggestions
			}
		}

		// Metrics routes

		// Alert routes
		alert := api.Group("/alerts")
		{
			alert.GET("", h.GetAlerts)                  // Get all alerts
			alert.GET("/active", h.GetActiveAlerts)     // Get active alerts
			alert.GET("/type/:type", h.GetAlertsByType) // Get alerts by type
			alert.PUT("/:id/resolve", h.ResolveAlert)   // Resolve an alert
			alert.GET("/stats", h.GetAlertStats)        // Get alert statistics

			// Alert rules management
			rules := alert.Group("/rules")
			{
				rules.GET("", h.GetRules)       // Get all rules
				rules.POST("", h.CreateRule)    // Create new rule
				rules.PUT("/:id", h.UpdateRule) // Update rule
			}
		}
		metrics := api.Group("/metrics")
		{
			metrics.GET("", h.GetAllMetrics)           // Get all metrics
			metrics.GET("/search", h.GetSearchMetrics) // Get search-specific metrics
			metrics.POST("/reset", h.ResetMetrics)     // Reset metrics (admin only)
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
			// Negative feedback endpoints
			recommendations.POST("/dislike/:post_id", h.NotInterestedInPost) // "Not interested" button
			recommendations.POST("/skip/:post_id", h.SkipPost)               // Track when user scrolls past quickly
			recommendations.POST("/hide/:post_id", h.HidePost)               // "Hide this post" button
			// Discovery endpoints
			recommendations.GET("/popular", h.GetPopular)              // Globally popular posts
			recommendations.GET("/latest", h.GetLatest)                // Recently added posts
			recommendations.GET("/discovery-feed", h.GetDiscoveryFeed) // Blended discovery feed
			// CTR tracking
			recommendations.POST("/click", h.TrackRecommendationClick) // Track recommendation clicks
			recommendations.GET("/metrics/ctr", h.GetCTRMetrics)       // Get CTR metrics
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
			posts.POST("/:id/play", h.TrackPlay) // Play tracking for analytics and recommendations
			posts.POST("/:id/view", h.ViewPost)  // View tracking for analytics and recommendations
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

		// Error tracking routes
		errorTrackingHandler := handlers.NewErrorTrackingHandler(appKernel)
		errors := api.Group("/errors")
		{
			errors.Use(authHandlers.AuthMiddleware())
			errors.POST("/batch", errorTrackingHandler.RecordErrors)
			errors.GET("/stats", errorTrackingHandler.GetErrorStats)
			errors.GET("/:error_id", errorTrackingHandler.GetErrorDetails)
			errors.PUT("/:error_id/resolve", errorTrackingHandler.ResolveError)
		}

		// Story routes - with upload rate limiting for creation
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
			// Remix routes for stories
			stories.POST("/:id/remix", h.CreateRemixPost)
			stories.GET("/:id/remixes", h.GetStoryRemixes)
			stories.GET("/:id/remix-source", h.GetRemixSource)
		}

		// User stories route
		users.GET("/:id/stories", h.GetUserStories)

		// Story highlight routes
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

		// User highlights route
		users.GET("/:id/highlights", h.GetHighlights)

		// MIDI pattern routes
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

		// Project file routes
		projectFiles := api.Group("/project-files")
		{
			projectFiles.Use(authHandlers.AuthMiddleware())
			projectFiles.POST("", h.CreateProjectFile)
			projectFiles.GET("", h.ListProjectFiles)
			projectFiles.GET("/:id", h.GetProjectFile)
			projectFiles.GET("/:id/download", h.DownloadProjectFile)
			projectFiles.DELETE("/:id", h.DeleteProjectFile)
		}

		// Post project file route
		posts.GET("/:id/project-file", h.GetPostProjectFile)

		// Remix routes
		posts.POST("/:id/remix", h.CreateRemixPost)
		posts.GET("/:id/remix-chain", h.GetRemixChain)
		posts.GET("/:id/remixes", h.GetPostRemixes)
		posts.GET("/:id/remix-source", h.GetRemixSource)

		// Playlist routes
		playlists := api.Group("/playlists")
		{
			playlists.Use(authHandlers.AuthMiddleware())
			playlists.POST("", h.CreatePlaylist)
			playlists.GET("", h.GetPlaylists)
			playlists.GET("/:id", h.GetPlaylist)
			playlists.PUT("/:id", h.UpdatePlaylist)
			playlists.DELETE("/:id", h.DeletePlaylist)
			playlists.POST("/:id/entries", h.AddPlaylistEntry)
			playlists.DELETE("/:id/entries/:entry_id", h.DeletePlaylistEntry)
			playlists.POST("/:id/collaborators", h.AddPlaylistCollaborator)
			playlists.DELETE("/:id/collaborators/:user_id", h.DeletePlaylistCollaborator)
		}

		// MIDI Challenge routes
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

		// Sound routes
		soundHandlers := handlers.NewSoundHandlers(appKernel)
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

		// Post sound route
		posts.GET("/:id/sound", soundHandlers.GetPostSound)

		// WebSocket auxiliary routes (the main ws routes are registered before middleware)
		ws := api.Group("/ws")
		{
			// Note: Main WebSocket connection endpoints (GET "" and GET "/connect") are registered
			// at the top of main BEFORE middleware to avoid connection hijacking issues

			// Metrics endpoint (protected)
			ws.GET("/metrics", authHandlers.AuthMiddleware(), wsHandler.HandleMetrics)

			// Online status check (protected)
			ws.POST("/online", authHandlers.AuthMiddleware(), wsHandler.HandleOnlineStatus)
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

	// Cleanup application services via DI container
	if err := appKernel.Cleanup(ctx); err != nil {
		logger.Log.Error("Error during application cleanup", zap.Error(err))
	}

	// Shutdown WebSocket connections gracefully
	if err := wsHandler.Shutdown(ctx); err != nil {
		logger.WarnWithFields("WebSocket shutdown warning", err)
	}

	if err := srv.Shutdown(ctx); err != nil {
		logger.ErrorWithFields("Server forced to shutdown", err)
	}

	logger.Log.Info("Server exited")
}

// getEnvOrDefault returns the value of an environment variable or a default value
func getEnvOrDefault(key, defaultValue string) string {
	if value := os.Getenv(key); value != "" {
		return value
	}
	return defaultValue
}

// getEnvFloat parses an environment variable as a float64 or returns a default value
func getEnvFloat(key string, defaultValue float64) float64 {
	if value := os.Getenv(key); value != "" {
		if f, err := time.ParseDuration(value + "s"); err == nil {
			// Try parsing as duration first for convenience
			return f.Seconds()
		}
		// Parse as raw float
		var f float64
		if _, err := fmt.Sscanf(value, "%f", &f); err == nil {
			return f
		}
	}
	return defaultValue
}
