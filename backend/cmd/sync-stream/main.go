package main

import (
	"log"

	"github.com/joho/godotenv"
	"github.com/zfogg/sidechain/backend/internal/database"
	"github.com/zfogg/sidechain/backend/internal/models"
	"github.com/zfogg/sidechain/backend/internal/stream"
)

func main() {
	log.Println("🔄 Syncing Posts to Stream.io")
	log.Println("==============================")
	log.Println()

	// Load environment variables
	if err := godotenv.Load(); err != nil {
		log.Println("Warning: .env file not found, using system environment variables")
	}

	// Initialize database
	log.Println("🔄 Connecting to database...")
	if err := database.Initialize(); err != nil {
		log.Fatalf("❌ Failed to connect to database: %v", err)
	}
	defer database.Close()
	log.Println("✅ Database connected")
	log.Println()

	// Initialize Stream client
	log.Println("🔄 Connecting to Stream.io...")
	streamClient, err := stream.NewClient()
	if err != nil {
		log.Fatalf("❌ Failed to initialize Stream client: %v", err)
	}
	log.Println("✅ Stream.io connected")
	log.Println()

	// Find all active posts (with or without waveforms)
	var posts []models.AudioPost
	if err := database.DB.Where("deleted_at IS NULL").
		Order("created_at DESC").
		Find(&posts).Error; err != nil {
		log.Fatalf("❌ Failed to query posts: %v", err)
	}

	if len(posts) == 0 {
		log.Println("✅ No active posts to sync")
		return
	}

	log.Printf("📊 Found %d active posts to sync to Stream.io\n", len(posts))
	log.Println()

	successCount := 0
	failCount := 0

	for i, post := range posts {
		log.Printf("[%d/%d] Syncing post %s (user: %s)...", i+1, len(posts), post.ID, post.UserID)

		// Create activity with waveform URL
		activity := &stream.Activity{
			Actor:        "user:" + post.UserID,
			Verb:         "posted",
			Object:       "loop:" + post.ID,
			ForeignID:    "loop:" + post.ID,
			AudioURL:     post.AudioURL,
			BPM:          post.BPM,
			Key:          post.Key,
			DAW:          post.DAW,
			DurationBars: post.DurationBars,
			Genre:        post.Genre,
			WaveformURL:  post.WaveformURL,
		}

		if err := streamClient.CreateLoopActivity(post.UserID, activity); err != nil {
			log.Printf(" ❌ Failed: %v\n", err)
			failCount++
			continue
		}

		log.Printf(" ✅ Success\n")
		successCount++
	}

	log.Println()
	log.Printf("📊 Sync Complete: %d succeeded, %d failed\n", successCount, failCount)
	log.Println()

	// Show summary
	log.Println("✅ Posts synced to Stream.io!")
	log.Println()
	log.Println("Note: This synced posts to Stream.io with their current waveform URLs.")
	log.Println("      If posts don't have waveforms yet, run: go run cmd/backfill-waveforms/main.go")
	log.Println()
	log.Println("Next steps:")
	log.Println("  1. Restart the VST plugin or reload the feed")
	log.Println("  2. Posts should now appear in the plugin feed")
	log.Println("  3. Check plugin.log file for any errors")
}
