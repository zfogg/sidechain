package database

import (
	"fmt"
	"log"
	"os"
	"time"

	"github.com/zfogg/sidechain/backend/internal/models"
	"gorm.io/driver/postgres"
	"gorm.io/gorm"
	"gorm.io/gorm/logger"
)

// DB holds the database connection
var DB *gorm.DB

// Initialize creates and configures the database connection
func Initialize() error {
	databaseURL := os.Getenv("DATABASE_URL")
	if databaseURL == "" {
		// Fallback to individual components
		host := getEnvOrDefault("DB_HOST", "localhost")
		port := getEnvOrDefault("DB_PORT", "5432")
		user := getEnvOrDefault("DB_USER", "postgres")
		password := getEnvOrDefault("DB_PASSWORD", "")
		dbname := getEnvOrDefault("DB_NAME", "sidechain")
		sslmode := getEnvOrDefault("DB_SSLMODE", "disable")

		databaseURL = fmt.Sprintf("host=%s port=%s user=%s password=%s dbname=%s sslmode=%s",
			host, port, user, password, dbname, sslmode)
	}

	// Configure GORM logger
	gormLogger := logger.Default
	if os.Getenv("ENVIRONMENT") == "development" {
		gormLogger = logger.Default.LogMode(logger.Info)
	}

	// Open database connection
	db, err := gorm.Open(postgres.Open(databaseURL), &gorm.Config{
		Logger: gormLogger,
		NowFunc: func() time.Time {
			return time.Now().UTC()
		},
	})
	if err != nil {
		return fmt.Errorf("failed to connect to database: %w", err)
	}

	// Configure connection pool
	sqlDB, err := db.DB()
	if err != nil {
		return fmt.Errorf("failed to get underlying sql.DB: %w", err)
	}

	sqlDB.SetMaxIdleConns(10)
	sqlDB.SetMaxOpenConns(100)
	sqlDB.SetConnMaxLifetime(time.Hour)

	DB = db
	log.Println("✅ Database connected successfully")

	return nil
}

// Migrate runs auto-migration for all models
func Migrate() error {
	if DB == nil {
		return fmt.Errorf("database not initialized")
	}

	// Enable UUID extension for PostgreSQL
	err := DB.Exec("CREATE EXTENSION IF NOT EXISTS \"uuid-ossp\"").Error
	if err != nil {
		log.Printf("Warning: Could not create uuid-ossp extension: %v", err)
	}

	// Auto-migrate all models
	err = DB.AutoMigrate(
		&models.User{},
		&models.AudioPost{},
		&models.Device{},
		&models.OAuthProvider{},
		&models.PasswordReset{},
		&models.Comment{},
		&models.CommentMention{},
		&models.Report{},
		&models.UserBlock{},
		&models.SearchQuery{},
		&models.UserPreference{},
		&models.PlayHistory{},
		&models.Hashtag{},
		&models.PostHashtag{},
		&models.Story{},
		&models.StoryView{},
		&models.StoryHighlight{},
		&models.HighlightedStory{},
	)
	if err != nil {
		return fmt.Errorf("failed to run migrations: %w", err)
	}

	// Create indexes for performance
	err = createIndexes()
	if err != nil {
		return fmt.Errorf("failed to create indexes: %w", err)
	}

	log.Println("✅ Database migrations completed")
	return nil
}

// createIndexes creates performance indexes
func createIndexes() error {
	// User indexes
	DB.Exec("CREATE INDEX IF NOT EXISTS idx_users_email_lower ON users (LOWER(email))")
	DB.Exec("CREATE INDEX IF NOT EXISTS idx_users_username_lower ON users (LOWER(username))")
	DB.Exec("CREATE INDEX IF NOT EXISTS idx_users_stream_user_id ON users (stream_user_id)")

	// AudioPost indexes for feed queries
	DB.Exec("CREATE INDEX IF NOT EXISTS idx_audio_posts_user_created ON audio_posts (user_id, created_at DESC)")
	DB.Exec("CREATE INDEX IF NOT EXISTS idx_audio_posts_public_created ON audio_posts (is_public, created_at DESC)")
	DB.Exec("CREATE INDEX IF NOT EXISTS idx_audio_posts_bpm ON audio_posts (bpm) WHERE bpm > 0")
	DB.Exec("CREATE INDEX IF NOT EXISTS idx_audio_posts_genre ON audio_posts USING GIN (genre)")

	// Device indexes
	DB.Exec("CREATE INDEX IF NOT EXISTS idx_devices_fingerprint ON devices (device_fingerprint)")
	DB.Exec("CREATE INDEX IF NOT EXISTS idx_devices_user_active ON devices (user_id, is_active)")

	// OAuth provider indexes
	DB.Exec("CREATE INDEX IF NOT EXISTS idx_oauth_providers_unique ON oauth_providers (provider, provider_user_id)")
	DB.Exec("CREATE INDEX IF NOT EXISTS idx_oauth_providers_email ON oauth_providers (email)")

	// Comment indexes for efficient retrieval
	DB.Exec("CREATE INDEX IF NOT EXISTS idx_comments_post_created ON comments (post_id, created_at DESC)")
	DB.Exec("CREATE INDEX IF NOT EXISTS idx_comments_user ON comments (user_id)")
	DB.Exec("CREATE INDEX IF NOT EXISTS idx_comments_parent ON comments (parent_id) WHERE parent_id IS NOT NULL")
	DB.Exec("CREATE INDEX IF NOT EXISTS idx_comments_post_not_deleted ON comments (post_id, created_at DESC) WHERE is_deleted = false")

	// Full-text search index for comment content (6.1.10)
	// Using PostgreSQL GIN index with tsvector for fast full-text search
	DB.Exec("CREATE INDEX IF NOT EXISTS idx_comments_content_search ON comments USING gin(to_tsvector('english', content)) WHERE is_deleted = false")

	// Comment mention indexes
	DB.Exec("CREATE INDEX IF NOT EXISTS idx_comment_mentions_user ON comment_mentions (mentioned_user_id)")
	DB.Exec("CREATE INDEX IF NOT EXISTS idx_comment_mentions_comment ON comment_mentions (comment_id)")

	// Report indexes
	DB.Exec("CREATE INDEX IF NOT EXISTS idx_reports_reporter ON reports (reporter_id)")
	DB.Exec("CREATE INDEX IF NOT EXISTS idx_reports_target ON reports (target_type, target_id)")
	DB.Exec("CREATE INDEX IF NOT EXISTS idx_reports_target_user ON reports (target_user_id) WHERE target_user_id IS NOT NULL")
	DB.Exec("CREATE INDEX IF NOT EXISTS idx_reports_status ON reports (status)")
	DB.Exec("CREATE UNIQUE INDEX IF NOT EXISTS idx_reports_unique ON reports (reporter_id, target_type, target_id) WHERE deleted_at IS NULL")

	// User block indexes
	DB.Exec("CREATE INDEX IF NOT EXISTS idx_user_blocks_blocker ON user_blocks (blocker_id)")
	DB.Exec("CREATE INDEX IF NOT EXISTS idx_user_blocks_blocked ON user_blocks (blocked_id)")
	DB.Exec("CREATE UNIQUE INDEX IF NOT EXISTS idx_user_blocks_unique ON user_blocks (blocker_id, blocked_id) WHERE deleted_at IS NULL")

	// Search query analytics indexes (7.1.9)
	DB.Exec("CREATE INDEX IF NOT EXISTS idx_search_queries_user ON search_queries (user_id) WHERE user_id IS NOT NULL")
	DB.Exec("CREATE INDEX IF NOT EXISTS idx_search_queries_result_type ON search_queries (result_type)")
	DB.Exec("CREATE INDEX IF NOT EXISTS idx_search_queries_created ON search_queries (created_at DESC)")

	// User preference indexes (7.2.4)
	DB.Exec("CREATE INDEX IF NOT EXISTS idx_user_preferences_user ON user_preferences (user_id)")

	// Play history indexes (7.2.1.3, 7.2.4)
	DB.Exec("CREATE INDEX IF NOT EXISTS idx_play_history_user ON play_history (user_id, created_at DESC)")
	DB.Exec("CREATE INDEX IF NOT EXISTS idx_play_history_post ON play_history (post_id, created_at DESC)")
	DB.Exec("CREATE INDEX IF NOT EXISTS idx_play_history_user_post ON play_history (user_id, post_id)")

	// Hashtag indexes (7.2.5, 7.2.6)
	DB.Exec("CREATE INDEX IF NOT EXISTS idx_hashtags_name ON hashtags (name)")
	DB.Exec("CREATE INDEX IF NOT EXISTS idx_hashtags_post_count ON hashtags (post_count DESC)")
	DB.Exec("CREATE INDEX IF NOT EXISTS idx_post_hashtags_post ON post_hashtags (post_id)")
	DB.Exec("CREATE INDEX IF NOT EXISTS idx_post_hashtags_hashtag ON post_hashtags (hashtag_id)")
	DB.Exec("CREATE UNIQUE INDEX IF NOT EXISTS idx_post_hashtags_unique ON post_hashtags (post_id, hashtag_id)")

	// Story indexes (7.5.1.1.3)
	DB.Exec("CREATE INDEX IF NOT EXISTS idx_stories_user_expires ON stories (user_id, expires_at DESC)")
	DB.Exec("CREATE INDEX IF NOT EXISTS idx_stories_expires ON stories (expires_at) WHERE expires_at > NOW()")
	DB.Exec("CREATE INDEX IF NOT EXISTS idx_story_views_story ON story_views (story_id)")
	DB.Exec("CREATE INDEX IF NOT EXISTS idx_story_views_viewer ON story_views (viewer_id)")
	DB.Exec("CREATE UNIQUE INDEX IF NOT EXISTS idx_story_views_unique ON story_views (story_id, viewer_id)")

	// Story highlight indexes (7.5.6)
	DB.Exec("CREATE INDEX IF NOT EXISTS idx_story_highlights_user ON story_highlights (user_id)")
	DB.Exec("CREATE INDEX IF NOT EXISTS idx_story_highlights_user_sort ON story_highlights (user_id, sort_order)")
	DB.Exec("CREATE INDEX IF NOT EXISTS idx_highlighted_stories_highlight ON highlighted_stories (highlight_id)")
	DB.Exec("CREATE INDEX IF NOT EXISTS idx_highlighted_stories_story ON highlighted_stories (story_id)")
	DB.Exec("CREATE UNIQUE INDEX IF NOT EXISTS idx_highlighted_stories_unique ON highlighted_stories (highlight_id, story_id)")

	return nil
}

// Close closes the database connection
func Close() error {
	if DB == nil {
		return nil
	}

	sqlDB, err := DB.DB()
	if err != nil {
		return err
	}

	return sqlDB.Close()
}

// Health checks database connectivity
func Health() error {
	if DB == nil {
		return fmt.Errorf("database not initialized")
	}

	sqlDB, err := DB.DB()
	if err != nil {
		return err
	}

	return sqlDB.Ping()
}

// getEnvOrDefault returns environment variable or default value
func getEnvOrDefault(key, defaultValue string) string {
	if value := os.Getenv(key); value != "" {
		return value
	}
	return defaultValue
}
