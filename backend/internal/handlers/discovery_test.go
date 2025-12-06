package handlers

import (
	"encoding/json"
	"fmt"
	"net/http"
	"net/http/httptest"
	"time"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"github.com/zfogg/sidechain/backend/internal/models"
)

// =============================================================================
// USER SEARCH TESTS
// =============================================================================

func (suite *HandlersTestSuite) TestSearchUsers() {
	t := suite.T()

	// Create searchable users
	for i := 0; i < 5; i++ {
		user := &models.User{
			Email:        fmt.Sprintf("searchuser%d_%d@test.com", i, time.Now().UnixNano()),
			Username:     fmt.Sprintf("beatmaker%d", i),
			DisplayName:  fmt.Sprintf("Beat Maker %d", i),
			StreamUserID: fmt.Sprintf("stream_search_%d_%d", i, time.Now().UnixNano()),
		}
		require.NoError(t, suite.db.Create(user).Error)
	}

	req, _ := http.NewRequest("GET", "/api/search/users?q=beatmaker", nil)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)

	users := response["users"].([]interface{})
	assert.GreaterOrEqual(t, len(users), 5)

	meta := response["meta"].(map[string]interface{})
	assert.Equal(t, "beatmaker", meta["query"])
}

func (suite *HandlersTestSuite) TestSearchUsersMissingQuery() {
	t := suite.T()

	req, _ := http.NewRequest("GET", "/api/search/users", nil)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusBadRequest, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Equal(t, "search_query_required", response["error"])
}

func (suite *HandlersTestSuite) TestSearchUsersPagination() {
	t := suite.T()

	// Create many users
	for i := 0; i < 30; i++ {
		user := &models.User{
			Email:        fmt.Sprintf("producer%d_%d@test.com", i, time.Now().UnixNano()),
			Username:     fmt.Sprintf("producer%d", i),
			DisplayName:  fmt.Sprintf("Producer %d", i),
			StreamUserID: fmt.Sprintf("stream_producer_%d_%d", i, time.Now().UnixNano()),
		}
		require.NoError(t, suite.db.Create(user).Error)
	}

	req, _ := http.NewRequest("GET", "/api/search/users?q=producer&limit=10&offset=0", nil)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)

	users := response["users"].([]interface{})
	assert.LessOrEqual(t, len(users), 10)

	meta := response["meta"].(map[string]interface{})
	assert.Equal(t, float64(10), meta["limit"])
	assert.Equal(t, float64(0), meta["offset"])
}

// =============================================================================
// DISCOVERY TESTS
// =============================================================================

func (suite *HandlersTestSuite) TestGetTrendingUsers() {
	t := suite.T()

	// Create users with posts
	for i := 0; i < 5; i++ {
		user := &models.User{
			Email:         fmt.Sprintf("trending%d_%d@test.com", i, time.Now().UnixNano()),
			Username:      fmt.Sprintf("trendinguser%d", i),
			DisplayName:   fmt.Sprintf("Trending User %d", i),
			StreamUserID:  fmt.Sprintf("stream_trending_%d_%d", i, time.Now().UnixNano()),
			FollowerCount: (5 - i) * 100,
		}
		require.NoError(t, suite.db.Create(user).Error)
	}

	req, _ := http.NewRequest("GET", "/api/discover/trending?period=week&limit=10", nil)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)

	assert.Equal(t, "week", response["period"])
	assert.NotNil(t, response["users"])
}

func (suite *HandlersTestSuite) TestGetFeaturedProducers() {
	t := suite.T()

	// Create featured-worthy users (high follower count, active recently, has posts)
	now := time.Now()
	for i := 0; i < 3; i++ {
		user := &models.User{
			Email:         fmt.Sprintf("featured%d_%d@test.com", i, time.Now().UnixNano()),
			Username:      fmt.Sprintf("featureduser%d", i),
			DisplayName:   fmt.Sprintf("Featured User %d", i),
			StreamUserID:  fmt.Sprintf("stream_featured_%d_%d", i, time.Now().UnixNano()),
			FollowerCount: 100 + i*50,
			PostCount:     5 + i,
			LastActiveAt:  &now,
		}
		require.NoError(t, suite.db.Create(user).Error)
	}

	req, _ := http.NewRequest("GET", "/api/discover/featured?limit=10", nil)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)

	assert.NotNil(t, response["users"])
}

func (suite *HandlersTestSuite) TestGetUsersByGenre() {
	t := suite.T()

	// Create users with specific genre (using raw SQL for PostgreSQL array)
	for i := 0; i < 5; i++ {
		user := &models.User{
			Email:         fmt.Sprintf("houseuser%d_%d@test.com", i, time.Now().UnixNano()),
			Username:      fmt.Sprintf("houseprod%d", i),
			DisplayName:   fmt.Sprintf("House Producer %d", i),
			StreamUserID:  fmt.Sprintf("stream_house_%d_%d", i, time.Now().UnixNano()),
			FollowerCount: i * 10,
		}
		require.NoError(t, suite.db.Create(user).Error)
		suite.db.Exec("UPDATE users SET genre = '{House,Techno}' WHERE id = ?", user.ID)
	}

	req, _ := http.NewRequest("GET", "/api/discover/genre/House", nil)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)

	assert.Equal(t, "House", response["genre"])
	users := response["users"].([]interface{})
	assert.GreaterOrEqual(t, len(users), 5)
}

func (suite *HandlersTestSuite) TestGetAvailableGenres() {
	t := suite.T()

	// Create users with various genres (using raw SQL for PostgreSQL array)
	genres := []string{
		"{Electronic,House}",
		"{House,Techno}",
		"{Hip-Hop,Trap}",
		"{Electronic,Ambient}",
	}

	for i, g := range genres {
		user := &models.User{
			Email:        fmt.Sprintf("genreuser%d_%d@test.com", i, time.Now().UnixNano()),
			Username:     fmt.Sprintf("genreuser%d", i),
			DisplayName:  fmt.Sprintf("Genre User %d", i),
			StreamUserID: fmt.Sprintf("stream_genre_%d_%d", i, time.Now().UnixNano()),
		}
		require.NoError(t, suite.db.Create(user).Error)
		suite.db.Exec("UPDATE users SET genre = '"+g+"' WHERE id = ?", user.ID)
	}

	req, _ := http.NewRequest("GET", "/api/discover/genres", nil)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)

	assert.NotNil(t, response["genres"])
}

func (suite *HandlersTestSuite) TestGetSimilarUsersNoPosts() {
	t := suite.T()

	req, _ := http.NewRequest("GET", "/api/users/"+suite.testUser.ID+"/similar", nil)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)

	assert.Equal(t, "no_posts_to_analyze", response["reason"])
}

func (suite *HandlersTestSuite) TestGetSimilarUsersWithPosts() {
	t := suite.T()

	// Create posts for the test user with unique StreamActivityID
	for i := 0; i < 5; i++ {
		post := &models.AudioPost{
			UserID:           suite.testUser.ID,
			AudioURL:         fmt.Sprintf("https://example.com/audio%d.mp3", i),
			OriginalFilename: fmt.Sprintf("loop%d.mp3", i),
			BPM:              120 + i*5,
			Key:              "C minor",
			ProcessingStatus: "complete",
			IsPublic:         true,
			StreamActivityID: fmt.Sprintf("stream_activity_%d_%d", i, time.Now().UnixNano()),
		}
		require.NoError(t, suite.db.Create(post).Error)
	}

	// Create another user with similar posts
	otherID := fmt.Sprintf("%d", time.Now().UnixNano())
	otherUser := &models.User{
		Email:        fmt.Sprintf("similar_%s@test.com", otherID),
		Username:     fmt.Sprintf("similaruser%s", otherID[:8]),
		DisplayName:  "Similar User",
		StreamUserID: fmt.Sprintf("stream_similar_%s", otherID),
	}
	require.NoError(t, suite.db.Create(otherUser).Error)

	for i := 0; i < 3; i++ {
		post := &models.AudioPost{
			UserID:           otherUser.ID,
			AudioURL:         fmt.Sprintf("https://example.com/similar%d.mp3", i),
			OriginalFilename: fmt.Sprintf("similar%d.mp3", i),
			BPM:              125, // Within range
			Key:              "C minor",
			ProcessingStatus: "complete",
			IsPublic:         true,
			StreamActivityID: fmt.Sprintf("stream_activity_similar_%d_%d", i, time.Now().UnixNano()),
		}
		require.NoError(t, suite.db.Create(post).Error)
	}

	req, _ := http.NewRequest("GET", "/api/users/"+suite.testUser.ID+"/similar?limit=5", nil)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)

	// Should have BPM and key analysis
	assert.NotNil(t, response["analyzed_bpm"])
	assert.NotNil(t, response["analyzed_key"])
}
