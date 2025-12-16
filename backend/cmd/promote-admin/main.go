package main

import (
	"flag"
	"fmt"
	"log"

	"github.com/joho/godotenv"
	"github.com/zfogg/sidechain/backend/internal/database"
	"github.com/zfogg/sidechain/backend/internal/models"
)

func main() {
	// Load environment variables
	godotenv.Load()

	// Parse command-line flags
	email := flag.String("email", "", "Email address of user to promote to admin")
	revoke := flag.Bool("revoke", false, "Revoke admin privileges instead of granting")
	flag.Parse()

	if *email == "" {
		fmt.Println("Usage: go run cmd/promote-admin/main.go -email=user@example.com")
		fmt.Println("       go run cmd/promote-admin/main.go -email=user@example.com -revoke")
		return
	}

	// Initialize database
	if err := database.Initialize(); err != nil {
		log.Fatalf("❌ Failed to initialize database: %v", err)
	}
	defer database.Close()

	db := database.DB
	if db == nil {
		log.Fatal("Failed to get database connection")
	}

	// Find user by email
	var user models.User
	result := db.Where("email = ?", *email).First(&user)

	if result.Error != nil {
		fmt.Printf("❌ User not found: %s\n", *email)
		return
	}

	if *revoke {
		// Revoke admin privileges
		if !user.IsAdmin {
			fmt.Printf("⚠️  User %s is not an admin\n", user.Username)
			return
		}

		user.IsAdmin = false
		if err := db.Save(&user).Error; err != nil {
			fmt.Printf("❌ Failed to revoke admin privileges: %v\n", err)
			return
		}

		fmt.Printf("✓ Admin privileges revoked for %s (%s)\n", user.Username, user.Email)
	} else {
		// Grant admin privileges
		if user.IsAdmin {
			fmt.Printf("⚠️  User %s is already an admin\n", user.Username)
			return
		}

		user.IsAdmin = true
		if err := db.Save(&user).Error; err != nil {
			fmt.Printf("❌ Failed to grant admin privileges: %v\n", err)
			return
		}

		fmt.Printf("✓ Admin privileges granted to %s (%s)\n", user.Username, user.Email)
		fmt.Printf("  User ID: %s\n", user.ID)
		fmt.Printf("  The user must log out and log back in for changes to take effect\n")
	}
}
