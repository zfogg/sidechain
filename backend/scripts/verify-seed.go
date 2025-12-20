package main

import (
	"encoding/json"
	"fmt"
	"log"
	"os"

	"github.com/joho/godotenv"
	"github.com/zfogg/sidechain/backend/internal/database"
	"github.com/zfogg/sidechain/backend/internal/models"
)

func main() {
	// Load environment variables
	if err := godotenv.Load(); err != nil {
		log.Println("Warning: .env file not found, using system environment variables")
	}

	// Initialize database connection
	if err := database.Initialize(); err != nil {
		log.Fatalf("❌ Failed to connect to database: %v", err)
	}
	defer database.Close()

	fmt.Println("🔍 Verifying seed data...")
	fmt.Println()

	// Count records
	var userCount, postCount, commentCount, hashtagCount, playHistoryCount int64

	database.DB.Model(&models.User{}).Where("deleted_at IS NULL").Count(&userCount)
	database.DB.Model(&models.AudioPost{}).Where("deleted_at IS NULL").Count(&postCount)
	database.DB.Model(&models.Comment{}).Where("deleted_at IS NULL AND is_deleted = false").Count(&commentCount)
	// GORM uses pluralized table names
	database.DB.Table("hashtags").Count(&hashtagCount)
	database.DB.Table("play_histories").Count(&playHistoryCount)

	fmt.Println("📊 Record Counts:")
	fmt.Printf("  Users:        %d\n", userCount)
	fmt.Printf("  Audio Posts:  %d\n", postCount)
	fmt.Printf("  Comments:     %d\n", commentCount)
	fmt.Printf("  Hashtags:     %d\n", hashtagCount)
	fmt.Printf("  Play History: %d\n", playHistoryCount)
	fmt.Println()

	// Sample data
	fmt.Println("📝 Sample Data:")
	fmt.Println()

	// Sample users
	var users []models.User
	database.DB.Where("deleted_at IS NULL").Limit(3).Find(&users)
	fmt.Println("  Sample Users:")
	for _, u := range users {
		fmt.Printf("    - %s (@%s) - %d posts, %d followers\n", u.DisplayName, u.Username, u.PostCount, u.FollowerCount)
	}
	fmt.Println()

	// Sample posts
	var posts []models.AudioPost
	database.DB.Where("deleted_at IS NULL").Limit(3).Find(&posts)
	fmt.Println("  Sample Posts:")
	for _, p := range posts {
		fmt.Printf("    - %s - %d BPM, Key: %s, Genre: %v\n", p.OriginalFilename, p.BPM, p.Key, p.Genre)
	}
	fmt.Println()

	// Sample comments
	var comments []models.Comment
	database.DB.Where("deleted_at IS NULL AND is_deleted = false").Limit(3).Find(&comments)
	fmt.Println("  Sample Comments:")
	for _, c := range comments {
		content := c.Content
		if len(content) > 50 {
			content = content[:50] + "..."
		}
		fmt.Printf("    - %s\n", content)
	}
	fmt.Println()

	// Hashtag usage
	var hashtags []models.Hashtag
	database.DB.Table("hashtags").Order("post_count DESC").Limit(5).Find(&hashtags)
	fmt.Println("  Top Hashtags:")
	for _, h := range hashtags {
		fmt.Printf("    - # %s (%d posts)\n", h.Name, h.PostCount)
	}
	fmt.Println()

	// User preferences
	var prefCount int64
	database.DB.Table("user_preferences").Count(&prefCount)
	fmt.Printf("  User Preferences: %d\n", prefCount)
	fmt.Println()

	// Verify relationships
	fmt.Println("🔗 Relationship Verification:")
	var postWithUser models.AudioPost
	database.DB.Preload("User").Where("deleted_at IS NULL").First(&postWithUser)
	if postWithUser.User.ID != "" {
		fmt.Printf("  ✅ Posts have user relationships\n")
	}

	var commentWithPost models.Comment
	database.DB.Preload("Post").Where("deleted_at IS NULL").First(&commentWithPost)
	if commentWithPost.Post.ID != "" {
		fmt.Printf("  ✅ Comments have post relationships\n")
	}

	var postHashtag models.PostHashtag
	database.DB.Table("post_hashtags").Preload("Hashtag").First(&postHashtag)
	if postHashtag.Hashtag.ID != "" {
		fmt.Printf("  ✅ Posts have hashtag relationships\n")
	}
	fmt.Println()

	// Export sample data as JSON for API testing
	if len(os.Args) > 1 && os.Args[1] == "--json" {
		sampleData := map[string]interface{}{
			"user_id":  users[0].ID,
			"username": users[0].Username,
			"post_id":  posts[0].ID,
		}
		jsonData, _ := json.MarshalIndent(sampleData, "", "  ")
		fmt.Println("📋 Sample IDs for API testing:")
		fmt.Println(string(jsonData))
	}

	fmt.Println("✅ Seed data verification complete!")
}
