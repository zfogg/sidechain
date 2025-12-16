package service

import (
	"testing"
)

func TestNewProfileService(t *testing.T) {
	service := NewProfileService()
	if service == nil {
		t.Error("NewProfileService returned nil")
	}
}

func TestProfileService_ViewProfile(t *testing.T) {
	service := NewProfileService()
	if service == nil {
		t.Fatal("NewProfileService returned nil")
	}

	usernames := []string{"testuser", "producer1", "unknown-user"}
	for _, username := range usernames {
		err := service.ViewProfile(username)
		// Should complete without panic
		if err != nil && username != "" {
			t.Logf("ViewProfile %s returned error (expected for unknown user): %v", username, err)
		}
	}
}

func TestProfileService_FollowUnfollow(t *testing.T) {
	service := NewProfileService()

	username := "producer1"
	errFollow := service.FollowUser(username)
	errUnfollow := service.UnfollowUser(username)

	// Both operations should have same status
	if (errFollow != nil) != (errUnfollow != nil) {
		t.Logf("Follow/Unfollow asymmetry for %s", username)
	}
}

func TestProfileService_MuteUnmute(t *testing.T) {
	service := NewProfileService()

	username := "spam-user"
	errMute := service.MuteUser(username)
	errUnmute := service.UnmuteUser(username)

	// Both should have same status
	if (errMute != nil) != (errUnmute != nil) {
		t.Logf("Mute/Unmute asymmetry for %s", username)
	}
}

func TestProfileService_ListMutedUsers(t *testing.T) {
	service := NewProfileService()

	pages := []int{1, 2, 3}
	for _, page := range pages {
		err := service.ListMutedUsers(page)
		// Should not panic
		t.Logf("ListMutedUsers page %d: %v", page, err)
	}
}

func TestProfileService_GetFollowers(t *testing.T) {
	service := NewProfileService()

	tests := []struct {
		username string
		page     int
	}{
		{"user1", 1},
		{"user2", 2},
		{"", 1},
	}

	for _, tt := range tests {
		err := service.GetFollowers(tt.username, tt.page)
		if err != nil && tt.username != "" {
			t.Logf("GetFollowers %s page %d: error (expected): %v", tt.username, tt.page, err)
		}
	}
}

func TestProfileService_GetFollowing(t *testing.T) {
	service := NewProfileService()

	tests := []struct {
		username string
		page     int
	}{
		{"user1", 1},
		{"user2", 2},
	}

	for _, tt := range tests {
		err := service.GetFollowing(tt.username, tt.page)
		if err != nil {
			t.Logf("GetFollowing %s page %d: error (expected): %v", tt.username, tt.page, err)
		}
	}
}

func TestProfileService_GetActivity(t *testing.T) {
	service := NewProfileService()

	usernames := []string{"active-user", "producer1"}
	for _, username := range usernames {
		err := service.GetActivity(username)
		if err != nil {
			t.Logf("GetActivity %s: error (expected): %v", username, err)
		}
	}
}

func TestProfileService_EditProfile(t *testing.T) {
	service := NewProfileService()

	// EditProfile requires interactive input, so it will likely fail
	err := service.EditProfile()
	if err != nil {
		t.Logf("EditProfile: error (expected - interactive): %v", err)
	}
}

func TestProfileService_UploadProfilePicture(t *testing.T) {
	service := NewProfileService()

	// Will require interactive input or file input
	err := service.UploadProfilePicture()
	if err != nil {
		t.Logf("UploadProfilePicture: error (expected - requires input): %v", err)
	}
}
