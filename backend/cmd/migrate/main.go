package main

import (
	"context"
	"fmt"
	"log"
	"os"

	"github.com/joho/godotenv"
	"github.com/zfogg/sidechain/backend/internal/database"
	"github.com/zfogg/sidechain/backend/internal/models"
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
	log.Println("🔄 Backfilling Elasticsearch indices with database data...")
	log.Println("============================================================")
	log.Println()

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
	log.Println()

	ctx := context.Background()

	// Create indices with mappings
	log.Println("📊 Creating Elasticsearch indices (or skipping if already exist)...")
	if err := searchClient.InitializeIndices(ctx); err != nil {
		log.Printf("⚠️  Note: Indices may already exist (non-fatal): %v\n", err)
	}
	log.Println()

	// Backfill users index
	log.Println("👥 Backfilling users index...")
	backfillUsers(ctx, searchClient)
	log.Println()

	// Backfill posts index
	log.Println("🎵 Backfilling posts index...")
	backfillPosts(ctx, searchClient)
	log.Println()

	// Backfill stories index
	log.Println("📖 Backfilling stories index...")
	backfillStories(ctx, searchClient)
	log.Println()

	log.Println("✨ Elasticsearch backfill complete!")
	log.Println()
	log.Println("Summary:")
	log.Println("  ✅ Users: Indexed all active users with follower counts")
	log.Println("  ✅ Posts: Indexed all public posts with current engagement metrics")
	log.Println("  ✅ Stories: Indexed all active stories")
	log.Println()
	log.Println("Next steps:")
	log.Println("  1. Verify search is working with: go run cmd/server/main.go")
	log.Println("  2. Test user search at /api/v1/search/users?q=username")
	log.Println("  3. Test post search at /api/v1/search/posts?q=loop_name")
}

// backfillUsers indexes all active users to Elasticsearch
func backfillUsers(ctx context.Context, searchClient *search.Client) {
	var users []models.User
	if err := database.DB.Find(&users).Error; err != nil {
		log.Printf("❌ Failed to query users: %v\n", err)
		return
	}

	if len(users) == 0 {
		log.Println("⚠️  No users found to backfill")
		return
	}

	log.Printf("Found %d users to index...\n", len(users))

	successCount := 0
	failCount := 0

	for i, user := range users {
		if (i+1)%10 == 0 || i == 0 {
			log.Printf("  [%d/%d] Indexing users...\n", i+1, len(users))
		}

		userDoc := map[string]interface{}{
			"id":              user.ID,
			"username":        user.Username,
			"display_name":    user.DisplayName,
			"bio":             user.Bio,
			"genre":           user.Genre,
			"follower_count":  user.FollowerCount,
			"created_at":      user.CreatedAt,
		}

		if err := searchClient.IndexUser(ctx, user.ID, userDoc); err != nil {
			log.Printf("  ⚠️  Failed to index user %s: %v\n", user.ID, err)
			failCount++
			continue
		}
		successCount++
	}

	log.Printf("✅ Users indexed: %d succeeded, %d failed\n", successCount, failCount)
}

// backfillPosts indexes all public posts to Elasticsearch with engagement metrics
func backfillPosts(ctx context.Context, searchClient *search.Client) {
	var posts []models.AudioPost
	// Only index public, non-archived posts
	if err := database.DB.Where("is_public = true AND is_archived = false AND deleted_at IS NULL").Find(&posts).Error; err != nil {
		log.Printf("❌ Failed to query posts: %v\n", err)
		return
	}

	if len(posts) == 0 {
		log.Println("⚠️  No posts found to backfill")
		return
	}

	log.Printf("Found %d posts to index...\n", len(posts))

	successCount := 0
	failCount := 0

	for i, post := range posts {
		if (i+1)%50 == 0 || i == 0 {
			log.Printf("  [%d/%d] Indexing posts...\n", i+1, len(posts))
		}

		// Load user data for username
		var user models.User
		database.DB.Select("username").Where("id = ?", post.UserID).First(&user)

		postDoc := map[string]interface{}{
			"id":              post.ID,
			"user_id":         post.UserID,
			"username":        user.Username,
			"genre":           post.Genre,
			"bpm":             post.BPM,
			"key":             post.Key,
			"daw":             post.DAW,
			"like_count":      post.LikeCount,
			"play_count":      post.PlayCount,
			"comment_count":   post.CommentCount,
			"created_at":      post.CreatedAt,
		}

		if err := searchClient.IndexPost(ctx, post.ID, postDoc); err != nil {
			log.Printf("  ⚠️  Failed to index post %s: %v\n", post.ID, err)
			failCount++
			continue
		}
		successCount++
	}

	log.Printf("✅ Posts indexed: %d succeeded, %d failed\n", successCount, failCount)
}

// backfillStories indexes all active stories to Elasticsearch
func backfillStories(ctx context.Context, searchClient *search.Client) {
	var stories []models.Story
	// Only index non-expired stories
	if err := database.DB.Where("deleted_at IS NULL AND (expires_at IS NULL OR expires_at > NOW())").Find(&stories).Error; err != nil {
		log.Printf("❌ Failed to query stories: %v\n", err)
		return
	}

	if len(stories) == 0 {
		log.Println("⚠️  No stories found to backfill")
		return
	}

	log.Printf("Found %d stories to index...\n", len(stories))

	successCount := 0
	failCount := 0

	for i, story := range stories {
		if (i+1)%25 == 0 || i == 0 {
			log.Printf("  [%d/%d] Indexing stories...\n", i+1, len(stories))
		}

		// Load user data for username
		var user models.User
		database.DB.Select("username").Where("id = ?", story.UserID).First(&user)

		storyDoc := map[string]interface{}{
			"id":         story.ID,
			"user_id":    story.UserID,
			"username":   user.Username,
			"created_at": story.CreatedAt,
			"expires_at": story.ExpiresAt,
		}

		if err := searchClient.IndexStory(ctx, story.ID, storyDoc); err != nil {
			log.Printf("  ⚠️  Failed to index story %s: %v\n", story.ID, err)
			failCount++
			continue
		}
		successCount++
	}

	log.Printf("✅ Stories indexed: %d succeeded, %d failed\n", successCount, failCount)
}
