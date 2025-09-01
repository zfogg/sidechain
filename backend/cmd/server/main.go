package main

import (
	"context"
	"log"
	"net/http"
	"os"
	"os/signal"
	"syscall"
	"time"

	"github.com/gin-contrib/cors"
	"github.com/gin-gonic/gin"
	"github.com/joho/godotenv"
	"github.com/zfogg/sidechain/backend/internal/audio"
	"github.com/zfogg/sidechain/backend/internal/auth"
	"github.com/zfogg/sidechain/backend/internal/database"
	"github.com/zfogg/sidechain/backend/internal/handlers"
	"github.com/zfogg/sidechain/backend/internal/storage"
	"github.com/zfogg/sidechain/backend/internal/stream"
)

func main() {
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

	// Initialize audio processor
	audioProcessor := audio.NewProcessor(s3Uploader)

	// Initialize handlers
	h := handlers.NewHandlers(streamClient, audioProcessor)
	authHandlers := handlers.NewAuthHandlers(authService)

	// Setup Gin router
	r := gin.Default()

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
	{
		// Authentication routes (public)
		authGroup := api.Group("/auth")
		{
			// Native auth
			authGroup.POST("/register", authHandlers.Register)
			authGroup.POST("/login", authHandlers.Login)

			// OAuth flows
			authGroup.GET("/google", authHandlers.GoogleOAuth)
			authGroup.GET("/google/callback", authHandlers.GoogleCallback)
			authGroup.GET("/discord", authHandlers.DiscordOAuth)
			authGroup.GET("/discord/callback", authHandlers.DiscordCallback)

			// User info (protected)
			authGroup.GET("/me", authHandlers.AuthMiddleware(), authHandlers.Me)
		}

		// Audio routes
		audio := api.Group("/audio")
		{
			audio.Use(authHandlers.AuthMiddleware())
			audio.POST("/upload", h.UploadAudio)
			audio.GET("/:id", h.GetAudio)
		}

		// Feed routes
		feed := api.Group("/feed")
		{
			feed.Use(authHandlers.AuthMiddleware())
			feed.GET("/timeline", h.GetTimeline)
			feed.GET("/global", h.GetGlobalFeed)
			feed.POST("/post", h.CreatePost)
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

		// User routes
		users := api.Group("/users")
		{
			users.Use(authHandlers.AuthMiddleware())
			users.GET("/me", h.GetProfile)
			users.PUT("/me", h.UpdateProfile)
		}
	}

	// Server configuration
	port := os.Getenv("PORT")
	if port == "" {
		port = "8080"
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

	if err := srv.Shutdown(ctx); err != nil {
		log.Fatal("Server forced to shutdown:", err)
	}

	log.Println("Server exited")
}
