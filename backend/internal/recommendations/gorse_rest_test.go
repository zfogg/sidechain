package recommendations

import (
	"encoding/json"
	"fmt"
	"net/http"
	"net/http/httptest"
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"github.com/zfogg/sidechain/backend/internal/models"
	"gorm.io/driver/sqlite"
	"gorm.io/gorm"
)

// setupTestDB creates an in-memory SQLite database for testing
func setupTestDB(t *testing.T) *gorm.DB {
	db, err := gorm.Open(sqlite.Open(":memory:"), &gorm.Config{
		DisableForeignKeyConstraintWhenMigrating: true,
	})
	require.NoError(t, err)

	// Create tables manually with SQLite-compatible syntax
	// (GORM AutoMigrate tries to use PostgreSQL-specific features like gen_random_uuid)
	err = db.Exec(`
		CREATE TABLE users (
			id TEXT PRIMARY KEY,
			email TEXT NOT NULL,
			username TEXT NOT NULL,
			display_name TEXT NOT NULL,
			bio TEXT,
			location TEXT,
			password_hash TEXT,
			email_verified INTEGER DEFAULT 0,
			email_verify_token TEXT,
			google_id TEXT,
			discord_id TEXT,
			oauth_profile_picture_url TEXT,
			profile_picture_url TEXT,
			daw_preference TEXT,
			genre TEXT,
			social_links TEXT,
			follower_count INTEGER DEFAULT 0,
			following_count INTEGER DEFAULT 0,
			post_count INTEGER DEFAULT 0,
			last_active_at DATETIME,
			is_online INTEGER DEFAULT 0,
			stream_user_id TEXT,
			stream_token TEXT,
			created_at DATETIME,
			updated_at DATETIME,
			deleted_at DATETIME
		)
	`).Error
	require.NoError(t, err)

	err = db.Exec(`
		CREATE TABLE audio_posts (
			id TEXT PRIMARY KEY,
			user_id TEXT NOT NULL,
			audio_url TEXT NOT NULL,
			original_filename TEXT,
			file_size INTEGER,
			duration REAL,
			bpm INTEGER,
			key TEXT,
			duration_bars INTEGER,
			daw TEXT,
			genre TEXT,
			waveform_url TEXT,
			like_count INTEGER DEFAULT 0,
			play_count INTEGER DEFAULT 0,
			comment_count INTEGER DEFAULT 0,
			download_count INTEGER DEFAULT 0,
			stream_activity_id TEXT,
			processing_status TEXT DEFAULT 'pending',
			is_public INTEGER DEFAULT 1,
			created_at DATETIME,
			updated_at DATETIME,
			deleted_at DATETIME
		)
	`).Error
	require.NoError(t, err)

	err = db.Exec(`
		CREATE TABLE play_histories (
			id TEXT PRIMARY KEY,
			user_id TEXT NOT NULL,
			post_id TEXT NOT NULL,
			duration REAL DEFAULT 0,
			completed INTEGER DEFAULT 0,
			created_at DATETIME
		)
	`).Error
	require.NoError(t, err)

	return db
}

// createTestUser creates a test user in the database
func createTestUser(t *testing.T, db *gorm.DB, id, username string) *models.User {
	// Check if user already exists
	var existingUser models.User
	err := db.Raw("SELECT * FROM users WHERE id = ?", id).Scan(&existingUser).Error
	if err == nil && existingUser.ID != "" {
		// User exists, return it
		return &existingUser
	}

	// Use StringArray type for genre
	genreValue, _ := models.StringArray{"Electronic", "House"}.Value()
	user := &models.User{
		ID:          id,
		Username:    username,
		DisplayName: username + " Display",
		Email:       username + "@test.com",
		Genre:       models.StringArray{"Electronic", "House"},
		CreatedAt:   time.Now(),
		UpdatedAt:   time.Now(),
	}
	// Insert using raw SQL to handle genre properly
	err = db.Exec(`
		INSERT INTO users (id, email, username, display_name, genre, created_at, updated_at)
		VALUES (?, ?, ?, ?, ?, ?, ?)
	`, user.ID, user.Email, user.Username, user.DisplayName, genreValue, user.CreatedAt, user.UpdatedAt).Error
	require.NoError(t, err)
	// Reload to get proper Genre field
	err = db.Raw("SELECT * FROM users WHERE id = ?", id).Scan(user).Error
	require.NoError(t, err)
	return user
}

// createTestPost creates a test post in the database
func createTestPost(t *testing.T, db *gorm.DB, id, userID string) *models.AudioPost {
	user := createTestUser(t, db, userID, "testuser")
	genreValue, _ := models.StringArray{"House", "Techno"}.Value()
	post := &models.AudioPost{
		ID:        id,
		UserID:    userID,
		User:      *user,
		AudioURL:  "https://example.com/audio.mp3",
		BPM:       128,
		Key:       "C major",
		Genre:     models.StringArray{"House", "Techno"},
		IsPublic:  true,
		CreatedAt: time.Now(),
		UpdatedAt: time.Now(),
	}
	// Insert using raw SQL to handle genre properly and avoid missing columns
	err := db.Exec(`
		INSERT INTO audio_posts (id, user_id, audio_url, bpm, key, genre, is_public, created_at, updated_at)
		VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
	`, post.ID, post.UserID, post.AudioURL, post.BPM, post.Key, genreValue, 1, post.CreatedAt, post.UpdatedAt).Error
	require.NoError(t, err)
	// Reload to get proper Genre field
	err = db.Raw("SELECT * FROM audio_posts WHERE id = ?", id).Scan(post).Error
	require.NoError(t, err)
	return post
}

func TestNewGorseRESTClient(t *testing.T) {
	db := setupTestDB(t)
	client := NewGorseRESTClient("http://localhost:8087", "test-api-key", db)

	assert.NotNil(t, client)
	assert.Equal(t, "http://localhost:8087", client.baseURL)
	assert.Equal(t, "test-api-key", client.apiKey)
	assert.NotNil(t, client.client)
	assert.Equal(t, 10*time.Second, client.client.Timeout)
}

func TestSyncUser(t *testing.T) {
	db := setupTestDB(t)
	user := createTestUser(t, db, "user123", "testuser")

	// Create mock Gorse server
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		assert.Equal(t, "POST", r.Method)
		assert.Equal(t, "/api/user", r.URL.Path)
		assert.Equal(t, "test-api-key", r.Header.Get("X-API-Key"))
		assert.Equal(t, "application/json", r.Header.Get("Content-Type"))

		// Verify request body
		var receivedUser GorseUser
		err := json.NewDecoder(r.Body).Decode(&receivedUser)
		require.NoError(t, err)

		assert.Equal(t, "user123", receivedUser.UserId)
		assert.NotNil(t, receivedUser.Labels)
		assert.Equal(t, []interface{}{"Electronic", "House"}, receivedUser.Labels["genres"])
		assert.Contains(t, receivedUser.Comment, "testuser")

		w.WriteHeader(http.StatusOK)
		json.NewEncoder(w).Encode(map[string]interface{}{"RowAffected": 1})
	}))
	defer server.Close()

	client := NewGorseRESTClient(server.URL, "test-api-key", db)
	err := client.SyncUser(user.ID)
	assert.NoError(t, err)
}

func TestSyncUser_NotFound(t *testing.T) {
	db := setupTestDB(t)
	client := NewGorseRESTClient("http://localhost:8087", "test-api-key", db)

	err := client.SyncUser("nonexistent")
	assert.Error(t, err)
	assert.Contains(t, err.Error(), "user not found")
}

func TestSyncItem(t *testing.T) {
	db := setupTestDB(t)
	post := createTestPost(t, db, "post123", "user123")

	// Create mock Gorse server
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		assert.Equal(t, "POST", r.Method)
		assert.Equal(t, "/api/item", r.URL.Path)
		assert.Equal(t, "test-api-key", r.Header.Get("X-API-Key"))

		// Verify request body
		var receivedItem GorseItem
		err := json.NewDecoder(r.Body).Decode(&receivedItem)
		require.NoError(t, err)

		assert.Equal(t, "post123", receivedItem.ItemId)
		assert.False(t, receivedItem.IsHidden)
		assert.Contains(t, receivedItem.Categories, "House")
		assert.Contains(t, receivedItem.Categories, "Techno")
		assert.Contains(t, receivedItem.Categories, "bpm_128")
		assert.Contains(t, receivedItem.Categories, "key_C major")
		assert.NotNil(t, receivedItem.Labels)
		assert.Equal(t, []interface{}{"House", "Techno"}, receivedItem.Labels["genres"])
		assert.Equal(t, float64(128), receivedItem.Labels["bpm"])
		assert.Equal(t, "C major", receivedItem.Labels["key"])

		w.WriteHeader(http.StatusOK)
		json.NewEncoder(w).Encode(map[string]interface{}{"RowAffected": 1})
	}))
	defer server.Close()

	client := NewGorseRESTClient(server.URL, "test-api-key", db)
	err := client.SyncItem(post.ID)
	assert.NoError(t, err)
}

func TestSyncItem_NotFound(t *testing.T) {
	db := setupTestDB(t)
	client := NewGorseRESTClient("http://localhost:8087", "test-api-key", db)

	err := client.SyncItem("nonexistent")
	assert.Error(t, err)
	assert.Contains(t, err.Error(), "post not found")
}

func TestSyncItem_Hidden(t *testing.T) {
	db := setupTestDB(t)
	user := createTestUser(t, db, "user123", "testuser")
	post := &models.AudioPost{
		ID:        "post123",
		UserID:    "user123",
		User:      *user,
		AudioURL:  "https://example.com/audio.mp3",
		IsPublic:  false, // Hidden post
		CreatedAt: time.Now(),
		UpdatedAt: time.Now(),
	}
	// Insert using raw SQL
	err := db.Exec(`
		INSERT INTO audio_posts (id, user_id, audio_url, is_public, created_at, updated_at)
		VALUES (?, ?, ?, ?, ?, ?)
	`, post.ID, post.UserID, post.AudioURL, 0, post.CreatedAt, post.UpdatedAt).Error
	require.NoError(t, err)
	// Reload
	err = db.Raw("SELECT * FROM audio_posts WHERE id = ?", post.ID).Scan(post).Error
	require.NoError(t, err)

	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		var receivedItem GorseItem
		json.NewDecoder(r.Body).Decode(&receivedItem)
		assert.True(t, receivedItem.IsHidden) // Should be hidden
		w.WriteHeader(http.StatusOK)
		json.NewEncoder(w).Encode(map[string]interface{}{"RowAffected": 1})
	}))
	defer server.Close()

	client := NewGorseRESTClient(server.URL, "test-api-key", db)
	err = client.SyncItem(post.ID)
	assert.NoError(t, err)
}

func TestSyncFeedback(t *testing.T) {
	db := setupTestDB(t)

	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		assert.Equal(t, "POST", r.Method)
		assert.Equal(t, "/api/feedback", r.URL.Path)
		assert.Equal(t, "test-api-key", r.Header.Get("X-API-Key"))

		// Verify request body
		var receivedFeedback []GorseFeedback
		err := json.NewDecoder(r.Body).Decode(&receivedFeedback)
		require.NoError(t, err)

		require.Len(t, receivedFeedback, 1)
		assert.Equal(t, "like", receivedFeedback[0].FeedbackType)
		assert.Equal(t, "user123", receivedFeedback[0].UserId)
		assert.Equal(t, "post123", receivedFeedback[0].ItemId)
		assert.NotEmpty(t, receivedFeedback[0].Timestamp)

		w.WriteHeader(http.StatusOK)
		json.NewEncoder(w).Encode(map[string]interface{}{"RowAffected": 1})
	}))
	defer server.Close()

	client := NewGorseRESTClient(server.URL, "test-api-key", db)
	err := client.SyncFeedback("user123", "post123", "like")
	assert.NoError(t, err)
}

func TestGetForYouFeed(t *testing.T) {
	db := setupTestDB(t)
	_ = createTestPost(t, db, "post1", "user1")
	_ = createTestPost(t, db, "post2", "user2")

	// Create mock Gorse server
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		assert.Equal(t, "GET", r.Method)
		assert.Equal(t, "/api/recommend/user123", r.URL.Path)
		assert.Equal(t, "n=10", r.URL.RawQuery)
		assert.Equal(t, "test-api-key", r.Header.Get("X-API-Key"))

		// Return item IDs
		itemIDs := []string{"post1", "post2", "post3"}
		json.NewEncoder(w).Encode(itemIDs)
	}))
	defer server.Close()

	client := NewGorseRESTClient(server.URL, "test-api-key", db)
	scores, err := client.GetForYouFeed("user123", 10, 0)
	assert.NoError(t, err)
	assert.Len(t, scores, 2) // Only post1 and post2 exist in DB
	assert.Equal(t, "post1", scores[0].Post.ID)
	assert.Equal(t, "post2", scores[1].Post.ID)
	assert.NotEmpty(t, scores[0].Reason)
}

func TestGetForYouFeed_WithOffset(t *testing.T) {
	db := setupTestDB(t)
	_ = createTestPost(t, db, "post1", "user1")
	_ = createTestPost(t, db, "post2", "user2")
	_ = createTestPost(t, db, "post3", "user3")

	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		// Return 5 item IDs
		itemIDs := []string{"post1", "post2", "post3", "post4", "post5"}
		json.NewEncoder(w).Encode(itemIDs)
	}))
	defer server.Close()

	client := NewGorseRESTClient(server.URL, "test-api-key", db)
	scores, err := client.GetForYouFeed("user123", 2, 1) // limit=2, offset=1
	assert.NoError(t, err)
	assert.Len(t, scores, 2) // Should skip first item and return next 2
	assert.Equal(t, "post2", scores[0].Post.ID)
	assert.Equal(t, "post3", scores[1].Post.ID)
}

func TestGetForYouFeed_Empty(t *testing.T) {
	db := setupTestDB(t)

	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		json.NewEncoder(w).Encode([]string{})
	}))
	defer server.Close()

	client := NewGorseRESTClient(server.URL, "test-api-key", db)
	scores, err := client.GetForYouFeed("user123", 10, 0)
	assert.NoError(t, err)
	assert.Empty(t, scores)
}

func TestGetForYouFeed_Error(t *testing.T) {
	db := setupTestDB(t)

	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.WriteHeader(http.StatusInternalServerError)
	}))
	defer server.Close()

	client := NewGorseRESTClient(server.URL, "test-api-key", db)
	scores, err := client.GetForYouFeed("user123", 10, 0)
	assert.Error(t, err)
	assert.Nil(t, scores)
	assert.Contains(t, err.Error(), "gorse API error")
}

func TestGetSimilarPosts(t *testing.T) {
	db := setupTestDB(t)
	_ = createTestPost(t, db, "post1", "user1")
	_ = createTestPost(t, db, "post2", "user2")

	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		assert.Equal(t, "GET", r.Method)
		assert.Equal(t, "/api/item/post123/neighbors", r.URL.Path)
		assert.Equal(t, "n=5", r.URL.RawQuery)

		// Return scores
		scores := []map[string]interface{}{
			{"Id": "post1", "Score": 0.95},
			{"Id": "post2", "Score": 0.85},
		}
		json.NewEncoder(w).Encode(scores)
	}))
	defer server.Close()

	client := NewGorseRESTClient(server.URL, "test-api-key", db)
	posts, err := client.GetSimilarPosts("post123", 5)
	assert.NoError(t, err)
	assert.Len(t, posts, 2)
	assert.Equal(t, "post1", posts[0].ID)
	assert.Equal(t, "post2", posts[1].ID)
}

func TestGetSimilarPosts_Empty(t *testing.T) {
	db := setupTestDB(t)

	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		json.NewEncoder(w).Encode([]map[string]interface{}{})
	}))
	defer server.Close()

	client := NewGorseRESTClient(server.URL, "test-api-key", db)
	posts, err := client.GetSimilarPosts("post123", 5)
	assert.NoError(t, err)
	assert.Empty(t, posts)
}

func TestGetSimilarUsers(t *testing.T) {
	db := setupTestDB(t)
	_ = createTestUser(t, db, "user1", "user1")
	_ = createTestUser(t, db, "user2", "user2")

	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		assert.Equal(t, "GET", r.Method)
		assert.Equal(t, "/api/user/user123/neighbors", r.URL.Path)
		assert.Equal(t, "n=5", r.URL.RawQuery)

		// Return user scores
		scores := []map[string]interface{}{
			{"Id": "user1", "Score": 0.95},
			{"Id": "user2", "Score": 0.85},
		}
		json.NewEncoder(w).Encode(scores)
	}))
	defer server.Close()

	client := NewGorseRESTClient(server.URL, "test-api-key", db)
	users, err := client.GetSimilarUsers("user123", 5)
	assert.NoError(t, err)
	assert.Len(t, users, 2)
	assert.Equal(t, "user1", users[0].ID)
	assert.Equal(t, "user2", users[1].ID)
}

func TestGetSimilarUsers_Empty(t *testing.T) {
	db := setupTestDB(t)

	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		json.NewEncoder(w).Encode([]map[string]interface{}{})
	}))
	defer server.Close()

	client := NewGorseRESTClient(server.URL, "test-api-key", db)
	users, err := client.GetSimilarUsers("user123", 5)
	assert.NoError(t, err)
	assert.Empty(t, users)
}

func TestGetSimilarUsers_Error(t *testing.T) {
	db := setupTestDB(t)

	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.WriteHeader(http.StatusNotFound)
	}))
	defer server.Close()

	client := NewGorseRESTClient(server.URL, "test-api-key", db)
	users, err := client.GetSimilarUsers("user123", 5)
	assert.NoError(t, err) // Should return empty, not error
	assert.Empty(t, users)
}

func TestBatchSyncUsers(t *testing.T) {
	db := setupTestDB(t)
	_ = createTestUser(t, db, "user1", "user1")
	_ = createTestUser(t, db, "user2", "user2")

	var receivedUsers []GorseUser
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		assert.Equal(t, "POST", r.Method)
		assert.Equal(t, "/api/users", r.URL.Path) // Batch endpoint

		err := json.NewDecoder(r.Body).Decode(&receivedUsers)
		require.NoError(t, err)

		assert.Len(t, receivedUsers, 2)
		assert.Equal(t, "user1", receivedUsers[0].UserId)
		assert.Equal(t, "user2", receivedUsers[1].UserId)

		w.WriteHeader(http.StatusOK)
		json.NewEncoder(w).Encode(map[string]interface{}{"RowAffected": 2})
	}))
	defer server.Close()

	client := NewGorseRESTClient(server.URL, "test-api-key", db)
	err := client.BatchSyncUsers()
	assert.NoError(t, err)
}

func TestBatchSyncUsers_Empty(t *testing.T) {
	db := setupTestDB(t)

	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		var receivedUsers []GorseUser
		json.NewDecoder(r.Body).Decode(&receivedUsers)
		assert.Empty(t, receivedUsers)
		w.WriteHeader(http.StatusOK)
		json.NewEncoder(w).Encode(map[string]interface{}{"RowAffected": 0})
	}))
	defer server.Close()

	client := NewGorseRESTClient(server.URL, "test-api-key", db)
	err := client.BatchSyncUsers()
	assert.NoError(t, err)
}

func TestBatchSyncItems(t *testing.T) {
	db := setupTestDB(t)
	_ = createTestPost(t, db, "post1", "user1")
	_ = createTestPost(t, db, "post2", "user2")

	var receivedItems []GorseItem
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		assert.Equal(t, "POST", r.Method)
		assert.Equal(t, "/api/items", r.URL.Path) // Batch endpoint

		err := json.NewDecoder(r.Body).Decode(&receivedItems)
		require.NoError(t, err)

		assert.Len(t, receivedItems, 2)
		assert.Equal(t, "post1", receivedItems[0].ItemId)
		assert.Equal(t, "post2", receivedItems[1].ItemId)

		// Verify labels format
		assert.NotNil(t, receivedItems[0].Labels)
		assert.Equal(t, []interface{}{"House", "Techno"}, receivedItems[0].Labels["genres"])

		w.WriteHeader(http.StatusOK)
		json.NewEncoder(w).Encode(map[string]interface{}{"RowAffected": 2})
	}))
	defer server.Close()

	client := NewGorseRESTClient(server.URL, "test-api-key", db)
	err := client.BatchSyncItems()
	assert.NoError(t, err)
}

func TestBatchSyncItems_OnlyPublic(t *testing.T) {
	db := setupTestDB(t)
	user := createTestUser(t, db, "user1", "user1")

	// Create public post
	publicPost := &models.AudioPost{
		ID:        "public1",
		UserID:    "user1",
		User:      *user,
		AudioURL:  "https://example.com/audio.mp3",
		IsPublic:  true,
		CreatedAt: time.Now(),
		UpdatedAt: time.Now(),
	}
	err := db.Exec(`
		INSERT INTO audio_posts (id, user_id, audio_url, is_public, created_at, updated_at)
		VALUES (?, ?, ?, ?, ?, ?)
	`, publicPost.ID, publicPost.UserID, publicPost.AudioURL, 1, publicPost.CreatedAt, publicPost.UpdatedAt).Error
	require.NoError(t, err)

	// Create private post
	privatePost := &models.AudioPost{
		ID:        "private1",
		UserID:    "user1",
		User:      *user,
		AudioURL:  "https://example.com/audio2.mp3",
		IsPublic:  false,
		CreatedAt: time.Now(),
		UpdatedAt: time.Now(),
	}
	err = db.Exec(`
		INSERT INTO audio_posts (id, user_id, audio_url, is_public, created_at, updated_at)
		VALUES (?, ?, ?, ?, ?, ?)
	`, privatePost.ID, privatePost.UserID, privatePost.AudioURL, 0, privatePost.CreatedAt, privatePost.UpdatedAt).Error
	require.NoError(t, err)

	var receivedItems []GorseItem
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		json.NewDecoder(r.Body).Decode(&receivedItems)
		// Should only receive public post
		assert.Len(t, receivedItems, 1)
		assert.Equal(t, "public1", receivedItems[0].ItemId)
		w.WriteHeader(http.StatusOK)
		json.NewEncoder(w).Encode(map[string]interface{}{"RowAffected": 1})
	}))
	defer server.Close()

	client := NewGorseRESTClient(server.URL, "test-api-key", db)
	err = client.BatchSyncItems()
	assert.NoError(t, err)
}

func TestBatchSyncFeedback(t *testing.T) {
	db := setupTestDB(t)
	_ = createTestUser(t, db, "user1", "user1")
	_ = createTestPost(t, db, "post1", "user1")

	// Create play history
	playHistory := []models.PlayHistory{
		{
			ID:        "ph1",
			UserID:    "user1",
			PostID:    "post1",
			Duration:  60.0,
			Completed: true,
			CreatedAt: time.Now(),
		},
		{
			ID:        "ph2",
			UserID:    "user1",
			PostID:    "post1",
			Duration:  30.0,
			Completed: false,
			CreatedAt: time.Now(),
		},
	}
	for _, ph := range playHistory {
		db.Exec(`
			INSERT INTO play_histories (id, user_id, post_id, duration, completed, created_at)
			VALUES (?, ?, ?, ?, ?, ?)
		`, ph.ID, ph.UserID, ph.PostID, ph.Duration, ph.Completed, ph.CreatedAt)
	}

	var receivedFeedbacks []GorseFeedback
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		assert.Equal(t, "POST", r.Method)
		assert.Equal(t, "/api/feedback", r.URL.Path)

		err := json.NewDecoder(r.Body).Decode(&receivedFeedbacks)
		require.NoError(t, err)

		// Should receive 2 feedbacks
		assert.Len(t, receivedFeedbacks, 2)
		// First one completed = "like"
		assert.Equal(t, "like", receivedFeedbacks[0].FeedbackType)
		// Second one not completed = "view"
		assert.Equal(t, "view", receivedFeedbacks[1].FeedbackType)

		w.WriteHeader(http.StatusOK)
		json.NewEncoder(w).Encode(map[string]interface{}{"RowAffected": 2})
	}))
	defer server.Close()

	client := NewGorseRESTClient(server.URL, "test-api-key", db)
	err := client.BatchSyncFeedback()
	assert.NoError(t, err)
}

func TestBatchSyncFeedback_Empty(t *testing.T) {
	db := setupTestDB(t)

	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		// Should not be called if no feedback
		t.Error("Should not call API if no feedback")
	}))
	defer server.Close()

	client := NewGorseRESTClient(server.URL, "test-api-key", db)
	err := client.BatchSyncFeedback()
	assert.NoError(t, err) // Should succeed even with no feedback
}

func TestBatchSyncFeedback_Batching(t *testing.T) {
	db := setupTestDB(t)
	_ = createTestUser(t, db, "user1", "user1")
	_ = createTestPost(t, db, "post1", "user1")

	// Create 150 play history records (more than batch size of 100)
	for i := 0; i < 150; i++ {
		ph := models.PlayHistory{
			ID:        fmt.Sprintf("ph%d", i),
			UserID:    "user1",
			PostID:    "post1",
			Duration:  60.0,
			Completed: true,
			CreatedAt: time.Now(),
		}
		db.Create(&ph)
	}

	callCount := 0
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		callCount++
		var receivedFeedbacks []GorseFeedback
		json.NewDecoder(r.Body).Decode(&receivedFeedbacks)
		// First batch should have 100, second should have 50
		if callCount == 1 {
			assert.Len(t, receivedFeedbacks, 100)
		} else {
			assert.Len(t, receivedFeedbacks, 50)
		}
		w.WriteHeader(http.StatusOK)
		json.NewEncoder(w).Encode(map[string]interface{}{"RowAffected": len(receivedFeedbacks)})
	}))
	defer server.Close()

	client := NewGorseRESTClient(server.URL, "test-api-key", db)
	err := client.BatchSyncFeedback()
	assert.NoError(t, err)
	assert.Equal(t, 2, callCount) // Should be called twice
}

func TestMakeRequest_ErrorHandling(t *testing.T) {
	db := setupTestDB(t)

	// Test with invalid URL
	client := NewGorseRESTClient("http://invalid-url-that-does-not-exist:9999", "test-api-key", db)
	err := client.SyncFeedback("user1", "post1", "like")
	assert.Error(t, err)
	assert.Contains(t, err.Error(), "request failed")
}

func TestMakeRequest_HTTPError(t *testing.T) {
	db := setupTestDB(t)

	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.WriteHeader(http.StatusBadRequest)
		json.NewEncoder(w).Encode(map[string]interface{}{"error": "bad request"})
	}))
	defer server.Close()

	client := NewGorseRESTClient(server.URL, "test-api-key", db)
	err := client.SyncFeedback("user1", "post1", "like")
	assert.Error(t, err)
	assert.Contains(t, err.Error(), "gorse API error")
}

func TestGenerateReason(t *testing.T) {
	db := setupTestDB(t)
	client := NewGorseRESTClient("http://localhost:8087", "test-api-key", db)

	tests := []struct {
		name     string
		post     models.AudioPost
		expected string
	}{
		{
			name: "with genre",
			post: models.AudioPost{
				Genre: []string{"House"},
			},
			expected: "matches your genre preferences",
		},
		{
			name: "with genre and BPM",
			post: models.AudioPost{
				Genre: []string{"House"},
				BPM:   128,
			},
			expected: "matches your genre preferences and matches your BPM range",
		},
		{
			name: "popular post",
			post: models.AudioPost{
				LikeCount: 15,
			},
			expected: "popular with other producers",
		},
		{
			name: "recent post",
			post: models.AudioPost{
				CreatedAt: time.Now().Add(-1 * time.Hour),
			},
			expected: "recently posted",
		},
		{
			name:     "no attributes",
			post:     models.AudioPost{},
			expected: "trending",
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			reason := client.generateReason(tt.post)
			assert.Contains(t, reason, tt.expected)
		})
	}
}

func TestLabelsFormat(t *testing.T) {
	db := setupTestDB(t)
	user := createTestUser(t, db, "user1", "user1")

	// Test that labels are properly formatted as map
	var receivedUser GorseUser
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		json.NewDecoder(r.Body).Decode(&receivedUser)
		w.WriteHeader(http.StatusOK)
		json.NewEncoder(w).Encode(map[string]interface{}{"RowAffected": 1})
	}))
	defer server.Close()

	client := NewGorseRESTClient(server.URL, "test-api-key", db)
	err := client.SyncUser(user.ID)
	require.NoError(t, err)

	// Verify labels is a map, not a slice
	assert.IsType(t, map[string]interface{}(nil), receivedUser.Labels)
	assert.NotNil(t, receivedUser.Labels["genres"])
}

func TestItemLabelsFormat(t *testing.T) {
	db := setupTestDB(t)
	post := createTestPost(t, db, "post1", "user1")

	var receivedItem GorseItem
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		json.NewDecoder(r.Body).Decode(&receivedItem)
		w.WriteHeader(http.StatusOK)
		json.NewEncoder(w).Encode(map[string]interface{}{"RowAffected": 1})
	}))
	defer server.Close()

	client := NewGorseRESTClient(server.URL, "test-api-key", db)
	err := client.SyncItem(post.ID)
	require.NoError(t, err)

	// Verify labels is a map with proper structure
	assert.IsType(t, map[string]interface{}(nil), receivedItem.Labels)
	assert.Equal(t, []interface{}{"House", "Techno"}, receivedItem.Labels["genres"])
	assert.Equal(t, float64(128), receivedItem.Labels["bpm"])
	assert.Equal(t, "C major", receivedItem.Labels["key"])
}

func TestGetForYouFeed_NonExistentItems(t *testing.T) {
	db := setupTestDB(t)
	_ = createTestPost(t, db, "post1", "user1")

	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		// Return item IDs, some don't exist in DB
		itemIDs := []string{"post1", "nonexistent1", "nonexistent2"}
		json.NewEncoder(w).Encode(itemIDs)
	}))
	defer server.Close()

	client := NewGorseRESTClient(server.URL, "test-api-key", db)
	scores, err := client.GetForYouFeed("user123", 10, 0)
	assert.NoError(t, err)
	// Should only return post1, skip nonexistent items
	assert.Len(t, scores, 1)
	assert.Equal(t, "post1", scores[0].Post.ID)
}

func TestGetForYouFeed_MaintainsOrder(t *testing.T) {
	db := setupTestDB(t)
	_ = createTestPost(t, db, "post1", "user1")
	_ = createTestPost(t, db, "post2", "user2")
	_ = createTestPost(t, db, "post3", "user3")

	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		// Return in specific order
		itemIDs := []string{"post3", "post1", "post2"}
		json.NewEncoder(w).Encode(itemIDs)
	}))
	defer server.Close()

	client := NewGorseRESTClient(server.URL, "test-api-key", db)
	scores, err := client.GetForYouFeed("user123", 10, 0)
	assert.NoError(t, err)
	// Should maintain Gorse's order
	assert.Len(t, scores, 3)
	assert.Equal(t, "post3", scores[0].Post.ID)
	assert.Equal(t, "post1", scores[1].Post.ID)
	assert.Equal(t, "post2", scores[2].Post.ID)
}
