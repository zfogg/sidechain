package service

import (
	"fmt"

	"github.com/zfogg/sidechain/cli/pkg/api"
	"github.com/zfogg/sidechain/cli/pkg/client"
	"github.com/zfogg/sidechain/cli/pkg/credentials"
	"github.com/zfogg/sidechain/cli/pkg/formatter"
	"github.com/zfogg/sidechain/cli/pkg/prompter"
)

type ProfileService struct{}

// NewProfileService creates a new profile service
func NewProfileService() *ProfileService {
	return &ProfileService{}
}

// ViewProfile views a user's profile
func (s *ProfileService) ViewProfile(username string) error {
	if username == "" {
		username = "me"
	}

	client.Init()

	// Load and set auth token if available
	creds, _ := credentials.Load()
	if creds != nil && creds.IsValid() {
		client.SetAuthToken(creds.AccessToken)
	}

	formatter.PrintInfo("Fetching profile for %s...", username)

	var user *api.User
	var err error

	if username == "me" {
		user, err = api.GetCurrentUser()
	} else {
		user, err = api.GetUserProfile(username)
	}

	if err != nil {
		if api.IsNotFound(err) {
			formatter.PrintError("User not found: %s", username)
		} else {
			formatter.PrintError("Failed to fetch profile: %v", err)
		}
		return err
	}

	// Display profile
	fmt.Printf("\n")
	keyValues := map[string]interface{}{
		"Username":       user.Username,
		"Display Name":   user.DisplayName,
		"Email":          user.Email,
		"Bio":            user.Bio,
		"Location":       user.Location,
		"Followers":      user.FollowerCount,
		"Following":      user.FollowingCount,
		"Posts":          user.PostCount,
		"Email Verified": user.EmailVerified,
		"2FA Enabled":    user.TwoFactorEnabled,
		"Private":        user.IsPrivate,
		"Created":        user.CreatedAt.Format("2006-01-02"),
	}

	// Show admin status if user is admin
	if creds != nil && creds.IsAdmin && user.IsAdmin {
		keyValues["Admin"] = "✓ YES"
	}

	formatter.PrintKeyValue(keyValues)

	return nil
}

// EditProfile edits current user's profile
func (s *ProfileService) EditProfile() error {
	creds, err := credentials.Load()
	if err != nil {
		return err
	}

	if creds == nil || !creds.IsValid() {
		formatter.PrintError("Not logged in. Please run 'sidechain-cli auth login'")
		return fmt.Errorf("not authenticated")
	}

	client.Init()
	client.SetAuthToken(creds.AccessToken)

	// Fetch current profile
	user, err := api.GetCurrentUser()
	if err != nil {
		formatter.PrintError("Failed to fetch current profile: %v", err)
		return err
	}

	// Prompt for new values
	fmt.Printf("\n")
	formatter.PrintInfo("Editing profile (leave blank to keep current value)")
	fmt.Printf("\n")

	displayName, _ := prompter.PromptString("Display Name [" + user.DisplayName + "]: ")
	if displayName == "" {
		displayName = user.DisplayName
	}

	bio, _ := prompter.PromptString("Bio [" + user.Bio + "]: ")
	if bio == "" {
		bio = user.Bio
	}

	location, _ := prompter.PromptString("Location [" + user.Location + "]: ")
	if location == "" {
		location = user.Location
	}

	// Create update request
	updateReq := api.UpdateProfileRequest{
		DisplayName: displayName,
		Bio:         bio,
		Location:    location,
	}

	formatter.PrintInfo("Updating profile...")

	updatedUser, err := api.UpdateUserProfile(updateReq)
	if err != nil {
		formatter.PrintError("Failed to update profile: %v", err)
		return err
	}

	formatter.PrintSuccess("✓ Profile updated successfully!")
	fmt.Printf("\n")
	formatter.PrintKeyValue(map[string]interface{}{
		"Display Name": updatedUser.DisplayName,
		"Bio":          updatedUser.Bio,
		"Location":     updatedUser.Location,
	})

	return nil
}

// FollowUser follows a user
func (s *ProfileService) FollowUser(username string) error {
	if username == "" {
		return fmt.Errorf("username required")
	}

	creds, err := credentials.Load()
	if err != nil {
		return err
	}

	if creds == nil || !creds.IsValid() {
		formatter.PrintError("Not logged in. Please run 'sidechain-cli auth login'")
		return fmt.Errorf("not authenticated")
	}

	client.Init()
	client.SetAuthToken(creds.AccessToken)

	confirm, _ := prompter.PromptConfirm("Follow " + username + "?")
	if !confirm {
		return nil
	}

	formatter.PrintInfo("Following %s...", username)

	if err := api.Follow(username); err != nil {
		formatter.PrintError("Failed to follow user: %v", err)
		return err
	}

	formatter.PrintSuccess("✓ Now following %s", username)
	return nil
}

// UnfollowUser unfollows a user
func (s *ProfileService) UnfollowUser(username string) error {
	if username == "" {
		return fmt.Errorf("username required")
	}

	creds, err := credentials.Load()
	if err != nil {
		return err
	}

	if creds == nil || !creds.IsValid() {
		formatter.PrintError("Not logged in. Please run 'sidechain-cli auth login'")
		return fmt.Errorf("not authenticated")
	}

	client.Init()
	client.SetAuthToken(creds.AccessToken)

	confirm, _ := prompter.PromptConfirm("Unfollow " + username + "?")
	if !confirm {
		return nil
	}

	formatter.PrintInfo("Unfollowing %s...", username)

	if err := api.Unfollow(username); err != nil {
		formatter.PrintError("Failed to unfollow user: %v", err)
		return err
	}

	formatter.PrintSuccess("✓ Unfollowed %s", username)
	return nil
}

// MuteUser mutes a user
func (s *ProfileService) MuteUser(username string) error {
	if username == "" {
		return fmt.Errorf("username required")
	}

	creds, err := credentials.Load()
	if err != nil {
		return err
	}

	if creds == nil || !creds.IsValid() {
		formatter.PrintError("Not logged in. Please run 'sidechain-cli auth login'")
		return fmt.Errorf("not authenticated")
	}

	client.Init()
	client.SetAuthToken(creds.AccessToken)

	confirm, _ := prompter.PromptConfirm("Mute " + username + "?")
	if !confirm {
		return nil
	}

	formatter.PrintInfo("Muting %s...", username)

	if err := api.MuteUser(username); err != nil {
		formatter.PrintError("Failed to mute user: %v", err)
		return err
	}

	formatter.PrintSuccess("✓ Muted %s", username)
	return nil
}

// UnmuteUser unmutes a user
func (s *ProfileService) UnmuteUser(username string) error {
	if username == "" {
		return fmt.Errorf("username required")
	}

	creds, err := credentials.Load()
	if err != nil {
		return err
	}

	if creds == nil || !creds.IsValid() {
		formatter.PrintError("Not logged in. Please run 'sidechain-cli auth login'")
		return fmt.Errorf("not authenticated")
	}

	client.Init()
	client.SetAuthToken(creds.AccessToken)

	formatter.PrintInfo("Unmuting %s...", username)

	if err := api.UnmuteUser(username); err != nil {
		formatter.PrintError("Failed to unmute user: %v", err)
		return err
	}

	formatter.PrintSuccess("✓ Unmuted %s", username)
	return nil
}

// ListMutedUsers lists muted users
func (s *ProfileService) ListMutedUsers(page int) error {
	creds, err := credentials.Load()
	if err != nil {
		return err
	}

	if creds == nil || !creds.IsValid() {
		formatter.PrintError("Not logged in. Please run 'sidechain-cli auth login'")
		return fmt.Errorf("not authenticated")
	}

	client.Init()
	client.SetAuthToken(creds.AccessToken)

	formatter.PrintInfo("Fetching muted users...")

	users, err := api.GetMutedUsers(page, 20)
	if err != nil {
		formatter.PrintError("Failed to fetch muted users: %v", err)
		return err
	}

	if len(users) == 0 {
		formatter.PrintInfo("No muted users")
		return nil
	}

	fmt.Printf("\n")
	headers := []string{"Username", "Display Name"}
	var rows [][]string

	for _, user := range users {
		rows = append(rows, []string{user.Username, user.DisplayName})
	}

	formatter.PrintTable(headers, rows)
	return nil
}

// GetFollowers lists followers of a user
func (s *ProfileService) GetFollowers(username string, page int) error {
	client.Init()

	creds, _ := credentials.Load()
	if creds != nil && creds.IsValid() {
		client.SetAuthToken(creds.AccessToken)
	}

	if username == "" {
		username = "me"
	}

	formatter.PrintInfo("Fetching followers for %s...", username)

	followers, total, err := api.GetFollowers(username, page, 20)
	if err != nil {
		formatter.PrintError("Failed to fetch followers: %v", err)
		return err
	}

	fmt.Printf("\n")
	formatter.PrintInfo("Followers: %d", total)
	fmt.Printf("\n")

	if len(followers) == 0 {
		formatter.PrintInfo("No followers found")
		return nil
	}

	headers := []string{"Username", "Display Name", "Followers"}
	var rows [][]string

	for _, user := range followers {
		rows = append(rows, []string{
			user.Username,
			user.DisplayName,
			fmt.Sprintf("%d", user.FollowerCount),
		})
	}

	formatter.PrintTable(headers, rows)
	return nil
}

// GetFollowing lists users that someone is following
func (s *ProfileService) GetFollowing(username string, page int) error {
	client.Init()

	creds, _ := credentials.Load()
	if creds != nil && creds.IsValid() {
		client.SetAuthToken(creds.AccessToken)
	}

	if username == "" {
		username = "me"
	}

	formatter.PrintInfo("Fetching following for %s...", username)

	following, total, err := api.GetFollowing(username, page, 20)
	if err != nil {
		formatter.PrintError("Failed to fetch following: %v", err)
		return err
	}

	fmt.Printf("\n")
	formatter.PrintInfo("Following: %d", total)
	fmt.Printf("\n")

	if len(following) == 0 {
		formatter.PrintInfo("Not following anyone")
		return nil
	}

	headers := []string{"Username", "Display Name", "Followers"}
	var rows [][]string

	for _, user := range following {
		rows = append(rows, []string{
			user.Username,
			user.DisplayName,
			fmt.Sprintf("%d", user.FollowerCount),
		})
	}

	formatter.PrintTable(headers, rows)
	return nil
}

// GetActivity gets user's activity summary
func (s *ProfileService) GetActivity(username string) error {
	client.Init()

	creds, _ := credentials.Load()
	if creds != nil && creds.IsValid() {
		client.SetAuthToken(creds.AccessToken)
	}

	if username == "" {
		username = "me"
	}

	formatter.PrintInfo("Fetching activity for %s...", username)

	activity, err := api.GetUserActivity(username)
	if err != nil {
		formatter.PrintError("Failed to fetch activity: %v", err)
		return err
	}

	fmt.Printf("\n")
	formatter.PrintObject(activity, "Activity")
	return nil
}

// UploadProfilePicture uploads a profile picture
func (s *ProfileService) UploadProfilePicture() error {
	client.Init()

	creds, _ := credentials.Load()
	if creds != nil && creds.IsValid() {
		client.SetAuthToken(creds.AccessToken)
	}

	filePath, err := prompter.PromptString("Profile picture file path: ")
	if err != nil {
		return err
	}

	formatter.PrintInfo("Uploading profile picture...")

	user, err := api.UploadProfilePicture(filePath)
	if err != nil {
		formatter.PrintError("Failed to upload profile picture: %v", err)
		return err
	}

	formatter.PrintSuccess("Profile picture uploaded successfully!")
	fmt.Printf("Profile picture URL: %s\n", user.ProfilePicture)
	return nil
}
