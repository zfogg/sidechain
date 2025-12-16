package service

import (
	"bufio"
	"fmt"
	"os"
	"strings"

	"github.com/zfogg/sidechain/cli/pkg/api"
	"github.com/zfogg/sidechain/cli/pkg/formatter"
)

// MuteService provides operations for managing muted users
type MuteService struct{}

// NewMuteService creates a new mute service
func NewMuteService() *MuteService {
	return &MuteService{}
}

// MuteUser mutes a user by username
func (ms *MuteService) MuteUser() error {
	reader := bufio.NewReader(os.Stdin)

	fmt.Print("Username to mute: ")
	username, _ := reader.ReadString('\n')
	username = strings.TrimSpace(username)

	if username == "" {
		return fmt.Errorf("username cannot be empty")
	}

	// Mute the user
	err := api.MuteUser(username)
	if err != nil {
		return fmt.Errorf("failed to mute user: %w", err)
	}

	formatter.PrintSuccess("✓ User muted successfully!")
	fmt.Printf("\n")
	keyValues := map[string]interface{}{
		"Username": username,
	}
	formatter.PrintKeyValue(keyValues)

	return nil
}

// UnmuteUser unmutes a user by username
func (ms *MuteService) UnmuteUser() error {
	reader := bufio.NewReader(os.Stdin)

	fmt.Print("Username to unmute: ")
	username, _ := reader.ReadString('\n')
	username = strings.TrimSpace(username)

	if username == "" {
		return fmt.Errorf("username cannot be empty")
	}

	// Unmute the user
	err := api.UnmuteUser(username)
	if err != nil {
		return fmt.Errorf("failed to unmute user: %w", err)
	}

	formatter.PrintSuccess("✓ User unmuted successfully!")
	fmt.Printf("\n")
	keyValues := map[string]interface{}{
		"Username": username,
	}
	formatter.PrintKeyValue(keyValues)

	return nil
}

// ListMutedUsers lists all muted users
func (ms *MuteService) ListMutedUsers(page, pageSize int) error {
	if page < 1 {
		page = 1
	}
	if pageSize < 1 || pageSize > 100 {
		pageSize = 50
	}

	users, err := api.GetMutedUsers(page, pageSize)
	if err != nil {
		return fmt.Errorf("failed to fetch muted users: %w", err)
	}

	if len(users) == 0 {
		fmt.Println("No muted users")
		return nil
	}

	// Create table data
	headers := []string{"Username", "Display Name", "Bio"}
	rows := make([][]string, len(users))

	for i, user := range users {
		rows[i] = []string{
			user.Username,
			user.DisplayName,
			user.Bio,
		}
	}

	formatter.PrintTable(headers, rows)

	return nil
}
