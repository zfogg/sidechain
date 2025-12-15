package main

import (
	"log"

	"github.com/joho/godotenv"
	"github.com/zfogg/sidechain/backend/internal/database"
	"github.com/zfogg/sidechain/backend/internal/models"
)

func main() {
	log.Println("🧹 Cleaning up invalid posts")
	log.Println("============================")
	log.Println()

	// Load environment
	if err := godotenv.Load(); err != nil {
		log.Println("Warning: .env file not found, using system environment variables")
	}

	// Connect to database
	log.Println("🔄 Connecting to database...")
	if err := database.Initialize(); err != nil {
		log.Fatalf("❌ Failed to connect to database: %v", err)
	}
	defer database.Close()
	log.Println("✅ Database connected")
	log.Println()

	// Find posts with invalid audio URLs (old test data with tech/ paths)
	var invalidPosts []models.AudioPost
	if err := database.DB.Where("audio_url LIKE ?", "%tech/%").Find(&invalidPosts).Error; err != nil {
		log.Fatalf("❌ Failed to query posts: %v", err)
	}

	if len(invalidPosts) == 0 {
		log.Println("✅ No invalid posts found")
		return
	}

	log.Printf("📊 Found %d posts with invalid audio URLs (tech/ paths)\n", len(invalidPosts))
	log.Println()

	// Show what will be deleted
	log.Println("Posts to be deleted:")
	for i, post := range invalidPosts {
		log.Printf("  [%d] %s - %s (user: %s)\n", i+1, post.ID, post.AudioURL, post.UserID)
	}
	log.Println()

	// Delete the posts
	log.Print("⚠️  Deleting invalid posts (press Ctrl+C to cancel)...")

	result := database.DB.Delete(&invalidPosts)
	if result.Error != nil {
		log.Fatalf("\n❌ Failed to delete posts: %v", result.Error)
	}

	log.Printf("\n✅ Successfully deleted %d invalid posts\n", result.RowsAffected)
	log.Println()
	log.Println("💡 Posts with proper 'uploads/' paths have been preserved")
}
