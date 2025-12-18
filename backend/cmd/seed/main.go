package main

import (
	"fmt"
	"log"
	"os"

	"github.com/joho/godotenv"
	"github.com/zfogg/sidechain/backend/internal/database"
	"github.com/zfogg/sidechain/backend/internal/recommendations"
	"github.com/zfogg/sidechain/backend/internal/seed"
	"github.com/zfogg/sidechain/backend/internal/stream"
)

func main() {
	// Load environment variables
	if err := godotenv.Load(); err != nil {
		log.Println("Warning: .env file not found, using system environment variables")
	}

	// Parse command
	command := "dev"
	if len(os.Args) > 1 {
		command = os.Args[1]
	}

	switch command {
	case "dev":
		seedDev()
	case "test":
		seedTest()
	case "clean":
		cleanSeed()
	default:
		fmt.Println("Usage: seed [dev|test|clean]")
		fmt.Println("  dev   - Seed development database with realistic data")
		fmt.Println("  test  - Seed test database with minimal data")
		fmt.Println("  clean - Remove all seed data (use with caution)")
		os.Exit(1)
	}
}

func seedDev() {
	log.Println("🌱 Seeding development database...")

	// Initialize database connection
	if err := database.Initialize(); err != nil {
		log.Fatalf("❌ Failed to connect to database: %v", err)
	}
	defer database.Close()

	log.Println("✅ Database connected")

	// Initialize Gorse client for recommendation syncing
	gorseAPIURL := os.Getenv("GORSE_API_URL")
	gorseAPIKey := os.Getenv("GORSE_API_KEY")

	seeder := seed.NewSeeder(database.DB)

	if gorseAPIURL != "" && gorseAPIKey != "" {
		log.Println("🔮 Initializing Gorse client for recommendation syncing...")
		gorse := recommendations.NewGorseRESTClient(gorseAPIURL, gorseAPIKey, database.DB)
		seeder.SetGorseClient(gorse)
		log.Println("✅ Gorse client configured")
	} else {
		log.Println("⚠️  GORSE_API_URL or GORSE_API_KEY not set - skipping Gorse sync")
		log.Println("   Set these environment variables to enable recommendation seeding")
	}

	// Initialize Stream.io client for conversation seeding
	log.Println("💬 Initializing Stream.io client for conversation seeding...")
	streamClient, err := stream.NewClient()
	if err != nil {
		log.Printf("⚠️  Failed to initialize Stream.io client: %v\n", err)
		log.Println("   Set STREAM_API_KEY and STREAM_API_SECRET to enable conversation seeding")
	} else {
		seeder.SetStreamClient(streamClient)
		log.Println("✅ Stream.io client configured")
	}

	// Run seed functions
	if err := seeder.SeedDev(); err != nil {
		log.Fatalf("❌ Seeding failed: %v", err)
	}

	log.Println("✅ Development database seeded successfully!")
}

func seedTest() {
	log.Println("🧪 Seeding test database...")

	// Initialize database connection
	if err := database.Initialize(); err != nil {
		log.Fatalf("❌ Failed to connect to database: %v", err)
	}
	defer database.Close()

	log.Println("✅ Database connected")

	// Initialize Stream.io client for conversation seeding
	log.Println("💬 Initializing Stream.io client for conversation seeding...")
	streamClient, err := stream.NewClient()
	if err != nil {
		log.Printf("⚠️  Failed to initialize Stream.io client: %v\n", err)
		log.Println("   Set STREAM_API_KEY and STREAM_API_SECRET to enable conversation seeding")
	}

	// Run seed functions
	seeder := seed.NewSeeder(database.DB)
	if streamClient != nil {
		seeder.SetStreamClient(streamClient)
		log.Println("✅ Stream.io client configured")
	}

	if err := seeder.SeedTest(); err != nil {
		log.Fatalf("❌ Seeding failed: %v", err)
	}

	log.Println("✅ Test database seeded successfully!")
}

func cleanSeed() {
	log.Println("🧹 Cleaning seed data...")

	// Initialize database connection
	if err := database.Initialize(); err != nil {
		log.Fatalf("❌ Failed to connect to database: %v", err)
	}
	defer database.Close()

	log.Println("✅ Database connected")

	// Clean seed data
	seeder := seed.NewSeeder(database.DB)
	if err := seeder.Clean(); err != nil {
		log.Fatalf("❌ Clean failed: %v", err)
	}

	log.Println("✅ Seed data cleaned successfully!")
}
