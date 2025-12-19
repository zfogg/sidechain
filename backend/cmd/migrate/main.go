package main

import (
	"context"
	"fmt"
	"log"
	"os"

	"github.com/joho/godotenv"
	"github.com/zfogg/sidechain/backend/internal/database"
	"github.com/zfogg/sidechain/backend/internal/search"
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
	case "index-elasticsearch":
		indexElasticsearch()
	default:
		fmt.Println("Usage: migrate [up|down|create|index-elasticsearch]")
		fmt.Println("  up                     - Run all pending migrations")
		fmt.Println("  down                   - Rollback last migration (not implemented)")
		fmt.Println("  create                 - Create a new migration file (not implemented)")
		fmt.Println("  index-elasticsearch    - Backfill Elasticsearch indices with existing data")
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

// indexElasticsearch backfills Elasticsearch indices with existing database data
func indexElasticsearch() {
	log.Println("🔄 Phase 0.8: Backfilling Elasticsearch indices...")

	// Initialize database connection
	if err := database.Initialize(); err != nil {
		log.Fatalf("❌ Failed to connect to database: %v", err)
	}
	defer database.Close()
	log.Println("✅ Database connected")

	// Initialize Elasticsearch client
	searchClient, err := search.NewClient()
	if err != nil {
		log.Fatalf("❌ Failed to connect to Elasticsearch: %v", err)
	}
	log.Println("✅ Elasticsearch connected")

	ctx := context.Background()

	// Create indices with mappings
	log.Println("📊 Creating Elasticsearch indices...")
	if err := searchClient.InitializeIndices(ctx); err != nil {
		log.Printf("⚠️  Note: Indices may already exist (non-fatal): %v\n", err)
	} else {
		log.Println("✅ Elasticsearch indices created successfully")
	}

	log.Println("\n✨ Phase 0.8 complete!")
	log.Println("📝 TODO: Implement backfill of users, posts, and stories from database")
	log.Println("💡 This will be implemented in Phase 1 when entity creation handlers are updated")
}
