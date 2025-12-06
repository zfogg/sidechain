package middleware

import (
	"net/http"
	"net/http/httptest"
	"testing"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/stretchr/testify/assert"
)

func TestRateLimiter(t *testing.T) {
	gin.SetMode(gin.TestMode)

	config := RateLimitConfig{
		Limit:  3,
		Window: time.Second,
		KeyFunc: func(c *gin.Context) string {
			return c.ClientIP()
		},
	}

	limiter := NewRateLimiter(config)

	router := gin.New()
	router.Use(limiter)
	router.GET("/test", func(c *gin.Context) {
		c.JSON(http.StatusOK, gin.H{"status": "ok"})
	})

	// First 3 requests should succeed
	for i := 0; i < 3; i++ {
		req := httptest.NewRequest("GET", "/test", nil)
		w := httptest.NewRecorder()
		router.ServeHTTP(w, req)
		assert.Equal(t, http.StatusOK, w.Code, "Request %d should succeed", i+1)
	}

	// 4th request should be rate limited
	req := httptest.NewRequest("GET", "/test", nil)
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)
	assert.Equal(t, http.StatusTooManyRequests, w.Code, "4th request should be rate limited")

	// Wait for window to expire
	time.Sleep(time.Second + 100*time.Millisecond)

	// Should work again after window expires
	req = httptest.NewRequest("GET", "/test", nil)
	w = httptest.NewRecorder()
	router.ServeHTTP(w, req)
	assert.Equal(t, http.StatusOK, w.Code, "Request after window should succeed")
}

func TestRateLimiterDifferentClients(t *testing.T) {
	gin.SetMode(gin.TestMode)

	config := RateLimitConfig{
		Limit:  2,
		Window: time.Second,
		KeyFunc: func(c *gin.Context) string {
			return c.GetHeader("X-Client-ID")
		},
	}

	limiter := NewRateLimiter(config)

	router := gin.New()
	router.Use(limiter)
	router.GET("/test", func(c *gin.Context) {
		c.JSON(http.StatusOK, gin.H{"status": "ok"})
	})

	// Client A makes 2 requests (at limit)
	for i := 0; i < 2; i++ {
		req := httptest.NewRequest("GET", "/test", nil)
		req.Header.Set("X-Client-ID", "client-a")
		w := httptest.NewRecorder()
		router.ServeHTTP(w, req)
		assert.Equal(t, http.StatusOK, w.Code)
	}

	// Client A is now rate limited
	req := httptest.NewRequest("GET", "/test", nil)
	req.Header.Set("X-Client-ID", "client-a")
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)
	assert.Equal(t, http.StatusTooManyRequests, w.Code, "Client A should be rate limited")

	// Client B should still work
	req = httptest.NewRequest("GET", "/test", nil)
	req.Header.Set("X-Client-ID", "client-b")
	w = httptest.NewRecorder()
	router.ServeHTTP(w, req)
	assert.Equal(t, http.StatusOK, w.Code, "Client B should not be rate limited")
}

func TestDefaultConfigs(t *testing.T) {
	// Test that default configs are reasonable
	defaultConfig := DefaultRateLimitConfig()
	assert.Equal(t, 100, defaultConfig.Limit)
	assert.Equal(t, time.Minute, defaultConfig.Window)
	assert.NotNil(t, defaultConfig.KeyFunc)

	authConfig := AuthRateLimitConfig()
	assert.Equal(t, 10, authConfig.Limit)
	assert.Equal(t, time.Minute, authConfig.Window)

	uploadConfig := UploadRateLimitConfig()
	assert.Equal(t, 20, uploadConfig.Limit)
	assert.Equal(t, time.Minute, uploadConfig.Window)
}
