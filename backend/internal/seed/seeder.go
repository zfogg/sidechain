package seed

import (
	"fmt"
	"math/rand"
	"time"

	"github.com/brianvoe/gofakeit/v7"
	"github.com/zfogg/sidechain/backend/internal/models"
	"golang.org/x/crypto/bcrypt"
	"gorm.io/gorm"
)

// Seeder handles database seeding operations
type Seeder struct {
	db *gorm.DB
}

// NewSeeder creates a new seeder instance
func NewSeeder(db *gorm.DB) *Seeder {
	// Seed random generator for reproducible results
	gofakeit.Seed(time.Now().UnixNano())
	return &Seeder{db: db}
}

// SeedDev seeds the development database with realistic data
func (s *Seeder) SeedDev() error {
	log := func(msg string) {
		fmt.Printf("  %s\n", msg)
	}

	log("Creating users...")
	users, err := s.seedUsers(20) // Create 20 users
	if err != nil {
		return fmt.Errorf("failed to seed users: %w", err)
	}

	log("Creating audio posts...")
	posts, err := s.seedAudioPosts(users, 50) // 50 posts across users
	if err != nil {
		return fmt.Errorf("failed to seed audio posts: %w", err)
	}

	log("Creating comments...")
	if err := s.seedComments(users, posts, 100); err != nil {
		return fmt.Errorf("failed to seed comments: %w", err)
	}

	log("Creating hashtags...")
	if err := s.seedHashtags(posts); err != nil {
		return fmt.Errorf("failed to seed hashtags: %w", err)
	}

	log("Creating play history...")
	if err := s.seedPlayHistory(users, posts, 200); err != nil {
		return fmt.Errorf("failed to seed play history: %w", err)
	}

	log("Creating user preferences...")
	if err := s.seedUserPreferences(users); err != nil {
		return fmt.Errorf("failed to seed user preferences: %w", err)
	}

	log("Creating devices...")
	if err := s.seedDevices(users, 15); err != nil {
		return fmt.Errorf("failed to seed devices: %w", err)
	}

	return nil
}

// SeedTest seeds the test database with minimal data
func (s *Seeder) SeedTest() error {
	log := func(msg string) {
		fmt.Printf("  %s\n", msg)
	}

	log("Creating test users...")
	users, err := s.seedUsers(3)
	if err != nil {
		return fmt.Errorf("failed to seed users: %w", err)
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
		fmt.Printf("    Found %d existing users (including %d seed users), skipping creation\n", len(users), seedUserCount)
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
			Email:          email,
			Username:       username,
			DisplayName:    gofakeit.Name(),
			Bio:            gofakeit.HipsterSentence(),
			Location:       fmt.Sprintf("%s, %s", gofakeit.City(), gofakeit.Country()),
			PasswordHash:   &hashedPasswordStr,
			EmailVerified:  true,
			ProfilePictureURL: fmt.Sprintf("https://api.dicebear.com/7.x/avataaars/svg?seed=%s", gofakeit.Username()),
			DAWPreference:  daws[rand.Intn(len(daws))],
			Genre:          userGenres,
			FollowerCount:  rand.Intn(1000),
			FollowingCount: rand.Intn(500),
			PostCount:      0,                    // Will be updated when posts are created
			IsOnline:       rand.Float32() < 0.3, // 30% chance of being online
		}

		// Set last active time
		lastActive := gofakeit.DateRange(time.Now().AddDate(0, 0, -30), time.Now())
		user.LastActiveAt = &lastActive

		if err := s.db.Create(&user).Error; err != nil {
			return nil, fmt.Errorf("failed to create user: %w", err)
		}

		users = append(users, user)
	}

	// Include existing (non-seed) users in the return list for post assignment
	users = append(users, existingUsers...)

	fmt.Printf("    Created %d new seed users, total %d users available\n", count, len(users))
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
	keys := []string{"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"}
	keys = append(keys, "Cm", "C#m", "Dm", "D#m", "Em", "Fm", "F#m", "Gm", "G#m", "Am", "A#m", "Bm")
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

		post := models.AudioPost{
			UserID:           user.ID,
			AudioURL:         fmt.Sprintf("https://cdn.sidechain.app/audio/%s.mp3", gofakeit.UUID()),
			OriginalFilename: generatedFilename,
			Filename:         generatedFilename, // User-editable display filename (required)
			FileSize:         int64(rand.Intn(5000000) + 1000000), // 1-6 MB
			Duration:         duration,
			BPM:              bpm,
			Key:              keys[rand.Intn(len(keys))],
			DurationBars:     durationBars,
			DAW:              daws[rand.Intn(len(daws))],
			Genre:            postGenres,
			WaveformSVG:      generateWaveformSVG(),
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

	fmt.Printf("    Created %d audio posts across %d users\n", len(posts), len(users))
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

	fmt.Printf("    Created %d comments\n", count)
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

	fmt.Printf("    Created and linked hashtags\n")
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

	fmt.Printf("    Created %d play history records\n", count)
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

		keys := []string{"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"}
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

	fmt.Printf("    Created user preferences\n")
	return nil
}

// seedDevices creates device records
func (s *Seeder) seedDevices(users []models.User, count int) error {
	if len(users) == 0 {
		return nil
	}

	platforms := []string{"macOS", "Windows", "Linux"}
	daws := []string{"Ableton Live", "FL Studio", "Logic Pro", "Pro Tools", "Cubase", "Studio One"}
	dawVersions := []string{"11.0", "12.0", "13.0", "2024.1", "2024.2"}

	for i := 0; i < count; i++ {
		user := users[rand.Intn(len(users))]

		device := models.Device{
			UserID:            &user.ID,
			DeviceFingerprint: gofakeit.UUID(),
			Platform:          platforms[rand.Intn(len(platforms))],
			DAW:               daws[rand.Intn(len(daws))],
			DAWVersion:        dawVersions[rand.Intn(len(dawVersions))],
			IsActive:          true,
		}

		lastUsed := gofakeit.DateRange(time.Now().AddDate(0, 0, -7), time.Now())
		device.LastUsedAt = &lastUsed

		if err := s.db.Create(&device).Error; err != nil {
			return fmt.Errorf("failed to create device: %w", err)
		}
	}

	fmt.Printf("    Created %d devices\n", count)
	return nil
}

// generateWaveformSVG generates a simple SVG waveform placeholder
func generateWaveformSVG() string {
	// Simple waveform with random peaks
	points := make([]string, 100)
	for i := 0; i < 100; i++ {
		height := rand.Intn(50) + 10
		points[i] = fmt.Sprintf("%d,%d", i*10, height)
	}
	pointsStr := ""
	for i, p := range points {
		if i > 0 {
			pointsStr += " "
		}
		pointsStr += p
	}
	return fmt.Sprintf(`<svg viewBox="0 0 1000 100"><polyline points="%s" fill="none" stroke="#000" stroke-width="2"/></svg>`, pointsStr)
}
