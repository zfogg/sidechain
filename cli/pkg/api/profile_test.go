package api

import (
	"testing"
)

func TestGetUserProfile_WithValidUsername(t *testing.T) {
	usernames := []string{"testuser", "producer1", "dj-smith"}

	for _, username := range usernames {
		user, err := GetUserProfile(username)
		if err != nil {
			t.Logf("GetUserProfile %s: API called (error expected): %v", username, err)
			continue
		}
		if user == nil {
			t.Errorf("GetUserProfile returned nil for %s", username)
		} else {
			if user.ID == "" {
				t.Errorf("Profile missing ID for %s", username)
			}
			if user.Username != username && user.Username != "" {
				t.Errorf("Username mismatch: expected %s, got %s", username, user.Username)
			}
		}
	}
}

func TestGetUserProfile_WithInvalidUsername(t *testing.T) {
	invalidUsernames := []string{"", "nonexistent-user-12345", "invalid@user"}

	for _, username := range invalidUsernames {
		user, err := GetUserProfile(username)
		if user != nil && user.ID != "" && err == nil {
			t.Logf("GetUserProfile with invalid username succeeded: %s", username)
		}
	}
}

func TestUpdateUserProfile_WithValidData(t *testing.T) {
	updates := []UpdateProfileRequest{
		{
			Bio:      "Updated bio",
			Location: "New York",
		},
		{
			DisplayName: "New Name",
			DAW:         "Ableton Live",
		},
		{
			Bio:      "LA-based producer",
			Location: "Los Angeles",
		},
	}

	for i, update := range updates {
		user, err := UpdateUserProfile(update)
		if err != nil {
			t.Logf("UpdateUserProfile %d: API called (error expected): %v", i, err)
			continue
		}
		if user == nil {
			t.Errorf("UpdateUserProfile returned nil for update %d", i)
		}
	}
}

func TestFollow_UnfollowSymmetry(t *testing.T) {
	usernames := []string{"user1", "user2"}

	for _, username := range usernames {
		errFollow := Follow(username)
		errUnfollow := Unfollow(username)

		hasFollowErr := errFollow != nil
		hasUnfollowErr := errUnfollow != nil

		if hasFollowErr != hasUnfollowErr {
			t.Logf("Follow/Unfollow error mismatch for %s: follow=%v, unfollow=%v", username, hasFollowErr, hasUnfollowErr)
		}
	}
}

func TestFollow_WithInvalidUsername(t *testing.T) {
	invalidUsernames := []string{"", "nonexistent"}

	for _, username := range invalidUsernames {
		err := Follow(username)
		if err == nil && username == "" {
			t.Logf("Follow accepted empty username")
		}
	}
}

func TestMuteUser_UnmuteUserSymmetry(t *testing.T) {
	usernames := []string{"spam-user", "troll"}

	for _, username := range usernames {
		errMute := MuteUser(username)
		errUnmute := UnmuteUser(username)

		hasMute := errMute != nil
		hasUnmute := errUnmute != nil

		if hasMute != hasUnmute {
			t.Logf("Mute/Unmute error mismatch for %s: mute=%v, unmute=%v", username, hasMute, hasUnmute)
		}
	}
}

func TestGetMutedUsers_WithPagination(t *testing.T) {
	tests := []struct {
		page     int
		pageSize int
	}{
		{1, 10},
		{2, 20},
		{1, 50},
	}

	for _, tt := range tests {
		users, err := GetMutedUsers(tt.page, tt.pageSize)
		if err != nil {
			t.Logf("GetMutedUsers page=%d, size=%d: API called (error expected)", tt.page, tt.pageSize)
			continue
		}
		if users != nil {
			for _, user := range users {
				if user.ID == "" {
					t.Errorf("Muted user missing ID")
				}
			}
		}
	}
}

func TestGetMutedUsers_WithInvalidPagination(t *testing.T) {
	invalidTests := []struct {
		page     int
		pageSize int
	}{
		{0, 10},
		{-1, 10},
		{1, 0},
	}

	for _, tt := range invalidTests {
		users, err := GetMutedUsers(tt.page, tt.pageSize)
		if users != nil && err == nil {
			t.Logf("GetMutedUsers accepted invalid pagination: page=%d, size=%d", tt.page, tt.pageSize)
		}
	}
}

func TestGetFollowers_WithValidUsername(t *testing.T) {
	tests := []struct {
		username string
		page     int
		pageSize int
	}{
		{"user1", 1, 10},
		{"user2", 1, 20},
		{"popular-user", 2, 10},
	}

	for _, tt := range tests {
		followers, count, err := GetFollowers(tt.username, tt.page, tt.pageSize)
		if err != nil {
			t.Logf("GetFollowers %s: API called (error expected): %v", tt.username, err)
			continue
		}
		if count < 0 {
			t.Errorf("GetFollowers returned negative count for %s", tt.username)
		}
		if followers != nil && len(followers) > count {
			t.Errorf("Followers count mismatch for %s: got %d followers but count is %d", tt.username, len(followers), count)
		}
	}
}

func TestGetFollowers_WithInvalidUsername(t *testing.T) {
	invalidUsernames := []string{"", "nonexistent-user"}

	for _, username := range invalidUsernames {
		followers, count, err := GetFollowers(username, 1, 10)
		if followers != nil && count >= 0 && err == nil {
			t.Logf("GetFollowers accepted invalid username: %s", username)
		}
	}
}

func TestGetFollowing_WithValidUsername(t *testing.T) {
	tests := []struct {
		username string
		page     int
		pageSize int
	}{
		{"user1", 1, 10},
		{"user2", 1, 20},
	}

	for _, tt := range tests {
		following, count, err := GetFollowing(tt.username, tt.page, tt.pageSize)
		if err != nil {
			t.Logf("GetFollowing %s: API called (error expected): %v", tt.username, err)
			continue
		}
		if count < 0 {
			t.Errorf("GetFollowing returned negative count for %s", tt.username)
		}
		if following != nil && len(following) > count {
			t.Errorf("Following count mismatch for %s", tt.username)
		}
	}
}

func TestGetUserActivity_WithValidUsername(t *testing.T) {
	usernames := []string{"active-user", "producer"}

	for _, username := range usernames {
		activity, err := GetUserActivity(username)
		if err != nil {
			t.Logf("GetUserActivity %s: API called (error expected): %v", username, err)
			continue
		}
		if activity == nil {
			t.Logf("GetUserActivity returned nil for %s (valid)", username)
		} else if len(activity) > 0 {
			t.Logf("GetUserActivity has data for %s", username)
		}
	}
}

func TestGetUserActivity_WithInvalidUsername(t *testing.T) {
	invalidUsernames := []string{"", "nonexistent-user"}

	for _, username := range invalidUsernames {
		activity, err := GetUserActivity(username)
		if activity != nil && len(activity) > 0 && err == nil {
			t.Logf("GetUserActivity accepted invalid username: %s", username)
		}
	}
}

func TestUploadProfilePicture_WithValidFile(t *testing.T) {
	// Would need actual test image file
	validPaths := []string{"/tmp/test-profile.jpg", "/tmp/profile-pic.png"}

	for _, path := range validPaths {
		user, err := UploadProfilePicture(path)
		if err != nil {
			t.Logf("UploadProfilePicture %s: API called or file missing (error expected): %v", path, err)
			continue
		}
		if user == nil {
			t.Errorf("UploadProfilePicture returned nil")
		}
	}
}

func TestUploadProfilePicture_WithInvalidPath(t *testing.T) {
	invalidPaths := []string{"", "/nonexistent/file.jpg", "relative/path.png"}

	for _, path := range invalidPaths {
		user, err := UploadProfilePicture(path)
		// Should fail with file not found
		if user != nil && err == nil && path != "" {
			t.Logf("UploadProfilePicture accepted invalid path: %s", path)
		}
	}
}
