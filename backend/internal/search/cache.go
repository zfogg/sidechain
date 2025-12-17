package search

import (
	"context"
	"crypto/md5"
	"encoding/json"
	"fmt"
	"os"
	"time"

	"github.com/redis/go-redis/v9"
)

// CachedClient wraps the search client with Redis caching (Phase 6.1)
type CachedClient struct {
	client *Client
	redis  *redis.Client
	ttl    time.Duration
}

// NewCachedClient creates a new search client with Redis caching
func NewCachedClient(searchClient *Client) (*CachedClient, error) {
	// Get Redis URL from environment
	redisURL := os.Getenv("REDIS_URL")
	if redisURL == "" {
		redisURL = "redis://localhost:6379"
	}

	// Parse Redis URL and create client
	opt, err := redis.ParseURL(redisURL)
	if err != nil {
		// If Redis is unavailable, return client without caching
		fmt.Printf("Warning: Failed to connect to Redis, search will work without caching: %v\n", err)
		return &CachedClient{
			client: searchClient,
			redis:  nil,
			ttl:    5 * time.Minute, // Default TTL even if Redis unavailable
		}, nil
	}

	rdb := redis.NewClient(opt)

	// Test connection
	_, err = rdb.Ping(context.Background()).Result()
	if err != nil {
		fmt.Printf("Warning: Redis ping failed, search will work without caching: %v\n", err)
		return &CachedClient{
			client: searchClient,
			redis:  nil,
			ttl:    5 * time.Minute,
		}, nil
	}

	return &CachedClient{
		client: searchClient,
		redis:  rdb,
		ttl:    5 * time.Minute, // 5 minute cache TTL for search results
	}, nil
}

// cacheKey generates a cache key for the search query
func (c *CachedClient) cacheKey(prefix string, params interface{}) string {
	data, _ := json.Marshal(params)
	hash := md5.Sum(data)
	return fmt.Sprintf("search:%s:%x", prefix, hash)
}

// SearchPosts searches for posts with caching
func (c *CachedClient) SearchPosts(ctx context.Context, params SearchPostsParams) (*SearchPostsResult, error) {
	// If Redis is unavailable, fall back to direct search
	if c.redis == nil {
		return c.client.SearchPosts(ctx, params)
	}

	// Try to get from cache
	cacheKey := c.cacheKey("posts", params)
	cached, err := c.redis.Get(ctx, cacheKey).Result()
	if err == nil {
		// Cache hit - parse and return
		var result SearchPostsResult
		if err := json.Unmarshal([]byte(cached), &result); err == nil {
			return &result, nil
		}
	}

	// Cache miss - perform search
	result, err := c.client.SearchPosts(ctx, params)
	if err != nil {
		return result, err
	}

	// Store in cache
	if result != nil {
		if data, err := json.Marshal(result); err == nil {
			c.redis.Set(ctx, cacheKey, data, c.ttl)
		}
	}

	return result, nil
}

// SearchUsers searches for users with caching
func (c *CachedClient) SearchUsers(ctx context.Context, query string, limit, offset int) (*SearchUsersResult, error) {
	// If Redis is unavailable, fall back to direct search
	if c.redis == nil {
		return c.client.SearchUsers(ctx, query, limit, offset)
	}

	// Try to get from cache
	cacheParams := map[string]interface{}{
		"query":  query,
		"limit":  limit,
		"offset": offset,
	}
	cacheKey := c.cacheKey("users", cacheParams)
	cached, err := c.redis.Get(ctx, cacheKey).Result()
	if err == nil {
		// Cache hit - parse and return
		var result SearchUsersResult
		if err := json.Unmarshal([]byte(cached), &result); err == nil {
			return &result, nil
		}
	}

	// Cache miss - perform search
	result, err := c.client.SearchUsers(ctx, query, limit, offset)
	if err != nil {
		return result, err
	}

	// Store in cache
	if result != nil {
		if data, err := json.Marshal(result); err == nil {
			c.redis.Set(ctx, cacheKey, data, c.ttl)
		}
	}

	return result, nil
}

// SearchStories searches for stories with caching (Phase 6.1)
func (c *CachedClient) SearchStories(ctx context.Context, query string, limit, offset int) (*SearchStoriesResult, error) {
	// If Redis is unavailable, fall back to direct search
	if c.redis == nil {
		return c.client.SearchStories(ctx, query, limit, offset)
	}

	// Try to get from cache
	cacheParams := map[string]interface{}{
		"query":  query,
		"limit":  limit,
		"offset": offset,
	}
	cacheKey := c.cacheKey("stories", cacheParams)
	cached, err := c.redis.Get(ctx, cacheKey).Result()
	if err == nil {
		// Cache hit - parse and return
		var result SearchStoriesResult
		if err := json.Unmarshal([]byte(cached), &result); err == nil {
			return &result, nil
		}
	}

	// Cache miss - perform search
	result, err := c.client.SearchStories(ctx, query, limit, offset)
	if err != nil {
		return result, err
	}

	// Store in cache
	if result != nil {
		if data, err := json.Marshal(result); err == nil {
			c.redis.Set(ctx, cacheKey, data, c.ttl)
		}
	}

	return result, nil
}

// InvalidatePostCache invalidates cache for posts (used when posts are updated)
func (c *CachedClient) InvalidatePostCache(ctx context.Context) error {
	if c.redis == nil {
		return nil
	}
	// Invalidate all post search cache entries
	iter := c.redis.Scan(ctx, 0, "search:posts:*", 0).Iterator()
	for iter.Next(ctx) {
		c.redis.Del(ctx, iter.Val())
	}
	return iter.Err()
}

// InvalidateUserCache invalidates cache for users
func (c *CachedClient) InvalidateUserCache(ctx context.Context) error {
	if c.redis == nil {
		return nil
	}
	// Invalidate all user search cache entries
	iter := c.redis.Scan(ctx, 0, "search:users:*", 0).Iterator()
	for iter.Next(ctx) {
		c.redis.Del(ctx, iter.Val())
	}
	return iter.Err()
}

// InvalidateStoryCache invalidates cache for stories
func (c *CachedClient) InvalidateStoryCache(ctx context.Context) error {
	if c.redis == nil {
		return nil
	}
	// Invalidate all story search cache entries
	iter := c.redis.Scan(ctx, 0, "search:stories:*", 0).Iterator()
	for iter.Next(ctx) {
		c.redis.Del(ctx, iter.Val())
	}
	return iter.Err()
}
