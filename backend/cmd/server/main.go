package main

import (
	"context"
	"io"
	"log"
	"net/http"
	"os"
	"os/signal"
	"syscall"
	"time"

	"github.com/gin-contrib/cors"
	"github.com/gin-contrib/gzip"
	"github.com/gin-gonic/gin"
	"github.com/joho/godotenv"
	"github.com/zfogg/sidechain/backend/internal/audio"
	"github.com/zfogg/sidechain/backend/internal/auth"
	"github.com/zfogg/sidechain/backend/internal/challenges"
	"github.com/zfogg/sidechain/backend/internal/database"
	"github.com/zfogg/sidechain/backend/internal/email"
	"github.com/zfogg/sidechain/backend/internal/handlers"
	"github.com/zfogg/sidechain/backend/internal/middleware"
	"github.com/zfogg/sidechain/backend/internal/models"
	"github.com/zfogg/sidechain/backend/internal/seed"
	"github.com/zfogg/sidechain/backend/internal/storage"
	"github.com/zfogg/sidechain/backend/internal/stories"
	"github.com/zfogg/sidechain/backend/internal/stream"
	"github.com/zfogg/sidechain/backend/internal/websocket"
)

func main() {
	// Setup file logging
	logFile, err := os.OpenFile("server.log", os.O_CREATE|os.O_WRONLY|os.O_APPEND, 0666)
	if err != nil {
		log.Fatalf("Failed to open log file: %v", err)
	}
	defer logFile.Close()

	// Write to both stdout and log file
	multiWriter := io.MultiWriter(os.Stdout, logFile)
	log.SetOutput(multiWriter)
	log.SetFlags(log.Ldate | log.Ltime | log.Lmicroseconds | log.Lshortfile)

	// Also configure Gin to log to the file
	gin.DefaultWriter = multiWriter
	gin.DefaultErrorWriter = multiWriter

	log.Println("=== Sidechain server starting ===")

	// Load environment variables
	if err := godotenv.Load(); err != nil {
		log.Println("Warning: .env file not found, using system environment variables")
	}

	// Initialize database
	if err := database.Initialize(); err != nil {
		log.Fatalf("Failed to initialize database: %v", err)
	}

	// Run migrations
	if err := database.Migrate(); err != nil {
		log.Fatalf("Failed to run migrations: %v", err)
	}

	// Auto-seed development data if in development mode and database is empty
	if os.Getenv("ENVIRONMENT") == "development" || os.Getenv("ENVIRONMENT") == "" {
		var userCount int64
		database.DB.Model(&models.User{}).Count(&userCount)
		if userCount == 0 {
			log.Println("🌱 Development mode: Database is empty, auto-seeding development data...")
			seeder := seed.NewSeeder(database.DB)
			if err := seeder.SeedDev(); err != nil {
				log.Printf("Warning: Auto-seed failed (non-fatal): %v", err)
				log.Println("You can manually seed with: go run cmd/seed/main.go dev")
			} else {
				log.Println("✅ Development data seeded successfully!")
			}
		} else {
			log.Printf("Database already has %d users, skipping auto-seed", userCount)
		}
	}

	// Initialize Stream.io client
	streamClient, err := stream.NewClient()
	if err != nil {
		log.Fatalf("Failed to initialize Stream.io client: %v", err)
	}

	// Initialize auth service
	jwtSecret := []byte(os.Getenv("JWT_SECRET"))
	if len(jwtSecret) == 0 {
		log.Fatalf("JWT_SECRET environment variable is required")
	}

	authService := auth.NewService(
		jwtSecret,
		streamClient,
		os.Getenv("GOOGLE_CLIENT_ID"),
		os.Getenv("GOOGLE_CLIENT_SECRET"),
		os.Getenv("DISCORD_CLIENT_ID"),
		os.Getenv("DISCORD_CLIENT_SECRET"),
	)

	// Initialize S3 uploader
	s3Uploader, err := storage.NewS3Uploader(
		os.Getenv("AWS_REGION"),
		os.Getenv("AWS_BUCKET"),
		os.Getenv("CDN_BASE_URL"),
	)
	if err != nil {
		log.Fatalf("Failed to initialize S3 uploader: %v", err)
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
			log.Printf("Warning: Failed to initialize email service: %v", err)
			log.Println("Password reset emails will not be sent - check AWS SES configuration")
			emailService = nil
		} else {
			log.Printf("✅ Email service (AWS SES) initialized successfully - from: %s", sesFromEmail)
		}
	} else {
		log.Println("Warning: SES_FROM_EMAIL or BASE_URL not set - password reset emails will not be sent")
		log.Println("Set SES_FROM_EMAIL, SES_FROM_NAME, and BASE_URL in .env to enable email sending")
	}

	// Check S3 access (skip for development)
	if err := s3Uploader.CheckBucketAccess(context.Background()); err != nil {
		log.Printf("Warning: S3 bucket access failed: %v", err)
		log.Println("Continuing without S3 - audio uploads will fail")
	}

	// Check FFmpeg availability
	if err := audio.CheckFFmpegAvailable(); err != nil {
		log.Printf("Warning: FFmpeg not available: %v", err)
		log.Println("Audio processing will be limited without FFmpeg")
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

	// Initialize handlers
	h := handlers.NewHandlers(streamClient, audioProcessor)
	h.SetWebSocketHandler(wsHandler) // Enable real-time follow notifications
	authHandlers := handlers.NewAuthHandlers(authService, s3Uploader, streamClient)
	authHandlers.SetJWTSecret(jwtSecret)
	authHandlers.SetEmailService(emailService) // Enable password reset emails

	// Setup Gin router
	r := gin.Default()

	// Gzip compression middleware (compress responses > 1KB)
	r.Use(gzip.Gzip(gzip.DefaultCompression, gzip.WithExcludedPaths([]string{
		"/api/v1/ws",         // Don't compress WebSocket connections
		"/api/v1/ws/connect", // Don't compress WebSocket connections
	})))

	// CORS middleware
	config := cors.DefaultConfig()
	config.AllowOrigins = []string{"*"} // Configure properly for production
	config.AllowMethods = []string{"GET", "POST", "PUT", "DELETE", "OPTIONS"}
	config.AllowHeaders = []string{"Origin", "Content-Length", "Content-Type", "Authorization"}
	r.Use(cors.New(config))

	// Health check endpoint
	r.GET("/health", func(c *gin.Context) {
		c.JSON(http.StatusOK, gin.H{
			"status":    "ok",
			"timestamp": time.Now().UTC(),
			"service":   "sidechain-backend",
		})
	})

	// API routes
	api := r.Group("/api/v1")
	api.Use(middleware.RateLimit()) // Global rate limit: 100 req/min per IP
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
			feed.GET("/timeline", h.GetTimeline)
			feed.GET("/timeline/enriched", h.GetEnrichedTimeline)
			feed.GET("/timeline/aggregated", h.GetAggregatedTimeline)
			feed.GET("/global", h.GetGlobalFeed)
			feed.GET("/global/enriched", h.GetEnrichedGlobalFeed)
			feed.GET("/trending", h.GetTrendingFeed)
			feed.GET("/trending/grouped", h.GetTrendingFeedGrouped)
			feed.POST("/post", h.CreatePost)
		}

		// Notification routes
		notifications := api.Group("/notifications")
		{
			notifications.Use(authHandlers.AuthMiddleware())
			notifications.GET("", h.GetNotifications)
			notifications.GET("/counts", h.GetNotificationCounts)
			notifications.POST("/read", h.MarkNotificationsRead)
			notifications.POST("/seen", h.MarkNotificationsSeen)
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

		// Public profile picture proxy (no auth required - works around JUCE SSL issues on Linux)
		// This must be outside the authenticated users group since JUCE may download
		// profile pictures before auth is fully established
		api.GET("/users/:id/profile-picture", authHandlers.ProxyProfilePicture)

		// User routes
		users := api.Group("/users")
		{
			users.Use(authHandlers.AuthMiddleware())
			users.GET("/me", h.GetProfile)
			users.PUT("/me", h.UpdateProfile)
			users.PUT("/username", h.ChangeUsername)
			users.POST("/upload-profile-picture", middleware.RateLimitUpload(), authHandlers.UploadProfilePicture)

			// User profile endpoints (require auth for following checks)
			users.GET("/:id/profile", h.GetUserProfile)
			users.GET("/:id/posts", h.GetUserPosts)
			users.GET("/:id/followers", h.GetUserFollowers)
			users.GET("/:id/following", h.GetUserFollowing)
			users.GET("/:id/activity", h.GetUserActivitySummary)
			users.GET("/:id/similar", h.GetSimilarUsers)
			users.POST("/:id/follow", h.FollowUserByID)
			users.DELETE("/:id/follow", h.UnfollowUserByID)
		}

		// Search routes
		search := api.Group("/search")
		{
			search.Use(authHandlers.AuthMiddleware())
			search.GET("/users", h.SearchUsers)
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
		}

		// Post routes (for comments, deletion, reporting)
		posts := api.Group("/posts")
		{
			posts.Use(authHandlers.AuthMiddleware())
			posts.POST("/:id/comments", h.CreateComment)
			posts.GET("/:id/comments", h.GetComments)
			posts.DELETE("/:id", h.DeletePost)
			posts.POST("/:id/report", h.ReportPost)
			posts.POST("/:id/download", h.DownloadPost)
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

		// Story routes (7.5) - with upload rate limiting for creation
		stories := api.Group("/stories")
		{
			stories.Use(authHandlers.AuthMiddleware())
			stories.POST("", middleware.RateLimitUpload(), h.CreateStory)
			stories.GET("", h.GetStories)
			stories.GET("/:id", h.GetStory)
			stories.POST("/:id/view", h.ViewStory)
			stories.GET("/:id/views", h.GetStoryViews)
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

		// WebSocket routes
		ws := api.Group("/ws")
		{
			// WebSocket connection endpoint - auth via query param ?token=... or Authorization header
			ws.GET("", wsHandler.HandleWebSocket)
			ws.GET("/connect", wsHandler.HandleWebSocket)

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

	srv := &http.Server{
		Addr:    ":" + port,
		Handler: r,
	}

	// Start server in a goroutine
	go func() {
		log.Printf("🎵 Sidechain backend starting on port %s", port)
		if err := srv.ListenAndServe(); err != nil && err != http.ErrServerClosed {
			log.Fatalf("Failed to start server: %v", err)
		}
	}()

	// Wait for interrupt signal to gracefully shutdown the server
	quit := make(chan os.Signal, 1)
	signal.Notify(quit, syscall.SIGINT, syscall.SIGTERM)
	<-quit
	log.Println("Shutting down server...")

	// Give outstanding requests 30 seconds to complete
	ctx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
	defer cancel()

	// Shutdown WebSocket connections gracefully
	if err := wsHandler.Shutdown(ctx); err != nil {
		log.Printf("WebSocket shutdown warning: %v", err)
	}

	if err := srv.Shutdown(ctx); err != nil {
		log.Fatal("Server forced to shutdown:", err)
	}

	log.Println("Server exited")
}
