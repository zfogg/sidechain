package database

import (
	"fmt"
	"os"
	"time"

	"github.com/zfogg/sidechain/backend/internal/logger"
	"github.com/zfogg/sidechain/backend/internal/metrics"
	"github.com/zfogg/sidechain/backend/internal/models"
	"github.com/zfogg/sidechain/backend/internal/telemetry"
	"go.uber.org/zap"
	"gorm.io/driver/postgres"
	"gorm.io/gorm"
	gormlogger "gorm.io/gorm/logger"
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
	gormLogger := gormlogger.Default
	if os.Getenv("ENVIRONMENT") == "development" {
		gormLogger = gormlogger.Default.LogMode(gormlogger.Info)
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

	// METRICS-1: Register GORM hooks for database query metrics
	registerMetricsHooks(db)

	// Register OpenTelemetry tracing plugin (if enabled)
	if os.Getenv("OTEL_ENABLED") == "true" {
		if err := db.Use(telemetry.GORMTracingPlugin()); err != nil {
			logger.Log.Warn("Failed to register GORM tracing plugin", zap.Error(err))
		} else {
			logger.Log.Info("OpenTelemetry GORM tracing plugin registered")
		}
	}

	logger.Log.Info("Database connected successfully")

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
		logger.Log.Warn("Could not create uuid-ossp extension", zap.Error(err))
	}

	// Run manual migrations before AutoMigrate
	runManualMigrations()

	// Auto-migrate all models
	// Note: MIDIPattern must come before AudioPost and Story due to foreign key references
	err = DB.AutoMigrate(
		&models.User{},
		&models.OAuthProvider{}, // OAuth account linking for email-based unification
		&models.MIDIPattern{},   // Must come before AudioPost and Story (foreign key dependency)
		&models.AudioPost{},
		&models.ProjectFile{}, // Project File Exchange
		&models.PasswordReset{},
		&models.Comment{},
		&models.CommentMention{},
		&models.Report{},
		&models.UserBlock{},
		&models.FollowRequest{}, // Private accounts follow request system
		&models.SearchQuery{},
		&models.UserPreference{},
		&models.PlayHistory{},
		&models.Hashtag{},
		&models.PostHashtag{},
		&models.Story{},
		&models.StoryView{},
		&models.StoryHighlight{},
		&models.HighlightedStory{},
		&models.Playlist{},                 // Collaborative Playlists
		&models.PlaylistEntry{},            // Collaborative Playlists
		&models.PlaylistCollaborator{},     // Collaborative Playlists
		&models.MIDIChallenge{},            // MIDI Battle Royale
		&models.MIDIChallengeEntry{},       // MIDI Battle Royale (references AudioPost and MIDIPattern)
		&models.MIDIChallengeVote{},        // MIDI Battle Royale
		&models.SavedPost{},                // Save/Bookmark posts feature
		&models.Repost{},                   // Repost/Share to feed feature
		&models.NotificationPreferences{},  // Notification preferences per user
		&models.MutedUser{},                // Mute users without unfollowing
		&models.Sound{},                    // Sound/Sample Pages
		&models.AudioFingerprint{},         // Audio fingerprinting for sound detection
		&models.SoundUsage{},               // Track sound usage across posts
		&models.ErrorLog{},                 // Error tracking and reporting
		&models.RecommendationImpression{}, // CTR tracking - impressions
		&models.RecommendationClick{},      // CTR tracking - clicks
	)
	if err != nil {
		return fmt.Errorf("failed to run migrations: %w", err)
	}

	// Create indexes for performance
	err = createIndexes()
	if err != nil {
		return fmt.Errorf("failed to create indexes: %w", err)
	}

	logger.Log.Info("Database migrations completed")
	return nil
}

// execIndex executes an index creation statement and logs any errors
func execIndex(sql string) {
	if err := DB.Exec(sql).Error; err != nil {
		// Truncate SQL for logging to avoid huge log messages
		truncatedSQL := sql
		if len(sql) > 80 {
			truncatedSQL = sql[:80] + "..."
		}
		logger.Log.Warn("Failed to create index", zap.Error(err), zap.String("sql", truncatedSQL))
	}
}

// createIndexes creates performance indexes
func createIndexes() error {
	// User indexes
	execIndex("CREATE INDEX IF NOT EXISTS idx_users_email_lower ON users (LOWER(email))")
	execIndex("CREATE INDEX IF NOT EXISTS idx_users_username_lower ON users (LOWER(username))")
	execIndex("CREATE INDEX IF NOT EXISTS idx_users_stream_user_id ON users (stream_user_id)")

	// OAuth provider indexes
	execIndex("CREATE UNIQUE INDEX IF NOT EXISTS idx_oauth_providers_unique ON oauth_providers (provider, provider_user_id)")

	// AudioPost indexes for feed queries
	execIndex("CREATE INDEX IF NOT EXISTS idx_audio_posts_user_created ON audio_posts (user_id, created_at DESC)")
	execIndex("CREATE INDEX IF NOT EXISTS idx_audio_posts_public_created ON audio_posts (is_public, created_at DESC)")
	execIndex("CREATE INDEX IF NOT EXISTS idx_audio_posts_bpm ON audio_posts (bpm) WHERE bpm > 0")
	execIndex("CREATE INDEX IF NOT EXISTS idx_audio_posts_genre ON audio_posts USING GIN (genre)")
	execIndex("CREATE INDEX IF NOT EXISTS idx_audio_posts_archived ON audio_posts (user_id, is_archived, created_at DESC)")
	execIndex("CREATE INDEX IF NOT EXISTS idx_audio_posts_not_archived ON audio_posts (user_id, created_at DESC) WHERE is_archived = false")

	// Device indexes
	execIndex("CREATE INDEX IF NOT EXISTS idx_devices_fingerprint ON devices (device_fingerprint)")
	execIndex("CREATE INDEX IF NOT EXISTS idx_devices_user_active ON devices (user_id, is_active)")

	// OAuth provider indexes
	execIndex("CREATE INDEX IF NOT EXISTS idx_oauth_providers_unique ON oauth_providers (provider, provider_user_id)")
	execIndex("CREATE INDEX IF NOT EXISTS idx_oauth_providers_email ON oauth_providers (email)")

	// Comment indexes for efficient retrieval
	execIndex("CREATE INDEX IF NOT EXISTS idx_comments_post_created ON comments (post_id, created_at DESC)")
	execIndex("CREATE INDEX IF NOT EXISTS idx_comments_user ON comments (user_id)")
	execIndex("CREATE INDEX IF NOT EXISTS idx_comments_parent ON comments (parent_id) WHERE parent_id IS NOT NULL")
	execIndex("CREATE INDEX IF NOT EXISTS idx_comments_post_not_deleted ON comments (post_id, created_at DESC) WHERE is_deleted = false")

	// Full-text search index for comment content
	// Using PostgreSQL GIN index with tsvector for fast full-text search
	execIndex("CREATE INDEX IF NOT EXISTS idx_comments_content_search ON comments USING gin(to_tsvector('english', content)) WHERE is_deleted = false")

	// Comment mention indexes
	execIndex("CREATE INDEX IF NOT EXISTS idx_comment_mentions_user ON comment_mentions (mentioned_user_id)")
	execIndex("CREATE INDEX IF NOT EXISTS idx_comment_mentions_comment ON comment_mentions (comment_id)")

	// Report indexes
	execIndex("CREATE INDEX IF NOT EXISTS idx_reports_reporter ON reports (reporter_id)")
	execIndex("CREATE INDEX IF NOT EXISTS idx_reports_target ON reports (target_type, target_id)")
	execIndex("CREATE INDEX IF NOT EXISTS idx_reports_target_user ON reports (target_user_id) WHERE target_user_id IS NOT NULL")
	execIndex("CREATE INDEX IF NOT EXISTS idx_reports_status ON reports (status)")
	execIndex("CREATE UNIQUE INDEX IF NOT EXISTS idx_reports_unique ON reports (reporter_id, target_type, target_id) WHERE deleted_at IS NULL")

	// User block indexes
	execIndex("CREATE INDEX IF NOT EXISTS idx_user_blocks_blocker ON user_blocks (blocker_id)")
	execIndex("CREATE INDEX IF NOT EXISTS idx_user_blocks_blocked ON user_blocks (blocked_id)")
	execIndex("CREATE UNIQUE INDEX IF NOT EXISTS idx_user_blocks_unique ON user_blocks (blocker_id, blocked_id) WHERE deleted_at IS NULL")

	// Search query analytics indexes
	execIndex("CREATE INDEX IF NOT EXISTS idx_search_queries_user ON search_queries (user_id) WHERE user_id IS NOT NULL")
	execIndex("CREATE INDEX IF NOT EXISTS idx_search_queries_result_type ON search_queries (result_type)")
	execIndex("CREATE INDEX IF NOT EXISTS idx_search_queries_created ON search_queries (created_at DESC)")

	// User preference indexes
	execIndex("CREATE INDEX IF NOT EXISTS idx_user_preferences_user ON user_preferences (user_id)")

	// Play history indexes
	execIndex("CREATE INDEX IF NOT EXISTS idx_play_history_user ON play_history (user_id, created_at DESC)")
	execIndex("CREATE INDEX IF NOT EXISTS idx_play_history_post ON play_history (post_id, created_at DESC)")
	execIndex("CREATE INDEX IF NOT EXISTS idx_play_history_user_post ON play_history (user_id, post_id)")

	// Hashtag indexes
	execIndex("CREATE INDEX IF NOT EXISTS idx_hashtags_name ON hashtags (name)")
	execIndex("CREATE INDEX IF NOT EXISTS idx_hashtags_post_count ON hashtags (post_count DESC)")
	execIndex("CREATE INDEX IF NOT EXISTS idx_post_hashtags_post ON post_hashtags (post_id)")
	execIndex("CREATE INDEX IF NOT EXISTS idx_post_hashtags_hashtag ON post_hashtags (hashtag_id)")
	execIndex("CREATE UNIQUE INDEX IF NOT EXISTS idx_post_hashtags_unique ON post_hashtags (post_id, hashtag_id)")

	// Story indexes
	execIndex("CREATE INDEX IF NOT EXISTS idx_stories_user_expires ON stories (user_id, expires_at DESC)")
	execIndex("CREATE INDEX IF NOT EXISTS idx_stories_expires ON stories (expires_at) WHERE expires_at > NOW()")
	execIndex("CREATE INDEX IF NOT EXISTS idx_story_views_story ON story_views (story_id)")
	execIndex("CREATE INDEX IF NOT EXISTS idx_story_views_viewer ON story_views (viewer_id)")
	execIndex("CREATE UNIQUE INDEX IF NOT EXISTS idx_story_views_unique ON story_views (story_id, viewer_id)")

	// Story highlight indexes
	execIndex("CREATE INDEX IF NOT EXISTS idx_story_highlights_user ON story_highlights (user_id)")
	execIndex("CREATE INDEX IF NOT EXISTS idx_story_highlights_user_sort ON story_highlights (user_id, sort_order)")
	execIndex("CREATE INDEX IF NOT EXISTS idx_highlighted_stories_highlight ON highlighted_stories (highlight_id)")
	execIndex("CREATE INDEX IF NOT EXISTS idx_highlighted_stories_story ON highlighted_stories (story_id)")
	execIndex("CREATE UNIQUE INDEX IF NOT EXISTS idx_highlighted_stories_unique ON highlighted_stories (highlight_id, story_id)")

	// MIDI pattern indexes
	execIndex("CREATE INDEX IF NOT EXISTS idx_midi_patterns_user ON midi_patterns (user_id)")
	execIndex("CREATE INDEX IF NOT EXISTS idx_midi_patterns_public ON midi_patterns (is_public) WHERE is_public = true")
	execIndex("CREATE INDEX IF NOT EXISTS idx_midi_patterns_user_created ON midi_patterns (user_id, created_at DESC)")
	execIndex("CREATE INDEX IF NOT EXISTS idx_midi_patterns_downloads ON midi_patterns (download_count DESC) WHERE is_public = true")

	// Project file indexes
	execIndex("CREATE INDEX IF NOT EXISTS idx_project_files_user ON project_files (user_id)")
	execIndex("CREATE INDEX IF NOT EXISTS idx_project_files_user_created ON project_files (user_id, created_at DESC)")
	execIndex("CREATE INDEX IF NOT EXISTS idx_project_files_daw_type ON project_files (daw_type)")
	execIndex("CREATE INDEX IF NOT EXISTS idx_project_files_post ON project_files (audio_post_id) WHERE audio_post_id IS NOT NULL")
	execIndex("CREATE INDEX IF NOT EXISTS idx_project_files_public ON project_files (is_public) WHERE is_public = true")

	// Playlist indexes
	execIndex("CREATE INDEX IF NOT EXISTS idx_playlists_owner ON playlists (owner_id)")
	execIndex("CREATE INDEX IF NOT EXISTS idx_playlists_public ON playlists (is_public) WHERE is_public = true")
	execIndex("CREATE INDEX IF NOT EXISTS idx_playlists_owner_created ON playlists (owner_id, created_at DESC)")
	execIndex("CREATE INDEX IF NOT EXISTS idx_playlist_entries_playlist ON playlist_entries (playlist_id)")
	execIndex("CREATE INDEX IF NOT EXISTS idx_playlist_entries_post ON playlist_entries (post_id)")
	execIndex("CREATE INDEX IF NOT EXISTS idx_playlist_entries_playlist_position ON playlist_entries (playlist_id, position)")
	execIndex("CREATE INDEX IF NOT EXISTS idx_playlist_collaborators_playlist ON playlist_collaborators (playlist_id)")
	execIndex("CREATE INDEX IF NOT EXISTS idx_playlist_collaborators_user ON playlist_collaborators (user_id)")
	execIndex("CREATE UNIQUE INDEX IF NOT EXISTS idx_playlist_collaborators_unique ON playlist_collaborators (playlist_id, user_id) WHERE deleted_at IS NULL")

	// MIDI challenge indexes
	execIndex("CREATE INDEX IF NOT EXISTS idx_midi_challenges_start_date ON midi_challenges (start_date)")
	execIndex("CREATE INDEX IF NOT EXISTS idx_midi_challenges_end_date ON midi_challenges (end_date)")
	execIndex("CREATE INDEX IF NOT EXISTS idx_midi_challenges_voting_end_date ON midi_challenges (voting_end_date)")
	execIndex("CREATE INDEX IF NOT EXISTS idx_midi_challenges_created ON midi_challenges (created_at DESC)")
	execIndex("CREATE INDEX IF NOT EXISTS idx_midi_challenge_entries_challenge ON midi_challenge_entries (challenge_id)")
	execIndex("CREATE INDEX IF NOT EXISTS idx_midi_challenge_entries_user ON midi_challenge_entries (user_id)")
	execIndex("CREATE INDEX IF NOT EXISTS idx_midi_challenge_entries_vote_count ON midi_challenge_entries (challenge_id, vote_count DESC)")
	execIndex("CREATE INDEX IF NOT EXISTS idx_midi_challenge_entries_post ON midi_challenge_entries (post_id) WHERE post_id IS NOT NULL")
	execIndex("CREATE INDEX IF NOT EXISTS idx_midi_challenge_entries_midi ON midi_challenge_entries (midi_pattern_id) WHERE midi_pattern_id IS NOT NULL")
	execIndex("CREATE UNIQUE INDEX IF NOT EXISTS idx_midi_challenge_entries_user_challenge ON midi_challenge_entries (user_id, challenge_id) WHERE deleted_at IS NULL")
	execIndex("CREATE INDEX IF NOT EXISTS idx_midi_challenge_votes_challenge ON midi_challenge_votes (challenge_id)")
	execIndex("CREATE INDEX IF NOT EXISTS idx_midi_challenge_votes_entry ON midi_challenge_votes (entry_id)")
	execIndex("CREATE INDEX IF NOT EXISTS idx_midi_challenge_votes_voter ON midi_challenge_votes (voter_id)")
	execIndex("CREATE UNIQUE INDEX IF NOT EXISTS idx_midi_challenge_votes_unique ON midi_challenge_votes (challenge_id, voter_id) WHERE deleted_at IS NULL")

	// Saved posts indexes
	execIndex("CREATE INDEX IF NOT EXISTS idx_saved_posts_user ON saved_posts (user_id)")
	execIndex("CREATE INDEX IF NOT EXISTS idx_saved_posts_post ON saved_posts (post_id)")
	execIndex("CREATE INDEX IF NOT EXISTS idx_saved_posts_user_created ON saved_posts (user_id, created_at DESC)")
	execIndex("CREATE UNIQUE INDEX IF NOT EXISTS idx_saved_posts_unique ON saved_posts (user_id, post_id)")

	// Repost indexes
	execIndex("CREATE INDEX IF NOT EXISTS idx_reposts_user ON reposts (user_id)")
	execIndex("CREATE INDEX IF NOT EXISTS idx_reposts_original_post ON reposts (original_post_id)")
	execIndex("CREATE INDEX IF NOT EXISTS idx_reposts_user_created ON reposts (user_id, created_at DESC)")
	execIndex("CREATE UNIQUE INDEX IF NOT EXISTS idx_reposts_unique ON reposts (user_id, original_post_id) WHERE deleted_at IS NULL")

	// Notification preferences indexes
	execIndex("CREATE UNIQUE INDEX IF NOT EXISTS idx_notification_preferences_user ON notification_preferences (user_id) WHERE deleted_at IS NULL")

	// Muted users indexes
	execIndex("CREATE INDEX IF NOT EXISTS idx_muted_users_user ON muted_users (user_id)")
	execIndex("CREATE INDEX IF NOT EXISTS idx_muted_users_muted ON muted_users (muted_user_id)")
	execIndex("CREATE UNIQUE INDEX IF NOT EXISTS idx_muted_users_unique ON muted_users (user_id, muted_user_id)")

	// Sound/Sample Pages indexes
	execIndex("CREATE INDEX IF NOT EXISTS idx_sounds_fingerprint_hash ON sounds (fingerprint_hash)")
	execIndex("CREATE INDEX IF NOT EXISTS idx_sounds_creator ON sounds (creator_id)")
	execIndex("CREATE INDEX IF NOT EXISTS idx_sounds_trending ON sounds (is_trending, trending_rank DESC) WHERE is_trending = true")
	execIndex("CREATE INDEX IF NOT EXISTS idx_sounds_usage_count ON sounds (usage_count DESC)")
	execIndex("CREATE INDEX IF NOT EXISTS idx_sounds_created ON sounds (created_at DESC)")
	execIndex("CREATE INDEX IF NOT EXISTS idx_sounds_original_post ON sounds (original_post_id) WHERE original_post_id IS NOT NULL")

	// Audio fingerprint indexes
	execIndex("CREATE INDEX IF NOT EXISTS idx_audio_fingerprints_hash ON audio_fingerprints (fingerprint_hash)")
	execIndex("CREATE INDEX IF NOT EXISTS idx_audio_fingerprints_sound ON audio_fingerprints (sound_id) WHERE sound_id IS NOT NULL")
	execIndex("CREATE INDEX IF NOT EXISTS idx_audio_fingerprints_post ON audio_fingerprints (audio_post_id) WHERE audio_post_id IS NOT NULL")
	execIndex("CREATE INDEX IF NOT EXISTS idx_audio_fingerprints_story ON audio_fingerprints (story_id) WHERE story_id IS NOT NULL")
	execIndex("CREATE INDEX IF NOT EXISTS idx_audio_fingerprints_status ON audio_fingerprints (status)")
	execIndex("CREATE UNIQUE INDEX IF NOT EXISTS idx_audio_fingerprints_post_unique ON audio_fingerprints (audio_post_id) WHERE audio_post_id IS NOT NULL AND deleted_at IS NULL")

	// Sound usage indexes
	execIndex("CREATE INDEX IF NOT EXISTS idx_sound_usages_sound ON sound_usages (sound_id)")
	execIndex("CREATE INDEX IF NOT EXISTS idx_sound_usages_user ON sound_usages (user_id)")
	execIndex("CREATE INDEX IF NOT EXISTS idx_sound_usages_post ON sound_usages (audio_post_id) WHERE audio_post_id IS NOT NULL")
	execIndex("CREATE INDEX IF NOT EXISTS idx_sound_usages_sound_created ON sound_usages (sound_id, created_at DESC)")

	// Error log indexes
	execIndex("CREATE INDEX IF NOT EXISTS idx_error_logs_user ON error_logs (user_id)")
	execIndex("CREATE INDEX IF NOT EXISTS idx_error_logs_severity ON error_logs (severity)")
	execIndex("CREATE INDEX IF NOT EXISTS idx_error_logs_source ON error_logs (source)")
	execIndex("CREATE INDEX IF NOT EXISTS idx_error_logs_user_created ON error_logs (user_id, created_at DESC)")
	execIndex("CREATE INDEX IF NOT EXISTS idx_error_logs_user_message_source ON error_logs (user_id, message, source)")
	execIndex("CREATE INDEX IF NOT EXISTS idx_error_logs_unresolved ON error_logs (user_id, is_resolved, created_at DESC) WHERE is_resolved = false")

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

// runManualMigrations runs SQL migrations that can't be done with AutoMigrate
func runManualMigrations() {
	// Migration: Rename avatar_url to oauth_profile_picture_url in users table
	// This is a one-time migration - check if old column exists before renaming
	var columnExists bool
	DB.Raw(`
		SELECT EXISTS (
			SELECT FROM information_schema.columns
			WHERE table_name = 'users' AND column_name = 'avatar_url'
		)
	`).Scan(&columnExists)

	if columnExists {
		logger.Log.Info("Running migration: Rename avatar_url to oauth_profile_picture_url")
		err := DB.Exec("ALTER TABLE users RENAME COLUMN avatar_url TO oauth_profile_picture_url").Error
		if err != nil {
			logger.Log.Warn("Could not rename avatar_url column", zap.Error(err))
		} else {
			logger.Log.Info("Renamed avatar_url to oauth_profile_picture_url")
		}
	}

	// Migration: Rename avatar_url to oauth_profile_picture_url in oauth_providers table
	DB.Raw(`
		SELECT EXISTS (
			SELECT FROM information_schema.columns
			WHERE table_name = 'oauth_providers' AND column_name = 'avatar_url'
		)
	`).Scan(&columnExists)

	if columnExists {
		logger.Log.Info("Running migration: Rename avatar_url to oauth_profile_picture_url in oauth_providers")
		err := DB.Exec("ALTER TABLE oauth_providers RENAME COLUMN avatar_url TO oauth_profile_picture_url").Error
		if err != nil {
			logger.Log.Warn("Could not rename avatar_url column in oauth_providers", zap.Error(err))
		} else {
			logger.Log.Info("Renamed avatar_url to oauth_profile_picture_url in oauth_providers")
		}
	}

	// Migration: Populate empty filename fields with original_filename
	// Check if there are any posts with empty filenames
	var emptyFilenameCount int64
	DB.Model(&models.AudioPost{}).Where("filename IS NULL OR filename = ''").Count(&emptyFilenameCount)

	if emptyFilenameCount > 0 {
		logger.Log.Info("Running migration: Populating empty filenames in audio_posts", zap.Int64("count", emptyFilenameCount))
		err := DB.Exec("UPDATE audio_posts SET filename = original_filename WHERE filename IS NULL OR filename = ''").Error
		if err != nil {
			logger.Log.Warn("Could not populate filenames in audio_posts", zap.Error(err))
		} else {
			logger.Log.Info("Populated filenames in audio_posts", zap.Int64("count", emptyFilenameCount))
		}
	}

	// Migration: Populate empty filename fields in stories
	var emptyStoryFilenameCount int64
	DB.Model(&models.Story{}).Where("(filename IS NULL OR filename = '') AND audio_url IS NOT NULL AND audio_url != ''").Count(&emptyStoryFilenameCount)

	if emptyStoryFilenameCount > 0 {
		logger.Log.Info("Running migration: Populating empty filenames in stories", zap.Int64("count", emptyStoryFilenameCount))
		err := DB.Exec("UPDATE stories SET filename = 'story_audio.mp3' WHERE (filename IS NULL OR filename = '') AND audio_url IS NOT NULL AND audio_url != ''").Error
		if err != nil {
			logger.Log.Warn("Could not populate filenames in stories", zap.Error(err))
		} else {
			logger.Log.Info("Populated filenames in stories", zap.Int64("count", emptyStoryFilenameCount))
		}
	}

	// Migration: Replace fake CDN URLs with real test audio URLs
	// Check if there are any posts with fake sidechain.app CDN URLs
	var fakeCdnCount int64
	DB.Model(&models.AudioPost{}).Where("audio_url LIKE 'https://cdn.sidechain.app/%'").Count(&fakeCdnCount)

	if fakeCdnCount > 0 {
		logger.Log.Info("Running migration: Replacing fake CDN URLs with real test audio", zap.Int64("count", fakeCdnCount))

		// Working test audio URLs (verified HTTP 200, kozco.com)
		testAudioURLs := []string{
			"https://www.kozco.com/tech/piano2.wav",
			"https://www.kozco.com/tech/organfinale.wav",
			"https://www.kozco.com/tech/LRMonoPhase4.wav",
			"https://www.kozco.com/tech/LRMonoPhaset4.wav",
			"https://www.kozco.com/tech/WAV-MP3.wav",
			"https://www.kozco.com/tech/c304-2.wav",
			"https://www.kozco.com/tech/32.mp3",
			"https://www.kozco.com/tech/LRMonoPhase4.mp3",
			"https://www.kozco.com/tech/organfinale.mp3",
			"https://www.kozco.com/tech/piano2-CoolEdit.mp3",
		}

		// Update all posts with fake URLs to use real ones (distributed randomly)
		var posts []models.AudioPost
		DB.Where("audio_url LIKE 'https://cdn.sidechain.app/%'").Find(&posts)

		for i, post := range posts {
			audioURL := testAudioURLs[i%len(testAudioURLs)]
			DB.Model(&post).Update("audio_url", audioURL)
		}

		logger.Log.Info("Replaced fake CDN URLs with real test audio", zap.Int64("count", fakeCdnCount))
	}

	// Replace broken freesound.org URLs with working kozco.com URLs
	var brokenCount int64
	DB.Model(&models.AudioPost{}).Where("audio_url LIKE 'https://cdn.freesound.org/%'").Count(&brokenCount)
	if brokenCount > 0 {
		logger.Log.Info("Running migration: Replacing broken freesound.org URLs with working audio", zap.Int64("count", brokenCount))

		workingAudioURLs := []string{
			"https://www.kozco.com/tech/piano2.wav",
			"https://www.kozco.com/tech/organfinale.wav",
			"https://www.kozco.com/tech/LRMonoPhase4.wav",
			"https://www.kozco.com/tech/LRMonoPhaset4.wav",
			"https://www.kozco.com/tech/WAV-MP3.wav",
			"https://www.kozco.com/tech/c304-2.wav",
			"https://www.kozco.com/tech/32.mp3",
			"https://www.kozco.com/tech/LRMonoPhase4.mp3",
			"https://www.kozco.com/tech/organfinale.mp3",
			"https://www.kozco.com/tech/piano2-CoolEdit.mp3",
		}

		var posts []models.AudioPost
		DB.Where("audio_url LIKE 'https://cdn.freesound.org/%'").Find(&posts)

		for i, post := range posts {
			audioURL := workingAudioURLs[i%len(workingAudioURLs)]
			DB.Model(&post).Update("audio_url", audioURL)
		}

		logger.Log.Info("Replaced broken URLs with working audio", zap.Int64("count", brokenCount))
	}
}

// registerMetricsHooks registers GORM callbacks to record database metrics
func registerMetricsHooks(db *gorm.DB) {
	// METRICS-1: Record database query timing using GORM Before/After callbacks
	db.Callback().Create().Before("gorm:before_create").Register("metrics:before_create", func(db *gorm.DB) {
		db.InstanceSet("metrics:start_time", time.Now())
	})

	db.Callback().Create().After("gorm:after_create").Register("metrics:after_create", func(db *gorm.DB) {
		if start, ok := db.InstanceGet("metrics:start_time"); ok {
			duration := time.Since(start.(time.Time)).Seconds()
			metrics.Get().DatabaseQueryDuration.WithLabelValues("create", "insert").Observe(duration)
			status := "success"
			if db.Error != nil {
				status = "error"
			}
			metrics.Get().DatabaseQueriesTotal.WithLabelValues("create", "insert", status).Inc()
		}
	})

	db.Callback().Query().Before("gorm:before_query").Register("metrics:before_query", func(db *gorm.DB) {
		db.InstanceSet("metrics:start_time", time.Now())
	})

	db.Callback().Query().After("gorm:after_query").Register("metrics:after_query", func(db *gorm.DB) {
		if start, ok := db.InstanceGet("metrics:start_time"); ok {
			duration := time.Since(start.(time.Time)).Seconds()
			metrics.Get().DatabaseQueryDuration.WithLabelValues("query", "select").Observe(duration)
			status := "success"
			if db.Error != nil && db.Error != gorm.ErrRecordNotFound {
				status = "error"
			}
			metrics.Get().DatabaseQueriesTotal.WithLabelValues("query", "select", status).Inc()
		}
	})

	db.Callback().Update().Before("gorm:before_update").Register("metrics:before_update", func(db *gorm.DB) {
		db.InstanceSet("metrics:start_time", time.Now())
	})

	db.Callback().Update().After("gorm:after_update").Register("metrics:after_update", func(db *gorm.DB) {
		if start, ok := db.InstanceGet("metrics:start_time"); ok {
			duration := time.Since(start.(time.Time)).Seconds()
			metrics.Get().DatabaseQueryDuration.WithLabelValues("update", "update").Observe(duration)
			status := "success"
			if db.Error != nil {
				status = "error"
			}
			metrics.Get().DatabaseQueriesTotal.WithLabelValues("update", "update", status).Inc()
		}
	})

	db.Callback().Delete().Before("gorm:before_delete").Register("metrics:before_delete", func(db *gorm.DB) {
		db.InstanceSet("metrics:start_time", time.Now())
	})

	db.Callback().Delete().After("gorm:after_delete").Register("metrics:after_delete", func(db *gorm.DB) {
		if start, ok := db.InstanceGet("metrics:start_time"); ok {
			duration := time.Since(start.(time.Time)).Seconds()
			metrics.Get().DatabaseQueryDuration.WithLabelValues("delete", "delete").Observe(duration)
			status := "success"
			if db.Error != nil {
				status = "error"
			}
			metrics.Get().DatabaseQueriesTotal.WithLabelValues("delete", "delete", status).Inc()
		}
	})
}
