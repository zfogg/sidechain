package main

import (
	"context"
	"fmt"
	"log"
	"strings"

	"github.com/joho/godotenv"
	"github.com/zfogg/sidechain/backend/internal/database"
	"github.com/zfogg/sidechain/backend/internal/models"
	"github.com/zfogg/sidechain/backend/internal/stream"
)

func main() {
	// Load environment variables
	if err := godotenv.Load(); err != nil {
		log.Println("No .env file found, using environment variables")
	}

	// Initialize database
	if err := database.Initialize(); err != nil {
		log.Fatalf("Failed to initialize database: %v", err)
	}
	defer database.Close()

	// Initialize Stream client
	streamClient, err := stream.NewClient()
	if err != nil {
		log.Fatalf("Failed to create stream client: %v", err)
	}

	fmt.Println("🔧 Starting follow relationship migration...")
	fmt.Println("This will fix all existing follows to use database IDs instead of Stream IDs")
	fmt.Println()

	// Get all users from database
	var users []models.User
	if err := database.DB.Find(&users).Error; err != nil {
		log.Fatalf("Failed to fetch users: %v", err)
	}

	fmt.Printf("Found %d users in database\n", len(users))

	totalFixed := 0
	totalErrors := 0

	// For each user, get their follows and migrate them
	for _, user := range users {
		fmt.Printf("\n📋 Processing user: %s (%s)\n", user.Username, user.ID)

		// Get timeline feed using DATABASE ID (this is what the new code uses)
		timelineFeed, err := streamClient.FeedsClient().FlatFeed(stream.FeedGroupTimeline, user.ID)
		if err != nil {
			log.Printf("  ❌ Failed to get timeline feed: %v", err)
			totalErrors++
			continue
		}

		// Get all follows for this user's timeline
		ctx := context.Background()
		following, err := timelineFeed.GetFollowing(ctx)
		if err != nil {
			log.Printf("  ❌ Failed to get following list: %v", err)
			totalErrors++
			continue
		}

		if len(following.Results) == 0 {
			fmt.Printf("  ℹ️  No follows found\n")
			continue
		}

		fmt.Printf("  Found %d follows to migrate\n", len(following.Results))

		// Process each follow
		for _, follow := range following.Results {
			targetFeedID := follow.TargetID // Format: "user:SOME_ID"

			// Extract the ID from the feed ID (format: "user:ID")
			var targetID string
			if _, err := fmt.Sscanf(targetFeedID, "user:%s", &targetID); err != nil {
				log.Printf("  ⚠️  Failed to parse feed ID: %s", targetFeedID)
				continue
			}

			// Check if this is a Stream User ID or Database ID by looking it up
			var targetUser models.User

			// First try to find by database ID
			errDB := database.DB.Where("id = ?", targetID).First(&targetUser).Error
			if errDB != nil {
				// Not found by DB ID, try Stream User ID
				errStream := database.DB.Where("stream_user_id = ?", targetID).First(&targetUser).Error
				if errStream != nil {
					log.Printf("  ⚠️  Could not find user with ID or Stream ID: %s (skipping)", targetID)
					totalErrors++
					continue
				}
				// Found by Stream ID - this needs migration!
				fmt.Printf("  🔄 Migrating follow: %s -> %s (Stream ID: %s -> DB ID: %s)\n",
					user.Username, targetUser.Username, targetUser.StreamUserID, targetUser.ID)

				// Unfollow the old relationship (using Stream ID)
				if err := streamClient.UnfollowUser(user.ID, targetUser.StreamUserID); err != nil {
					log.Printf("  ⚠️  Failed to unfollow old relationship: %v", err)
				}

				// Create new follow relationship (using Database ID)
				if err := streamClient.FollowUser(user.ID, targetUser.ID); err != nil {
					log.Printf("  ❌ Failed to create new follow relationship: %v", err)
					totalErrors++
					continue
				}

				fmt.Printf("  ✅ Successfully migrated follow to %s\n", targetUser.Username)
				totalFixed++
			} else {
				// Already using database ID - no migration needed
				fmt.Printf("  ✓ Follow already uses database ID: %s -> %s\n", user.Username, targetUser.Username)
			}
		}
	}

	fmt.Println()
	fmt.Println(strings.Repeat("=", 60))
	fmt.Printf("Migration complete!\n")
	fmt.Printf("  Fixed: %d follow relationships\n", totalFixed)
	fmt.Printf("  Errors: %d\n", totalErrors)
	fmt.Println(strings.Repeat("=", 60))
}
