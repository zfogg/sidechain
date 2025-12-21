package dto

import (
	"time"

	"github.com/zfogg/sidechain/backend/internal/models"
)

// UserResponse is the public user representation (safe for API responses)
type UserResponse struct {
	ID                string    `json:"id"`
	Username          string    `json:"username"`
	DisplayName       string    `json:"display_name"`
	Bio               string    `json:"bio"`
	ProfilePictureURL string    `json:"profile_picture_url"`
	AvatarURL         string    `json:"avatar_url"` // Effective avatar: S3 if available, else OAuth
	Genre             string    `json:"genre"`
	DAWPreference     string    `json:"daw_preference"`
	IsVerified        bool      `json:"is_verified"`
	IsPrivate         bool      `json:"is_private"`
	FollowerCount     int       `json:"follower_count"`
	FollowingCount    int       `json:"following_count"`
	PostCount         int       `json:"post_count"`
	CreatedAt         time.Time `json:"created_at"`

	// Relationship fields (only set when requested by authenticated user)
	IsFollowing *bool `json:"is_following,omitempty"`
	FollowsYou  *bool `json:"follows_you,omitempty"`
}

// UserDetailResponse includes additional private info (for authenticated user viewing their own profile)
type UserDetailResponse struct {
	UserResponse
	Email         string `json:"email"`
	EmailVerified bool   `json:"email_verified"`
}

// CreateUserRequest for native registration
type CreateUserRequest struct {
	Email       string `json:"email" binding:"required,email"`
	Username    string `json:"username" binding:"required,min=3,max=30,alphanum"`
	Password    string `json:"password" binding:"required,min=8"`
	DisplayName string `json:"display_name" binding:"required,min=1,max=50"`
}

// UpdateUserRequest for profile updates
type UpdateUserRequest struct {
	DisplayName   *string `json:"display_name,omitempty" binding:"omitempty,min=1,max=50"`
	Bio           *string `json:"bio,omitempty" binding:"omitempty,max=500"`
	Genre         *string `json:"genre,omitempty"`
	DAWPreference *string `json:"daw_preference,omitempty"`
	Location      *string `json:"location,omitempty"`
	Website       *string `json:"website,omitempty" binding:"omitempty,url"`
	IsPrivate     *bool   `json:"is_private,omitempty"`
}

// ToUserResponse converts models.User to UserResponse (excludes sensitive fields)
func ToUserResponse(user *models.User) *UserResponse {
	if user == nil {
		return nil
	}

	return &UserResponse{
		ID:                user.ID,
		Username:          user.Username,
		DisplayName:       user.DisplayName,
		Bio:               user.Bio,
		ProfilePictureURL: user.ProfilePictureURL,
		AvatarURL:         user.GetAvatarURL(), // Effective avatar: S3 if available, else OAuth
		Genre:             "", // TODO: Convert StringArray to string
		DAWPreference:     user.DAWPreference,
		IsVerified:        false, // TODO: Add IsVerified field to models.User
		IsPrivate:         user.IsPrivate,
		FollowerCount:     user.FollowerCount,
		FollowingCount:    user.FollowingCount,
		PostCount:         user.PostCount,
		CreatedAt:         user.CreatedAt,
	}
}

// ToUserDetailResponse converts models.User to UserDetailResponse (includes private fields)
func ToUserDetailResponse(user *models.User) *UserDetailResponse {
	if user == nil {
		return nil
	}

	return &UserDetailResponse{
		UserResponse:  *ToUserResponse(user),
		Email:         user.Email,
		EmailVerified: user.EmailVerified,
	}
}

// ToUserResponses converts array of users to responses
func ToUserResponses(users []*models.User) []*UserResponse {
	responses := make([]*UserResponse, len(users))
	for i, user := range users {
		responses[i] = ToUserResponse(user)
	}
	return responses
}
