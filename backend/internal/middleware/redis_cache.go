package middleware

import (
	"context"
	"crypto/sha256"
	"encoding/hex"
	"fmt"
	"time"

	"github.com/zfogg/sidechain/backend/internal/cache"
	"github.com/zfogg/sidechain/backend/internal/logger"
	"go.uber.org/zap"
)

// CacheManager provides utilities for Redis-based caching
type CacheManager struct {
	client *cache.RedisClient
}

// NewCacheManager creates a new cache manager
func NewCacheManager(client *cache.RedisClient) *CacheManager {
	return &CacheManager{client: client}
}

// CacheKey generates a cache key with prefix and hash
func CacheKey(prefix string, values ...string) string {
	keyStr := prefix
	for _, v := range values {
		keyStr += ":" + v
	}
	return keyStr
}

// GetCached attempts to retrieve a value from cache
// Returns (value, found, error)
func (cm *CacheManager) GetCached(ctx context.Context, key string) (string, bool, error) {
	if cm == nil || cm.client == nil {
		return "", false, nil // Cache miss, not an error
	}

	val, err := cm.client.Get(ctx, key)
	if err != nil {
		if err.Error() == "redis: nil" {
			return "", false, nil // Cache miss
		}
		logger.Log.Debug("Cache retrieval failed",
			zap.String("key", key),
			zap.Error(err),
		)
		return "", false, err // Cache error
	}

	logger.Log.Debug("Cache hit",
		zap.String("key", key),
	)
	return val, true, nil
}

// SetCached stores a value in cache with TTL
func (cm *CacheManager) SetCached(ctx context.Context, key string, value string, ttl time.Duration) error {
	if cm == nil || cm.client == nil {
		return nil // Skip caching if not available
	}

	if err := cm.client.SetEx(ctx, key, value, ttl); err != nil {
		logger.Log.Debug("Cache write failed",
			zap.String("key", key),
			zap.Error(err),
		)
		return err
	}

	logger.Log.Debug("Cache write successful",
		zap.String("key", key),
		zap.Duration("ttl", ttl),
	)
	return nil
}

// InvalidateCache invalidates one or more cache keys
func (cm *CacheManager) InvalidateCache(ctx context.Context, keys ...string) error {
	if cm == nil || cm.client == nil || len(keys) == 0 {
		return nil
	}

	if err := cm.client.Del(ctx, keys...); err != nil {
		logger.Log.Debug("Cache invalidation failed",
			zap.Strings("keys", keys),
			zap.Error(err),
		)
		return err
	}

	logger.Log.Debug("Cache invalidation successful",
		zap.Strings("keys", keys),
	)
	return nil
}

// InvalidateFeedCache invalidates all feed-related caches for a user
func (cm *CacheManager) InvalidateFeedCache(ctx context.Context, userID string) error {
	if cm == nil || cm.client == nil {
		return nil
	}

	// Invalidate user-specific feed caches
	pattern := fmt.Sprintf("feed:*:%s", userID)
	keys, err := cm.client.Keys(ctx, pattern)
	if err != nil {
		logger.Log.Debug("Failed to find feed cache keys for invalidation",
			zap.String("user_id", userID),
			zap.Error(err),
		)
		return err
	}

	if len(keys) > 0 {
		return cm.InvalidateCache(ctx, keys...)
	}
	return nil
}

// HashToken creates a SHA256 hash of a token for safe key storage
func HashToken(token string) string {
	hash := sha256.Sum256([]byte(token))
	return hex.EncodeToString(hash[:])
}

// GetCacheStats returns cache statistics for monitoring
func (cm *CacheManager) GetCacheStats(ctx context.Context, prefix string) map[string]interface{} {
	if cm == nil || cm.client == nil {
		return map[string]interface{}{"available": false}
	}

	// Count keys with prefix
	pattern := fmt.Sprintf("%s:*", prefix)
	keys, err := cm.client.Keys(ctx, pattern)

	stats := map[string]interface{}{
		"available": true,
		"prefix": prefix,
	}

	if err == nil {
		stats["key_count"] = len(keys)
	} else {
		stats["error"] = err.Error()
	}

	return stats
}
