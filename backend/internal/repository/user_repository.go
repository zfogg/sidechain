package repository

import (
	"context"
	"errors"

	"github.com/zfogg/sidechain/backend/internal/models"
	"gorm.io/gorm"
)

var (
	ErrUserNotFound = errors.New("user not found")
)

// UserRepository handles all database operations for users
type UserRepository interface {
	// User CRUD
	CreateUser(ctx context.Context, user *models.User) error
	GetUser(ctx context.Context, userID string) (*models.User, error)
	GetUserByEmail(ctx context.Context, email string) (*models.User, error)
	GetUserByUsername(ctx context.Context, username string) (*models.User, error)
	UpdateUser(ctx context.Context, user *models.User) error
	DeleteUser(ctx context.Context, userID string) error

	// User queries
	GetUsers(ctx context.Context, userIDs []string) ([]*models.User, error)
	SearchUsers(ctx context.Context, query string, limit, offset int) ([]*models.User, error)
	GetTrendingUsers(ctx context.Context, limit, offset int) ([]*models.User, error)
	GetUsersByGenre(ctx context.Context, genre string, limit, offset int) ([]*models.User, error)

	// Followers/Following
	GetFollowers(ctx context.Context, userID string, limit, offset int) ([]*models.User, error)
	GetFollowing(ctx context.Context, userID string, limit, offset int) ([]*models.User, error)
	GetFollowerCount(ctx context.Context, userID string) (int64, error)
	GetFollowingCount(ctx context.Context, userID string) (int64, error)

	// Follow relationship
	CreateFollow(ctx context.Context, followerID, followingID string) error
	DeleteFollow(ctx context.Context, followerID, followingID string) error
	IsFollowing(ctx context.Context, followerID, followingID string) (bool, error)

	// Stats
	GetTotalUserCount(ctx context.Context) (int64, error)
}

// userRepository implements UserRepository interface
type userRepository struct {
	db *gorm.DB
}

// NewUserRepository creates a new user repository
func NewUserRepository(db *gorm.DB) UserRepository {
	return &userRepository{db: db}
}

// CreateUser creates a new user
func (r *userRepository) CreateUser(ctx context.Context, user *models.User) error {
	if user == nil {
		return ErrInvalidInput
	}

	return r.db.WithContext(ctx).Create(user).Error
}

// GetUser gets a user by ID
func (r *userRepository) GetUser(ctx context.Context, userID string) (*models.User, error) {
	var user models.User
	err := r.db.WithContext(ctx).Where("id = ?", userID).First(&user).Error

	if errors.Is(err, gorm.ErrRecordNotFound) {
		return nil, ErrUserNotFound
	}

	return &user, err
}

// GetUserByEmail gets a user by email (case-insensitive)
func (r *userRepository) GetUserByEmail(ctx context.Context, email string) (*models.User, error) {
	var user models.User
	err := r.db.WithContext(ctx).
		Where("LOWER(email) = LOWER(?)", email).
		First(&user).Error

	if errors.Is(err, gorm.ErrRecordNotFound) {
		return nil, ErrUserNotFound
	}

	return &user, err
}

// GetUserByUsername gets a user by username
func (r *userRepository) GetUserByUsername(ctx context.Context, username string) (*models.User, error) {
	var user models.User
	err := r.db.WithContext(ctx).
		Where("LOWER(username) = LOWER(?)", username).
		First(&user).Error

	if errors.Is(err, gorm.ErrRecordNotFound) {
		return nil, ErrUserNotFound
	}

	return &user, err
}

// UpdateUser updates a user
func (r *userRepository) UpdateUser(ctx context.Context, user *models.User) error {
	if user == nil || user.ID == "" {
		return ErrInvalidInput
	}

	return r.db.WithContext(ctx).Save(user).Error
}

// DeleteUser soft deletes a user
func (r *userRepository) DeleteUser(ctx context.Context, userID string) error {
	return r.db.WithContext(ctx).
		Where("id = ?", userID).
		Delete(&models.User{}).Error
}

// GetUsers gets multiple users by IDs
func (r *userRepository) GetUsers(ctx context.Context, userIDs []string) ([]*models.User, error) {
	var users []*models.User

	err := r.db.WithContext(ctx).
		Where("id IN ?", userIDs).
		Find(&users).Error

	return users, err
}

// SearchUsers searches users by username or display name
func (r *userRepository) SearchUsers(ctx context.Context, query string, limit, offset int) ([]*models.User, error) {
	var users []*models.User

	searchPattern := "%" + query + "%"

	err := r.db.WithContext(ctx).
		Where("LOWER(username) LIKE LOWER(?) OR LOWER(display_name) LIKE LOWER(?)", searchPattern, searchPattern).
		Order("follower_count DESC").
		Limit(limit).
		Offset(offset).
		Find(&users).Error

	return users, err
}

// GetTrendingUsers gets trending users (by follower growth)
func (r *userRepository) GetTrendingUsers(ctx context.Context, limit, offset int) ([]*models.User, error) {
	var users []*models.User

	err := r.db.WithContext(ctx).
		Order("follower_count DESC").
		Limit(limit).
		Offset(offset).
		Find(&users).Error

	return users, err
}

// GetUsersByGenre gets users by primary genre
func (r *userRepository) GetUsersByGenre(ctx context.Context, genre string, limit, offset int) ([]*models.User, error) {
	var users []*models.User

	err := r.db.WithContext(ctx).
		Where("genre = ?", genre).
		Order("follower_count DESC").
		Limit(limit).
		Offset(offset).
		Find(&users).Error

	return users, err
}

// GetFollowers gets users following the given user
func (r *userRepository) GetFollowers(ctx context.Context, userID string, limit, offset int) ([]*models.User, error) {
	var users []*models.User

	err := r.db.WithContext(ctx).
		Joins("JOIN follows ON follows.follower_id = users.id").
		Where("follows.following_id = ?", userID).
		Order("follows.created_at DESC").
		Limit(limit).
		Offset(offset).
		Find(&users).Error

	return users, err
}

// GetFollowing gets users that the given user follows
func (r *userRepository) GetFollowing(ctx context.Context, userID string, limit, offset int) ([]*models.User, error) {
	var users []*models.User

	err := r.db.WithContext(ctx).
		Joins("JOIN follows ON follows.following_id = users.id").
		Where("follows.follower_id = ?", userID).
		Order("follows.created_at DESC").
		Limit(limit).
		Offset(offset).
		Find(&users).Error

	return users, err
}

// GetFollowerCount gets follower count for a user
func (r *userRepository) GetFollowerCount(ctx context.Context, userID string) (int64, error) {
	var count int64

	err := r.db.WithContext(ctx).
		Table("follows").
		Where("following_id = ?", userID).
		Count(&count).Error

	return count, err
}

// GetFollowingCount gets following count for a user
func (r *userRepository) GetFollowingCount(ctx context.Context, userID string) (int64, error) {
	var count int64

	err := r.db.WithContext(ctx).
		Table("follows").
		Where("follower_id = ?", userID).
		Count(&count).Error

	return count, err
}

// CreateFollow creates a follow relationship
func (r *userRepository) CreateFollow(ctx context.Context, followerID, followingID string) error {
	follow := struct {
		FollowerID  string `gorm:"column:follower_id"`
		FollowingID string `gorm:"column:following_id"`
	}{
		FollowerID:  followerID,
		FollowingID: followingID,
	}

	return r.db.WithContext(ctx).
		Table("follows").
		Create(&follow).Error
}

// DeleteFollow deletes a follow relationship
func (r *userRepository) DeleteFollow(ctx context.Context, followerID, followingID string) error {
	return r.db.WithContext(ctx).
		Table("follows").
		Where("follower_id = ? AND following_id = ?", followerID, followingID).
		Delete(nil).Error
}

// IsFollowing checks if follower follows following
func (r *userRepository) IsFollowing(ctx context.Context, followerID, followingID string) (bool, error) {
	var count int64

	err := r.db.WithContext(ctx).
		Table("follows").
		Where("follower_id = ? AND following_id = ?", followerID, followingID).
		Count(&count).Error

	return count > 0, err
}

// GetTotalUserCount gets total user count
func (r *userRepository) GetTotalUserCount(ctx context.Context) (int64, error) {
	var count int64
	err := r.db.WithContext(ctx).
		Model(&models.User{}).
		Count(&count).Error

	return count, err
}
