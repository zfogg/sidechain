package api

import (
	"bytes"
	"fmt"
	"io"
	"mime/multipart"
	"os"
	"path/filepath"
	"strconv"

	json "github.com/json-iterator/go"
	"github.com/zfogg/sidechain/cli/pkg/client"
	"github.com/zfogg/sidechain/cli/pkg/logger"
)

// GetUserProfile gets a user's public profile
func GetUserProfile(username string) (*User, error) {
	logger.Debug("Fetching user profile", "username", username)

	resp, err := client.GetClient().
		R().
		Get("/api/v1/users/" + username + "/profile")

	if err := CheckResponse(resp, err); err != nil {
		return nil, err
	}

	var profileResp ProfileResponse
	if err := json.Unmarshal(resp.Body(), &profileResp); err != nil {
		return nil, err
	}

	return &profileResp.User, nil
}

// UpdateUserProfile updates current user's profile
func UpdateUserProfile(req UpdateProfileRequest) (*User, error) {
	logger.Debug("Updating user profile")

	reqBody, err := json.Marshal(req)
	if err != nil {
		return nil, err
	}

	resp, err := client.GetClient().
		R().
		SetHeader("Content-Type", "application/json").
		SetBody(reqBody).
		Put("/api/v1/users/me")

	if err := CheckResponse(resp, err); err != nil {
		return nil, err
	}

	var profileResp ProfileResponse
	if err := json.Unmarshal(resp.Body(), &profileResp); err != nil {
		return nil, err
	}

	return &profileResp.User, nil
}

// Follow follows a user
func Follow(username string) error {
	logger.Debug("Following user", "username", username)

	resp, err := client.GetClient().
		R().
		Post("/api/v1/profile/follow")

	return CheckResponse(resp, err)
}

// Unfollow unfollows a user
func Unfollow(username string) error {
	logger.Debug("Unfollowing user", "username", username)

	resp, err := client.GetClient().
		R().
		Post("/api/v1/profile/unfollow")

	return CheckResponse(resp, err)
}

// MuteUser mutes a user
func MuteUser(username string) error {
	logger.Debug("Muting user", "username", username)

	resp, err := client.GetClient().
		R().
		Post("/api/v1/users/" + username + "/mute")

	return CheckResponse(resp, err)
}

// UnmuteUser unmutes a user
func UnmuteUser(username string) error {
	logger.Debug("Unmuting user", "username", username)

	resp, err := client.GetClient().
		R().
		Delete("/api/v1/users/" + username + "/mute")

	return CheckResponse(resp, err)
}

// GetMutedUsers gets list of muted users
func GetMutedUsers(page, pageSize int) ([]User, error) {
	logger.Debug("Fetching muted users", "page", page, "pageSize", pageSize)

	resp, err := client.GetClient().
		R().
		SetQueryParam("page", strconv.Itoa(page)).
		SetQueryParam("page_size", strconv.Itoa(pageSize)).
		Get("/api/v1/users/me/muted")

	if err := CheckResponse(resp, err); err != nil {
		return nil, err
	}

	var result map[string]interface{}
	if err := json.Unmarshal(resp.Body(), &result); err != nil {
		return nil, err
	}

	var users []User
	return users, nil
}

// GetFollowers gets user's followers
func GetFollowers(username string, page, pageSize int) ([]User, int, error) {
	logger.Debug("Fetching followers", "username", username, "page", page)

	resp, err := client.GetClient().
		R().
		SetQueryParam("page", strconv.Itoa(page)).
		SetQueryParam("page_size", strconv.Itoa(pageSize)).
		Get("/api/v1/users/" + username + "/followers")

	if err := CheckResponse(resp, err); err != nil {
		return nil, 0, err
	}

	var result map[string]interface{}
	if err := json.Unmarshal(resp.Body(), &result); err != nil {
		return nil, 0, err
	}

	var followers []User
	totalCount := 0

	if followersData, ok := result["followers"].([]interface{}); ok {
		for _, f := range followersData {
			followerJSON, _ := json.Marshal(f)
			var user User
			if err := json.Unmarshal(followerJSON, &user); err == nil {
				followers = append(followers, user)
			}
		}
	}

	if total, ok := result["total_count"].(float64); ok {
		totalCount = int(total)
	}

	return followers, totalCount, nil
}

// GetFollowing gets users that someone is following
func GetFollowing(username string, page, pageSize int) ([]User, int, error) {
	logger.Debug("Fetching following", "username", username, "page", page)

	resp, err := client.GetClient().
		R().
		SetQueryParam("page", strconv.Itoa(page)).
		SetQueryParam("page_size", strconv.Itoa(pageSize)).
		Get("/api/v1/users/" + username + "/following")

	if err := CheckResponse(resp, err); err != nil {
		return nil, 0, err
	}

	var result map[string]interface{}
	if err := json.Unmarshal(resp.Body(), &result); err != nil {
		return nil, 0, err
	}

	var following []User
	totalCount := 0

	if followingData, ok := result["following"].([]interface{}); ok {
		for _, f := range followingData {
			followJSON, _ := json.Marshal(f)
			var user User
			if err := json.Unmarshal(followJSON, &user); err == nil {
				following = append(following, user)
			}
		}
	}

	if total, ok := result["total_count"].(float64); ok {
		totalCount = int(total)
	}

	return following, totalCount, nil
}

// GetUserActivity gets user's activity summary
func GetUserActivity(username string) (map[string]interface{}, error) {
	logger.Debug("Fetching user activity", "username", username)

	resp, err := client.GetClient().
		R().
		Get("/api/v1/users/" + username + "/activity")

	if err := CheckResponse(resp, err); err != nil {
		return nil, err
	}

	var activity map[string]interface{}
	if err := json.Unmarshal(resp.Body(), &activity); err != nil {
		return nil, err
	}

	return activity, nil
}

// UploadProfilePicture uploads a profile picture for the current user
func UploadProfilePicture(filePath string) (*User, error) {
	logger.Debug("Uploading profile picture", "file_path", filePath)

	file, err := os.Open(filePath)
	if err != nil {
		return nil, fmt.Errorf("failed to open file: %w", err)
	}
	defer file.Close()

	// Create multipart form
	body := &bytes.Buffer{}
	writer := multipart.NewWriter(body)

	// Add file field
	part, err := writer.CreateFormFile("profile_picture", filepath.Base(filePath))
	if err != nil {
		return nil, fmt.Errorf("failed to create form file: %w", err)
	}

	_, err = io.Copy(part, file)
	if err != nil {
		return nil, fmt.Errorf("failed to copy file: %w", err)
	}

	err = writer.Close()
	if err != nil {
		return nil, fmt.Errorf("failed to close writer: %w", err)
	}

	resp, err := client.GetClient().
		R().
		SetHeader("Content-Type", writer.FormDataContentType()).
		SetBody(body.Bytes()).
		Post("/api/v1/users/upload-profile-picture")

	if err := CheckResponse(resp, err); err != nil {
		return nil, err
	}

	var profileResp ProfileResponse
	if err := json.Unmarshal(resp.Body(), &profileResp); err != nil {
		return nil, err
	}

	return &profileResp.User, nil
}
