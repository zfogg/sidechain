package main

import (
	"fmt"
	"log"

	"github.com/joho/godotenv"
	"github.com/zfogg/sidechain/backend/internal/database"
	"github.com/zfogg/sidechain/backend/internal/models"
	"github.com/zfogg/sidechain/backend/internal/stream"
	"golang.org/x/crypto/bcrypt"
)

func main() {
	// Load environment variables
	if err := godotenv.Load(); err != nil {
		log.Println("Warning: .env file not found, using system environment variables")
	}

	// Initialize database
	if err := database.Initialize(); err != nil {
		log.Fatalf("Failed to initialize database: %v", err)
	}
	defer database.Close()

	// Initialize Stream.io client
	streamClient, err := stream.NewClient()
	if err != nil {
		log.Fatalf("Failed to initialize Stream.io client: %v", err)
	}

	// Test users matching web/e2e/fixtures/test-users.ts
	testUsers := []struct {
		username    string
		email       string
		password    string
		displayName string
	}{
		{"alice", "alice@example.com", "password123", "Alice Smith"},
		{"bob", "bob@example.com", "password123", "Bob Johnson"},
		{"charlie", "charlie@example.com", "password123", "Charlie Brown"},
		{"diana", "diana@example.com", "password123", "Diana Prince"},
		{"eve", "eve@example.com", "password123", "Eve Wilson"},
	}

	for _, tu := range testUsers {
		// Check if user already exists
		var existingUser models.User
		err := database.DB.Where("username = ? OR email = ?", tu.username, tu.email).First(&existingUser).Error
		if err == nil {
			fmt.Printf("✓ User %s already exists, skipping...\n", tu.username)
			continue
		}

		// Hash password
		hashedPassword, err := bcrypt.GenerateFromPassword([]byte(tu.password), bcrypt.DefaultCost)
		if err != nil {
			log.Fatalf("Failed to hash password for %s: %v", tu.username, err)
		}
		hashedPasswordStr := string(hashedPassword)

		// Create user
		user := models.User{
			Email:             tu.email,
			Username:          tu.username,
			DisplayName:       tu.displayName,
			PasswordHash:      &hashedPasswordStr,
			EmailVerified:     true,
			ProfilePictureURL: fmt.Sprintf("https://api.dicebear.com/7.x/avataaars/png?seed=%s", tu.username),
			DAWPreference:     "Ableton",
			Genre:             models.StringArray{"Electronic", "House"},
			FollowerCount:     0,
			FollowingCount:    0,
			PostCount:         0,
		}

		if err := database.DB.Create(&user).Error; err != nil {
			log.Fatalf("Failed to create user %s: %v", tu.username, err)
		}

		// Create Stream.io user
		if err := streamClient.CreateUser(user.ID, user.Username); err != nil {
			log.Printf("⚠️  Failed to create Stream.io user for %s: %v\n", tu.username, err)
		} else {
			fmt.Printf("✓ Created Stream.io user for %s\n", tu.username)
		}

		fmt.Printf("✓ Created user: %s (%s)\n", tu.username, tu.email)
	}

	fmt.Println("\n✅ All test users created successfully!")
}
