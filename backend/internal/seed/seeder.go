package seed

import (
	"context"
	"fmt"
	"math/rand"
	"time"

	chat "github.com/GetStream/stream-chat-go/v5"
	"github.com/brianvoe/gofakeit/v7"
	"github.com/zfogg/sidechain/backend/internal/logger"
	"github.com/zfogg/sidechain/backend/internal/models"
	"github.com/zfogg/sidechain/backend/internal/recommendations"
	"github.com/zfogg/sidechain/backend/internal/stream"
	"go.uber.org/zap"
	"golang.org/x/crypto/bcrypt"
	"gorm.io/gorm"
)

// Seeder handles database seeding operations
type Seeder struct {
	db           *gorm.DB
	gorse        *recommendations.GorseRESTClient
	streamClient *stream.Client
}

// NewSeeder creates a new seeder instance
func NewSeeder(db *gorm.DB) *Seeder {
	// Seed random generator for reproducible results
	// Note: Seed returns an error only for invalid sources, time.Now().UnixNano() is always valid
	_ = gofakeit.Seed(time.Now().UnixNano())
	return &Seeder{db: db}
}

// SetGorseClient sets the Gorse client for syncing recommendations
func (s *Seeder) SetGorseClient(gorse *recommendations.GorseRESTClient) {
	s.gorse = gorse
}

// SetStreamClient sets the Stream.io client for creating conversations
func (s *Seeder) SetStreamClient(sc *stream.Client) {
	s.streamClient = sc
}

// SeedDev seeds the development database with realistic data
// Now creates 10x more data for better recommendation testing
func (s *Seeder) SeedDev() error {
	log := func(msg string) {
		logger.Log.Info(msg)
	}

	log("Creating users...")
	users, err := s.seedUsers(200) // 10x: 20 → 200 users
	if err != nil {
		return fmt.Errorf("failed to seed users: %w", err)
	}

	log("Creating audio posts...")
	posts, err := s.seedAudioPostsWithVariedDistribution(users, 1000) // 20x: 50 → 1000 posts
	if err != nil {
		return fmt.Errorf("failed to seed audio posts: %w", err)
	}

	log("Creating comments...")
	if err := s.seedComments(users, posts, 2000); err != nil { // 20x: 100 → 2000 comments
		return fmt.Errorf("failed to seed comments: %w", err)
	}

	log("Creating hashtags...")
	if err := s.seedHashtags(posts); err != nil {
		return fmt.Errorf("failed to seed hashtags: %w", err)
	}

	log("Creating play history...")
	if err := s.seedPlayHistory(users, posts, 5000); err != nil { // 25x: 200 → 5000 plays
		return fmt.Errorf("failed to seed play history: %w", err)
	}

	log("Creating user preferences...")
	if err := s.seedUserPreferences(users); err != nil {
		return fmt.Errorf("failed to seed user preferences: %w", err)
	}

	// Device seeding disabled - Device model not defined
	// log("Creating devices...")
	// if err := s.seedDevices(users, 150); err != nil { // 10x: 15 → 150 devices
	// return fmt.Errorf("failed to seed devices: %w", err)
	// }

	// Create conversations between users in getstream.io
	if s.streamClient != nil {
		log("Creating conversations in getstream.io...")
		if err := s.seedConversations(users); err != nil {
			return fmt.Errorf("failed to seed conversations: %w", err)
		}
	} else {
		log("Stream.io client not configured - skipping conversation seeding")
	}

	// Sync to Gorse if client is available
	if s.gorse != nil {
		log("Syncing data to Gorse...")
		if err := s.syncToGorse(users, posts); err != nil {
			return fmt.Errorf("failed to sync to Gorse: %w", err)
		}
	} else {
		log("Gorse client not configured - skipping recommendation sync")
	}

	return nil
}

// SeedTest seeds the test database with minimal data
func (s *Seeder) SeedTest() error {
	log := func(msg string) {
		logger.Log.Info(msg)
	}

	log("Creating test users...")
	// Create specific test users matching web/e2e/fixtures/test-users.ts
	testUserSpecs := []struct {
		username    string
		email       string
		displayName string
	}{
		{"alice", "alice@example.com", "Alice Smith"},
		{"bob", "bob@example.com", "Bob Johnson"},
		{"charlie", "charlie@example.com", "Charlie Brown"},
		{"diana", "diana@example.com", "Diana Prince"},
		{"eve", "eve@example.com", "Eve Wilson"},
	}

	var users []models.User
	for _, spec := range testUserSpecs {
		var user models.User
		err := s.db.Where("username = ? OR email = ?", spec.username, spec.email).First(&user).Error
		if err == nil {
			// User already exists
			users = append(users, user)
			continue
		}

		// Hash password (default: "password123" for test)
		hashedPassword, err := bcrypt.GenerateFromPassword([]byte("password123"), bcrypt.DefaultCost)
		if err != nil {
			return fmt.Errorf("failed to hash password: %w", err)
		}
		hashedPasswordStr := string(hashedPassword)

		user = models.User{
			Email:             spec.email,
			Username:          spec.username,
			DisplayName:       spec.displayName,
			PasswordHash:      &hashedPasswordStr,
			EmailVerified:     true,
			ProfilePictureURL: fmt.Sprintf("https://api.dicebear.com/7.x/avataaars/png?seed=%s", spec.username),
			DAWPreference:     "Ableton",
			Genre:             models.StringArray{"Electronic", "House"},
			FollowerCount:     0,
			FollowingCount:    0,
			PostCount:         0,
		}

		if err := s.db.Create(&user).Error; err != nil {
			return fmt.Errorf("failed to create test user %s: %w", spec.username, err)
		}

		// Create Stream.io user if client is available
		if s.streamClient != nil {
			if err := s.streamClient.CreateUser(user.ID, user.Username); err != nil {
				logger.Log.Warn("Failed to create Stream.io user",
					zap.String("username", spec.username),
					zap.Error(err))
			} else {
				// Sync user profile data to Stream.io
				customData := make(map[string]interface{})
				customData["display_name"] = user.DisplayName
				customData["profile_picture_url"] = user.ProfilePictureURL
				customData["bio"] = user.Bio
				customData["genre"] = user.Genre
				customData["daw_preference"] = user.DAWPreference

				if err := s.streamClient.UpdateUserProfile(user.ID, user.Username, customData); err != nil {
					logger.Log.Warn("Failed to sync profile data to Stream.io",
						zap.String("username", spec.username),
						zap.Error(err))
				}
			}
		}

		users = append(users, user)
	}

	if len(users) == 0 {
		return fmt.Errorf("no test users available")
	}

	log("Creating test audio posts...")
	posts, err := s.seedAudioPosts(users, 5)
	if err != nil {
		return fmt.Errorf("failed to seed audio posts: %w", err)
	}

	log("Creating test comments...")
	if err := s.seedComments(users, posts, 10); err != nil {
		return fmt.Errorf("failed to seed comments: %w", err)
	}

	// Create test conversations in getstream.io
	if s.streamClient != nil {
		log("Creating test conversations in getstream.io...")
		if err := s.seedConversations(users); err != nil {
			logger.Log.Warn("Failed to seed test conversations", zap.Error(err))
			// Don't fail the entire seed process if conversations fail
		}
	}

	return nil
}

// Clean removes all seed data (use with caution!)
func (s *Seeder) Clean() error {
	// Delete in reverse order of dependencies
	if err := s.db.Exec("DELETE FROM play_history").Error; err != nil {
		return fmt.Errorf("failed to clean play_history: %w", err)
	}
	if err := s.db.Exec("DELETE FROM user_preferences").Error; err != nil {
		return fmt.Errorf("failed to clean user_preferences: %w", err)
	}
	if err := s.db.Exec("DELETE FROM post_hashtags").Error; err != nil {
		return fmt.Errorf("failed to clean post_hashtags: %w", err)
	}
	if err := s.db.Exec("DELETE FROM hashtags").Error; err != nil {
		return fmt.Errorf("failed to clean hashtags: %w", err)
	}
	if err := s.db.Exec("DELETE FROM comment_mentions").Error; err != nil {
		return fmt.Errorf("failed to clean comment_mentions: %w", err)
	}
	if err := s.db.Exec("DELETE FROM comments").Error; err != nil {
		return fmt.Errorf("failed to clean comments: %w", err)
	}
	if err := s.db.Exec("DELETE FROM audio_posts").Error; err != nil {
		return fmt.Errorf("failed to clean audio_posts: %w", err)
	}
	if err := s.db.Exec("DELETE FROM devices").Error; err != nil {
		return fmt.Errorf("failed to clean devices: %w", err)
	}
	if err := s.db.Exec("DELETE FROM users").Error; err != nil {
		return fmt.Errorf("failed to clean users: %w", err)
	}

	return nil
}

// seedUsers creates users with realistic data
func (s *Seeder) seedUsers(count int) ([]models.User, error) {
	var users []models.User

	// Fetch any existing users first - we'll include them but also create new ones
	var existingUsers []models.User
	s.db.Find(&existingUsers)

	// Check if we already have seed users (users with @example.com email)
	var seedUserCount int64
	s.db.Model(&models.User{}).Where("email LIKE '%@example.com'").Count(&seedUserCount)
	if seedUserCount >= int64(count) {
		// Already have enough seed users, just fetch all
		if err := s.db.Find(&users).Error; err != nil {
			return nil, err
		}
		logger.Log.Info("Found existing users, skipping creation",
			zap.Int("total_users", len(users)),
			zap.Int64("seed_users", seedUserCount))
		return users, nil
	}

	genres := []string{"house", "techno", "dubstep", "trance", "drum & bass", "hip-hop", "trap", "future bass", "progressive house", "deep house"}
	daws := []string{"Ableton Live", "FL Studio", "Logic Pro", "Pro Tools", "Cubase", "Studio One", "Bitwig Studio"}

	for i := 0; i < count; i++ {
		username := gofakeit.Username()
		email := gofakeit.Email()

		// Ensure unique username/email
		var existingUser models.User
		for {
			if err := s.db.Where("username = ? OR email = ?", username, email).First(&existingUser).Error; err == gorm.ErrRecordNotFound {
				break
			}
			username = gofakeit.Username()
			email = gofakeit.Email()
		}

		// Hash password (default: "password123" for dev)
		hashedPassword, err := bcrypt.GenerateFromPassword([]byte("password123"), bcrypt.DefaultCost)
		if err != nil {
			return nil, fmt.Errorf("failed to hash password: %w", err)
		}
		hashedPasswordStr := string(hashedPassword)

		// Random genre selection
		genreCount := rand.Intn(3) + 1 // 1-3 genres
		userGenres := make([]string, 0, genreCount)
		genreMap := make(map[string]bool)
		for len(userGenres) < genreCount {
			genre := genres[rand.Intn(len(genres))]
			if !genreMap[genre] {
				genreMap[genre] = true
				userGenres = append(userGenres, genre)
			}
		}

		user := models.User{
			Email:             email,
			Username:          username,
			DisplayName:       gofakeit.Name(),
			Bio:               gofakeit.HipsterSentence(),
			Location:          fmt.Sprintf("%s, %s", gofakeit.City(), gofakeit.Country()),
			PasswordHash:      &hashedPasswordStr,
			EmailVerified:     true,
			ProfilePictureURL: fmt.Sprintf("https://api.dicebear.com/7.x/avataaars/png?seed=%s", username),
			DAWPreference:     daws[rand.Intn(len(daws))],
			Genre:             userGenres,
			FollowerCount:     rand.Intn(1000),
			FollowingCount:    rand.Intn(500),
			PostCount:         0,                    // Will be updated when posts are created
			IsOnline:          rand.Float32() < 0.3, // 30% chance of being online
		}

		// Set last active time
		lastActive := gofakeit.DateRange(time.Now().AddDate(0, 0, -30), time.Now())
		user.LastActiveAt = &lastActive

		if err := s.db.Create(&user).Error; err != nil {
			return nil, fmt.Errorf("failed to create user: %w", err)
		}

		// Sync user profile data to Stream.io if client is available
		if s.streamClient != nil {
			customData := make(map[string]interface{})
			customData["display_name"] = user.DisplayName
			customData["profile_picture_url"] = user.ProfilePictureURL
			customData["bio"] = user.Bio
			customData["genre"] = user.Genre
			customData["daw_preference"] = user.DAWPreference

			if err := s.streamClient.UpdateUserProfile(user.ID, user.Username, customData); err != nil {
				// Log but don't fail - Stream.io might not be configured
				logger.Log.Warn("Failed to sync profile data to Stream.io",
					zap.String("username", user.Username),
					zap.Error(err))
			}
		}

		users = append(users, user)
	}

	// Include existing (non-seed) users in the return list for post assignment
	users = append(users, existingUsers...)

	logger.Log.Info("Created new seed users",
		zap.Int("new_users", count),
		zap.Int("total_users", len(users)))
	return users, nil
}

// seedAudioPosts creates audio posts with realistic metadata
// Ensures each user gets at least minPostsPerUser posts, then distributes remaining randomly
func (s *Seeder) seedAudioPosts(users []models.User, count int) ([]models.AudioPost, error) {
	var posts []models.AudioPost

	if len(users) == 0 {
		return posts, nil
	}

	genres := []string{"house", "techno", "dubstep", "trance", "drum & bass", "hip-hop", "trap", "future bass"}
	keys := []string{"C", "C# ", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"}
	keys = append(keys, "Cm", "C# m", "Dm", "D#m", "Em", "Fm", "F#m", "Gm", "G#m", "Am", "A#m", "Bm")
	daws := []string{"Ableton Live", "FL Studio", "Logic Pro", "Pro Tools", "Cubase"}

	// Helper to create a post for a user
	createPost := func(user models.User) error {
		// Random duration between 4-32 seconds (typical loop length)
		duration := 4.0 + rand.Float64()*28.0
		durationBars := int(duration / 4.0) // Assuming 4/4 time

		// Random BPM between 100-180
		bpm := 100 + rand.Intn(81)

		// Random genre selection
		genreCount := rand.Intn(2) + 1 // 1-2 genres
		postGenres := make([]string, 0, genreCount)
		genreMap := make(map[string]bool)
		for len(postGenres) < genreCount {
			genre := genres[rand.Intn(len(genres))]
			if !genreMap[genre] {
				genreMap[genre] = true
				postGenres = append(postGenres, genre)
			}
		}

		// Generate a filename for this post
		generatedFilename := fmt.Sprintf("loop_%s.wav", gofakeit.Word())

		// Working test audio URLs (verified HTTP 200, kozco.com)
		testAudioURLs := []string{
			"https://www.kozco.com/tech/piano2.wav",
			"https://www.kozco.com/tech/organfinale.wav",
			"https://www.kozco.com/tech/LRMonoPhase4.wav",
			"https://www.kozco.com/tech/LRMonoPhaset4.wav",
			"https://www.kozco.com/tech/WAV-MP3.wav",
			"https://www.kozco.com/tech/c304-2.wav",
			"https://www.kozco.com/tech/32.mp3",
			"https://www.kozco.com/tech/LRMonoPhase4.mp3",
			"https://www.kozco.com/tech/organfinale.mp3",
			"https://www.kozco.com/tech/piano2-CoolEdit.mp3",
		}

		post := models.AudioPost{
			UserID:           user.ID,
			AudioURL:         testAudioURLs[rand.Intn(len(testAudioURLs))], // Use real test audio
			OriginalFilename: generatedFilename,
			Filename:         generatedFilename,                   // User-editable display filename (required)
			FileSize:         int64(rand.Intn(5000000) + 1000000), // 1-6 MB
			Duration:         duration,
			BPM:              bpm,
			Key:              keys[rand.Intn(len(keys))],
			DurationBars:     durationBars,
			DAW:              daws[rand.Intn(len(daws))],
			Genre:            postGenres,
			WaveformURL:      generateWaveformPlaceholderURL(),
			LikeCount:        rand.Intn(100),
			PlayCount:        rand.Intn(500),
			CommentCount:     0,
			StreamActivityID: gofakeit.UUID(),
			ProcessingStatus: "complete",
			IsPublic:         true,
		}

		// Random creation time within last 30 days
		createdAt := gofakeit.DateRange(time.Now().AddDate(0, 0, -30), time.Now())
		post.CreatedAt = createdAt
		post.UpdatedAt = createdAt

		if err := s.db.Create(&post).Error; err != nil {
			return fmt.Errorf("failed to create audio post: %w", err)
		}

		// Create Stream.io activity for the post if stream client is available
		if s.streamClient != nil {
			activity := &stream.Activity{
				Object:       fmt.Sprintf("loop:%s", post.ID),
				AudioURL:     post.AudioURL,
				BPM:          post.BPM,
				Key:          post.Key,
				DAW:          post.DAW,
				DurationBars: post.DurationBars,
				Genre:        post.Genre,
				WaveformURL:  post.WaveformURL,
			}
			if err := s.streamClient.CreateLoopActivity(user.ID, activity); err != nil {
				// Log but don't fail - Stream.io might not be configured in test environment
				logger.Log.Warn("Failed to create Stream.io activity for post",
					zap.String("post_id", post.ID),
					zap.Error(err))
			} else {
				// Update post with Stream activity ID
				post.StreamActivityID = activity.ID
				s.db.Model(&post).Update("stream_activity_id", activity.ID)
			}
		}

		posts = append(posts, post)

		// Update user's post count
		s.db.Model(&user).Update("post_count", gorm.Expr("post_count + 1"))
		return nil
	}

	// First, ensure each user gets at least 3 posts
	minPostsPerUser := 3
	for _, user := range users {
		// Check how many posts this user already has
		var existingCount int64
		s.db.Model(&models.AudioPost{}).Where("user_id = ? AND deleted_at IS NULL", user.ID).Count(&existingCount)

		// Create posts until they have at least minPostsPerUser
		postsNeeded := minPostsPerUser - int(existingCount)
		for j := 0; j < postsNeeded; j++ {
			if err := createPost(user); err != nil {
				return nil, err
			}
		}
	}

	// Then create remaining posts randomly distributed
	remainingPosts := count - len(posts)
	for i := 0; i < remainingPosts; i++ {
		user := users[rand.Intn(len(users))]
		if err := createPost(user); err != nil {
			return nil, err
		}
	}

	logger.Log.Info("Created audio posts",
		zap.Int("post_count", len(posts)),
		zap.Int("user_count", len(users)))
	return posts, nil
}

// seedAudioPostsWithVariedDistribution creates audio posts with realistic varied distribution
// Some users are very active (power users), most have moderate activity, some have minimal posts
func (s *Seeder) seedAudioPostsWithVariedDistribution(users []models.User, totalCount int) ([]models.AudioPost, error) {
	var posts []models.AudioPost

	if len(users) == 0 {
		return posts, nil
	}

	genres := []string{"house", "techno", "dubstep", "trance", "drum & bass", "hip-hop", "trap", "future bass"}
	keys := []string{"C", "C# ", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"}
	keys = append(keys, "Cm", "C# m", "Dm", "D#m", "Em", "Fm", "F#m", "Gm", "G#m", "Am", "A#m", "Bm")
	daws := []string{"Ableton Live", "FL Studio", "Logic Pro", "Pro Tools", "Cubase"}
	testAudioURLs := []string{
		"https://www.kozco.com/tech/piano2.wav",
		"https://www.kozco.com/tech/organfinale.wav",
		"https://www.kozco.com/tech/LRMonoPhase4.wav",
		"https://www.kozco.com/tech/LRMonoPhaset4.wav",
		"https://www.kozco.com/tech/WAV-MP3.wav",
		"https://www.kozco.com/tech/c304-2.wav",
		"https://www.kozco.com/tech/32.mp3",
		"https://www.kozco.com/tech/LRMonoPhase4.mp3",
		"https://www.kozco.com/tech/organfinale.mp3",
		"https://www.kozco.com/tech/piano2-CoolEdit.mp3",
	}

	// Helper to create a post for a user
	createPost := func(user models.User) error {
		duration := 4.0 + rand.Float64()*28.0
		durationBars := int(duration / 4.0)
		bpm := 100 + rand.Intn(81)

		genreCount := rand.Intn(2) + 1
		postGenres := make([]string, 0, genreCount)
		genreMap := make(map[string]bool)
		for len(postGenres) < genreCount {
			genre := genres[rand.Intn(len(genres))]
			if !genreMap[genre] {
				genreMap[genre] = true
				postGenres = append(postGenres, genre)
			}
		}

		generatedFilename := fmt.Sprintf("loop_%s.wav", gofakeit.Word())

		post := models.AudioPost{
			UserID:           user.ID,
			AudioURL:         testAudioURLs[rand.Intn(len(testAudioURLs))],
			OriginalFilename: generatedFilename,
			Filename:         generatedFilename,
			FileSize:         int64(rand.Intn(5000000) + 1000000),
			Duration:         duration,
			BPM:              bpm,
			Key:              keys[rand.Intn(len(keys))],
			DurationBars:     durationBars,
			DAW:              daws[rand.Intn(len(daws))],
			Genre:            postGenres,
			WaveformURL:      "",
			LikeCount:        rand.Intn(100),
			PlayCount:        rand.Intn(500),
			CommentCount:     0,
			StreamActivityID: gofakeit.UUID(),
			ProcessingStatus: "complete",
			IsPublic:         true,
		}

		createdAt := gofakeit.DateRange(time.Now().AddDate(0, 0, -30), time.Now())
		post.CreatedAt = createdAt
		post.UpdatedAt = createdAt

		if err := s.db.Create(&post).Error; err != nil {
			return fmt.Errorf("failed to create audio post: %w", err)
		}

		// Create Stream.io activity for the post if stream client is available
		if s.streamClient != nil {
			activity := &stream.Activity{
				Object:       fmt.Sprintf("loop:%s", post.ID),
				AudioURL:     post.AudioURL,
				BPM:          post.BPM,
				Key:          post.Key,
				DAW:          post.DAW,
				DurationBars: post.DurationBars,
				Genre:        post.Genre,
				WaveformURL:  post.WaveformURL,
			}
			if err := s.streamClient.CreateLoopActivity(user.ID, activity); err != nil {
				// Log but don't fail - Stream.io might not be configured in test environment
				logger.Log.Warn("Failed to create Stream.io activity for post",
					zap.String("post_id", post.ID),
					zap.Error(err))
			} else {
				// Update post with Stream activity ID
				post.StreamActivityID = activity.ID
				s.db.Model(&post).Update("stream_activity_id", activity.ID)
			}
		}

		posts = append(posts, post)
		s.db.Model(&user).Update("post_count", gorm.Expr("post_count + 1"))
		return nil
	}

	// Realistic distribution: Power Law (80/20 rule)
	// 10% of users are power users (20-50 posts each)
	// 30% of users are active (5-15 posts each)
	// 40% of users are moderate (2-5 posts each)
	// 20% of users are lurkers (0-2 posts each)

	powerUserCount := int(float64(len(users)) * 0.1)
	activeUserCount := int(float64(len(users)) * 0.3)
	moderateUserCount := int(float64(len(users)) * 0.4)
	// Rest are lurkers

	// Shuffle users for random assignment to categories
	shuffledUsers := make([]models.User, len(users))
	copy(shuffledUsers, users)
	rand.Shuffle(len(shuffledUsers), func(i, j int) {
		shuffledUsers[i], shuffledUsers[j] = shuffledUsers[j], shuffledUsers[i]
	})

	userIndex := 0
	postsCreated := 0

	// Power users: 20-50 posts each
	for i := 0; i < powerUserCount && postsCreated < totalCount; i++ {
		user := shuffledUsers[userIndex]
		userIndex++
		postCount := 20 + rand.Intn(31) // 20-50
		for j := 0; j < postCount && postsCreated < totalCount; j++ {
			if err := createPost(user); err != nil {
				return nil, err
			}
			postsCreated++
		}
	}

	// Active users: 5-15 posts each
	for i := 0; i < activeUserCount && postsCreated < totalCount; i++ {
		user := shuffledUsers[userIndex]
		userIndex++
		postCount := 5 + rand.Intn(11) // 5-15
		for j := 0; j < postCount && postsCreated < totalCount; j++ {
			if err := createPost(user); err != nil {
				return nil, err
			}
			postsCreated++
		}
	}

	// Moderate users: 2-5 posts each
	for i := 0; i < moderateUserCount && postsCreated < totalCount; i++ {
		user := shuffledUsers[userIndex]
		userIndex++
		postCount := 2 + rand.Intn(4) // 2-5
		for j := 0; j < postCount && postsCreated < totalCount; j++ {
			if err := createPost(user); err != nil {
				return nil, err
			}
			postsCreated++
		}
	}

	// Lurkers: 0-2 posts each (fill remaining posts if any)
	for userIndex < len(shuffledUsers) && postsCreated < totalCount {
		user := shuffledUsers[userIndex]
		userIndex++
		postCount := rand.Intn(3) // 0-2
		for j := 0; j < postCount && postsCreated < totalCount; j++ {
			if err := createPost(user); err != nil {
				return nil, err
			}
			postsCreated++
		}
	}

	// If we still need more posts, distribute randomly
	for postsCreated < totalCount {
		user := shuffledUsers[rand.Intn(len(shuffledUsers))]
		if err := createPost(user); err != nil {
			return nil, err
		}
		postsCreated++
	}

	logger.Log.Info("Created audio posts with varied distribution",
		zap.Int("total_posts", len(posts)),
		zap.String("distribution", "Power users (10%): ~20-50 posts, Active (30%): ~5-15 posts, Moderate (40%): ~2-5 posts, Lurkers (20%): ~0-2 posts"))
	return posts, nil
}

// seedComments creates comments on posts
func (s *Seeder) seedComments(users []models.User, posts []models.AudioPost, count int) error {
	if len(users) == 0 || len(posts) == 0 {
		return nil
	}

	commentTemplates := []string{
		"🔥 This is fire!",
		"Love the vibe on this one",
		"Perfect for my next track",
		"Great work!",
		"Can I get stems?",
		"This hits different",
		"Added to my playlist",
		"🔥🔥🔥",
		"Nice groove",
		"Would love to collab",
	}

	for i := 0; i < count; i++ {
		user := users[rand.Intn(len(users))]
		post := posts[rand.Intn(len(posts))]

		// Mix of template comments and random sentences
		var content string
		if rand.Float32() < 0.5 {
			content = commentTemplates[rand.Intn(len(commentTemplates))]
		} else {
			content = gofakeit.HipsterSentence()
		}

		comment := models.Comment{
			PostID:           post.ID,
			UserID:           user.ID,
			Content:          content,
			LikeCount:        rand.Intn(20),
			StreamActivityID: gofakeit.UUID(),
			IsEdited:         rand.Float32() < 0.1, // 10% chance of being edited
		}

		// Random creation time within last 30 days
		createdAt := gofakeit.DateRange(time.Now().AddDate(0, 0, -30), time.Now())
		comment.CreatedAt = createdAt
		comment.UpdatedAt = createdAt

		if comment.IsEdited {
			editedAt := gofakeit.DateRange(createdAt, time.Now())
			comment.EditedAt = &editedAt
		}

		if err := s.db.Create(&comment).Error; err != nil {
			return fmt.Errorf("failed to create comment: %w", err)
		}

		// Update post comment count
		s.db.Model(&post).Update("comment_count", gorm.Expr("comment_count + 1"))
	}

	logger.Log.Info("Created comments", zap.Int("count", count))
	return nil
}

// seedHashtags creates hashtags and links them to posts
func (s *Seeder) seedHashtags(posts []models.AudioPost) error {
	if len(posts) == 0 {
		return nil
	}

	hashtagNames := []string{
		"house", "techno", "dubstep", "trance", "drumandbass", "hiphop", "trap",
		"futurebass", "progressivehouse", "deephouse", "techhouse", "minimal",
		"loops", "beats", "production", "musicproduction", "daw", "ableton",
		"flstudio", "logicpro", "original", "collab", "newmusic",
	}

	hashtagMap := make(map[string]models.Hashtag)

	// Create or get hashtags
	for _, name := range hashtagNames {
		var hashtag models.Hashtag
		if err := s.db.Where("name = ?", name).First(&hashtag).Error; err == gorm.ErrRecordNotFound {
			hashtag = models.Hashtag{
				Name:       name,
				PostCount:  0,
				LastUsedAt: time.Now(),
			}
			if err := s.db.Create(&hashtag).Error; err != nil {
				return fmt.Errorf("failed to create hashtag: %w", err)
			}
		}
		hashtagMap[name] = hashtag
	}

	// Link hashtags to posts
	for _, post := range posts {
		// Each post gets 1-3 random hashtags
		hashtagCount := rand.Intn(3) + 1
		usedHashtags := make(map[string]bool)

		for i := 0; i < hashtagCount; i++ {
			name := hashtagNames[rand.Intn(len(hashtagNames))]
			if usedHashtags[name] {
				continue
			}
			usedHashtags[name] = true

			hashtag := hashtagMap[name]

			// Check if link already exists
			var existingLink models.PostHashtag
			if err := s.db.Where("post_id = ? AND hashtag_id = ?", post.ID, hashtag.ID).First(&existingLink).Error; err == gorm.ErrRecordNotFound {
				postHashtag := models.PostHashtag{
					PostID:    post.ID,
					HashtagID: hashtag.ID,
				}
				if err := s.db.Create(&postHashtag).Error; err != nil {
					return fmt.Errorf("failed to create post_hashtag: %w", err)
				}

				// Update hashtag post count
				s.db.Model(&hashtag).Update("post_count", gorm.Expr("post_count + 1"))
			}
		}
	}

	logger.Log.Info("Created and linked hashtags")
	return nil
}

// seedPlayHistory creates play history records
func (s *Seeder) seedPlayHistory(users []models.User, posts []models.AudioPost, count int) error {
	if len(users) == 0 || len(posts) == 0 {
		return nil
	}

	for i := 0; i < count; i++ {
		user := users[rand.Intn(len(users))]
		post := posts[rand.Intn(len(posts))]

		// Random duration listened (0 to full duration)
		duration := rand.Float64() * post.Duration
		completed := duration >= post.Duration*0.9 // 90%+ = completed

		playHistory := models.PlayHistory{
			UserID:    user.ID,
			PostID:    post.ID,
			Duration:  duration,
			Completed: completed,
		}

		// Random creation time within last 30 days
		createdAt := gofakeit.DateRange(time.Now().AddDate(0, 0, -30), time.Now())
		playHistory.CreatedAt = createdAt

		if err := s.db.Create(&playHistory).Error; err != nil {
			return fmt.Errorf("failed to create play history: %w", err)
		}

		// Update post play count
		s.db.Model(&post).Update("play_count", gorm.Expr("play_count + 1"))
	}

	logger.Log.Info("Created play history records", zap.Int("count", count))
	return nil
}

// seedUserPreferences creates user preferences based on play history
func (s *Seeder) seedUserPreferences(users []models.User) error {
	if len(users) == 0 {
		return nil
	}

	for _, user := range users {
		// Check if preference already exists
		var existing models.UserPreference
		if err := s.db.Where("user_id = ?", user.ID).First(&existing).Error; err == nil {
			continue // Already exists
		}

		// Generate random preferences
		genres := []string{"house", "techno", "dubstep", "trance", "drum & bass", "hip-hop", "trap"}
		genreWeights := make(map[string]float64)
		genreCount := rand.Intn(3) + 2 // 2-4 genres

		totalWeight := 0.0
		for i := 0; i < genreCount; i++ {
			genre := genres[rand.Intn(len(genres))]
			if _, exists := genreWeights[genre]; !exists {
				weight := 0.3 + rand.Float64()*0.7 // 0.3-1.0
				genreWeights[genre] = weight
				totalWeight += weight
			}
		}

		// Normalize weights
		for genre := range genreWeights {
			genreWeights[genre] = genreWeights[genre] / totalWeight
		}

		// Random BPM range
		minBPM := 100 + rand.Intn(30)
		maxBPM := minBPM + 20 + rand.Intn(40)

		keys := []string{"C", "C# ", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"}
		keyCount := rand.Intn(3) + 1
		keyPreferences := make([]string, 0, keyCount)
		keyMap := make(map[string]bool)
		for len(keyPreferences) < keyCount {
			key := keys[rand.Intn(len(keys))]
			if !keyMap[key] {
				keyMap[key] = true
				keyPreferences = append(keyPreferences, key)
			}
		}

		bpmRange := struct {
			Min int `json:"min"`
			Max int `json:"max"`
		}{
			Min: minBPM,
			Max: maxBPM,
		}

		preference := models.UserPreference{
			UserID:         user.ID,
			GenreWeights:   genreWeights,
			BPMRange:       &bpmRange,
			KeyPreferences: models.StringArray(keyPreferences),
		}

		if err := s.db.Create(&preference).Error; err != nil {
			return fmt.Errorf("failed to create user preference: %w", err)
		}
	}

	logger.Log.Info("Created user preferences")
	return nil
}


// seedConversations creates direct message and group conversations in getstream.io
func (s *Seeder) seedConversations(users []models.User) error {
	if s.streamClient == nil || s.streamClient.ChatClient == nil {
		return fmt.Errorf("stream chat client not configured")
	}

	if len(users) < 2 {
		return fmt.Errorf("need at least 2 users to create conversations")
	}

	ctx := context.Background()
	dmCreated := 0
	groupCreated := 0
	failed := 0

	// Shuffle users for random pairing
	shuffledUsers := make([]models.User, len(users))
	copy(shuffledUsers, users)
	rand.Shuffle(len(shuffledUsers), func(i, j int) {
		shuffledUsers[i], shuffledUsers[j] = shuffledUsers[j], shuffledUsers[i]
	})

	// Create direct message conversations between random pairs
	dmToCreate := 2 * len(users)
	if dmToCreate > len(users)*(len(users)-1)/2 {
		dmToCreate = len(users) * (len(users) - 1) / 2
	}

	for i := 0; i < len(shuffledUsers)-1 && dmCreated < dmToCreate; i++ {
		user1 := shuffledUsers[i]
		user2 := shuffledUsers[i+1]

		// Generate a direct message channel ID
		var channelID string
		if user1.ID < user2.ID {
			channelID = "dm-" + user1.ID[:8] + "-" + user2.ID[:8]
		} else {
			channelID = "dm-" + user2.ID[:8] + "-" + user1.ID[:8]
		}

		// Create the channel using GetStream SDK
		channelReq := &chat.ChannelRequest{
			Members: []string{user1.ID, user2.ID},
		}

		response, err := s.streamClient.ChatClient.CreateChannel(ctx, "messaging", channelID, user1.ID, channelReq)
		if err != nil {
			failed++
			continue
		}

		if response == nil || response.Channel == nil {
			failed++
			continue
		}

		// Send an initial welcome message
		msg := &chat.Message{
			Text: fmt.Sprintf("👋 DM started between %s and %s", user1.Username, user2.Username),
		}

		_, err = response.Channel.SendMessage(ctx, msg, user1.ID)
		if err != nil {
			failed++
			continue
		}

		dmCreated++
	}

	// Create group conversations with 3-5 random members
	groupsToCreate := int(float64(len(users)) * 0.5) // Create roughly 50% as many groups as users
	if groupsToCreate < 1 {
		groupsToCreate = 1
	}

	for i := 0; i < groupsToCreate; i++ {
		// Pick 3-5 random members for the group
		memberCount := 3 + rand.Intn(3) // 3-5 members
		if memberCount > len(shuffledUsers) {
			memberCount = len(shuffledUsers)
		}

		// Randomly select members
		memberIndices := rand.Perm(len(shuffledUsers))[:memberCount]
		var groupMembers []string
		var groupMemberNames []string

		for _, idx := range memberIndices {
			groupMembers = append(groupMembers, shuffledUsers[idx].ID)
			groupMemberNames = append(groupMemberNames, shuffledUsers[idx].Username)
		}

		// Generate group channel ID
		groupChannelID := fmt.Sprintf("group-%d", i)

		// Create the group channel
		channelReq := &chat.ChannelRequest{
			Members: groupMembers,
			ExtraData: map[string]interface{}{
				"name": fmt.Sprintf("🎵 %s's Music Collab", shuffledUsers[memberIndices[0]].Username),
			},
		}

		response, err := s.streamClient.ChatClient.CreateChannel(ctx, "messaging", groupChannelID, shuffledUsers[memberIndices[0]].ID, channelReq)
		if err != nil {
			failed++
			continue
		}

		if response == nil || response.Channel == nil {
			failed++
			continue
		}

		// Send a group welcome message
		groupMsg := &chat.Message{
			Text: fmt.Sprintf("👋 Group conversation started with: %s", joinStrings(groupMemberNames, ", ")),
		}

		_, err = response.Channel.SendMessage(ctx, groupMsg, shuffledUsers[memberIndices[0]].ID)
		if err != nil {
			failed++
			continue
		}

		groupCreated++
	}

	logger.Log.Info("Created conversations",
		zap.Int("direct_conversations", dmCreated),
		zap.Int("group_conversations", groupCreated),
		zap.Int("failed", failed))
	return nil
}

// joinStrings joins a slice of strings with a separator
func joinStrings(strs []string, sep string) string {
	result := ""
	for i, s := range strs {
		if i > 0 {
			result += sep
		}
		result += s
	}
	return result
}

// generateWaveformPlaceholderURL generates a placeholder waveform URL for seed data
func generateWaveformPlaceholderURL() string {
	// Return empty string - waveforms should be generated from actual audio
	return ""
}

// syncToGorse syncs all seed data to Gorse for recommendations
func (s *Seeder) syncToGorse(users []models.User, posts []models.AudioPost) error {
	if s.gorse == nil {
		return fmt.Errorf("Gorse client not configured")
	}

	// Sync users
	logger.Log.Info("Syncing users to Gorse", zap.Int("user_count", len(users)))
	for _, user := range users {
		if err := s.gorse.SyncUser(user.ID); err != nil {
			logger.Log.Warn("Failed to sync user to Gorse",
				zap.String("user_id", user.ID),
				zap.Error(err))
			// Continue syncing others even if one fails
		}
	}

	// Sync posts (items)
	logger.Log.Info("Syncing posts to Gorse", zap.Int("post_count", len(posts)))
	for _, post := range posts {
		if err := s.gorse.SyncItem(post.ID); err != nil {
			logger.Log.Warn("Failed to sync post to Gorse",
				zap.String("post_id", post.ID),
				zap.Error(err))
			// Continue syncing others even if one fails
		}
	}

	// Sync feedback (play history)
	var playHistory []models.PlayHistory
	if err := s.db.Find(&playHistory).Error; err != nil {
		return fmt.Errorf("failed to fetch play history: %w", err)
	}

	logger.Log.Info("Syncing feedback events to Gorse", zap.Int("event_count", len(playHistory)))
	for _, play := range playHistory {
		feedbackType := "view"
		if play.Completed {
			feedbackType = "like" // Completed plays are strong positive signals
		}

		if err := s.gorse.SyncFeedback(play.UserID, play.PostID, feedbackType); err != nil {
			logger.Log.Warn("Failed to sync feedback to Gorse",
				zap.String("user_id", play.UserID),
				zap.String("post_id", play.PostID),
				zap.Error(err))
			// Continue syncing others even if one fails
		}
	}

	// Sync likes (additional feedback)
	// Find all posts with like_count > 0 and create like feedback
	// Note: In production, we'd track individual likes in a separate table
	// For seed data, we'll create synthetic like feedback based on like_count
	logger.Log.Info("Generating like feedback from post engagement")
	likeCount := 0
	for _, post := range posts {
		if post.LikeCount > 0 {
			// Randomly assign likes to users (simplified for seed data)
			likesNeeded := post.LikeCount
			if likesNeeded > len(users) {
				likesNeeded = len(users) // Can't have more likes than users
			}

			// Shuffle users and assign first N as likers
			shuffledUsers := make([]models.User, len(users))
			copy(shuffledUsers, users)
			rand.Shuffle(len(shuffledUsers), func(i, j int) {
				shuffledUsers[i], shuffledUsers[j] = shuffledUsers[j], shuffledUsers[i]
			})

			for i := 0; i < likesNeeded; i++ {
				if err := s.gorse.SyncFeedback(shuffledUsers[i].ID, post.ID, "like"); err != nil {
					// Silently continue on error to avoid spam
					continue
				}
				likeCount++
			}
		}
	}
	logger.Log.Info("Generated like feedback events", zap.Int("count", likeCount))

	logger.Log.Info("Gorse sync complete",
		zap.Int("users", len(users)),
		zap.Int("posts", len(posts)),
		zap.Int("feedback_events", len(playHistory)+likeCount))

	return nil
}
