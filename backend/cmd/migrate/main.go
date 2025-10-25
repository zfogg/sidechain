package main

import (
	"fmt"
	"log"
	"os"

	"github.com/joho/godotenv"
	"github.com/zfogg/sidechain/backend/internal/database"
)

func main() {
	// Load environment variables
	if err := godotenv.Load(); err != nil {
		log.Println("Warning: .env file not found, using system environment variables")
	}

	// Parse command
	command := "up"
	if len(os.Args) > 1 {
		command = os.Args[1]
	}

	switch command {
	case "up":
		runMigrationsUp()
	case "down":
		runMigrationsDown()
	case "create":
		createMigration()
	default:
		fmt.Println("Usage: migrate [up|down|create]")
		fmt.Println("  up     - Run all pending migrations")
		fmt.Println("  down   - Rollback last migration (not implemented)")
		fmt.Println("  create - Create a new migration file (not implemented)")
		os.Exit(1)
	}
}

func runMigrationsUp() {
	log.Println("🔄 Connecting to database...")

	// Initialize database connection
	if err := database.Initialize(); err != nil {
		log.Fatalf("❌ Failed to connect to database: %v", err)
	}
	defer database.Close()

	log.Println("✅ Database connected")
	log.Println("📈 Running migrations...")

	// Run migrations
	if err := database.Migrate(); err != nil {
		log.Fatalf("❌ Migration failed: %v", err)
	}

	log.Println("✅ All migrations completed successfully!")
}

func runMigrationsDown() {
	log.Println("❌ Migration rollback not yet implemented")
	log.Println("💡 Tip: Use GORM's AutoMigrate for schema updates in development")
	os.Exit(1)
}

func createMigration() {
	if len(os.Args) < 3 {
		log.Println("❌ Migration name required")
		log.Println("Usage: migrate create <migration_name>")
		os.Exit(1)
	}

	log.Println("❌ Migration file creation not yet implemented")
	log.Println("💡 Tip: Add your model to internal/models and it will be auto-migrated")
	os.Exit(1)
}
