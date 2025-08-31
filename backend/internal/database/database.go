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