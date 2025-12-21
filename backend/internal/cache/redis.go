package cache

import (
	"context"
	"fmt"
	"time"

	"github.com/redis/go-redis/v9"
	"github.com/zfogg/sidechain/backend/internal/logger"
	"github.com/zfogg/sidechain/backend/internal/metrics"
	"go.opentelemetry.io/otel"
	"go.opentelemetry.io/otel/attribute"
	"go.opentelemetry.io/otel/codes"
	"go.uber.org/zap"
)

// RedisClient wraps the redis.Client with centralized connection pooling
type RedisClient struct {
	client *redis.Client
}

// Singleton instance (package-level)
var globalRedis *RedisClient

// NewRedisClient creates and initializes a Redis client with connection pooling
// Requires REDIS_HOST and optionally REDIS_PORT, REDIS_PASSWORD environment variables
func NewRedisClient(host string, port string, password string) (*RedisClient, error) {
	if host == "" {
		host = "localhost"
	}
	if port == "" {
		port = "6379"
	}

	addr := fmt.Sprintf("%s:%s", host, port)

	// Create client with connection pooling
	client := redis.NewClient(&redis.Options{
		Addr:           addr,
		Password:       password,
		DB:             0,
		MaxRetries:     3,
		PoolSize:       10,
		MinIdleConns:   5,
		ReadTimeout:    3 * time.Second,
		WriteTimeout:   3 * time.Second,
		DialTimeout:    5 * time.Second,
	})

	// Test connection
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()

	if err := client.Ping(ctx).Err(); err != nil {
		logger.ErrorWithFields("Failed to connect to Redis", err)
		return nil, err
	}

	rc := &RedisClient{client: client}
	globalRedis = rc

	logger.Log.Info("✅ Redis client connected successfully",
		zap.String("address", addr),
	)

	return rc, nil
}

// GetRedisClient returns the global Redis client instance
func GetRedisClient() *RedisClient {
	return globalRedis
}

// Close closes the Redis connection gracefully
func (rc *RedisClient) Close() error {
	if rc == nil || rc.client == nil {
		return nil
	}
	return rc.client.Close()
}

// Get retrieves a value from Redis
func (rc *RedisClient) Get(ctx context.Context, key string) (string, error) {
	// Start distributed tracing span
	_, span := otel.Tracer("redis").Start(ctx, "redis.get",
	)
	defer span.End()

	// Add span attributes
	span.SetAttributes(
		attribute.String("cache.key", maskSensitiveKey(key)),
		attribute.String("cache.operation", "get"),
	)

	start := time.Now()
	result, err := rc.client.Get(ctx, key).Result()

	// METRICS-2: Record Redis operation timing
	duration := time.Since(start).Seconds()
	metrics.Get().RedisOperationDuration.WithLabelValues("get", extractKeyPattern(key)).Observe(duration)

	status := "success"
	hit := false
	if err != nil {
		status = "error"
		if err != redis.Nil {
			span.SetStatus(codes.Error, err.Error())
			span.RecordError(err)
		} else {
			// Cache miss is not an error
			status = "miss"
		}
	} else {
		hit = true
	}

	// Record cache hit/miss
	span.SetAttributes(
		attribute.Bool("cache.hit", hit),
		attribute.String("cache.status", status),
	)

	metrics.Get().RedisOperationsTotal.WithLabelValues("get", status).Inc()

	return result, err
}

// Set stores a value in Redis
func (rc *RedisClient) Set(ctx context.Context, key string, value interface{}) error {
	// Start distributed tracing span
	_, span := otel.Tracer("redis").Start(ctx, "redis.set")
	defer span.End()

	// Add span attributes
	span.SetAttributes(
		attribute.String("cache.key", maskSensitiveKey(key)),
		attribute.String("cache.operation", "set"),
	)

	start := time.Now()
	err := rc.client.Set(ctx, key, value, 0).Err()

	// METRICS-2: Record Redis operation timing
	duration := time.Since(start).Seconds()
	metrics.Get().RedisOperationDuration.WithLabelValues("set", extractKeyPattern(key)).Observe(duration)

	status := "success"
	if err != nil {
		status = "error"
		span.SetStatus(codes.Error, err.Error())
		span.RecordError(err)
	}
	metrics.Get().RedisOperationsTotal.WithLabelValues("set", status).Inc()

	return err
}

// SetEx stores a value in Redis with expiration
func (rc *RedisClient) SetEx(ctx context.Context, key string, value interface{}, ttl time.Duration) error {
	// Start distributed tracing span with enhanced attributes
	_, span := otel.Tracer("redis").Start(ctx, "redis.setex")
	defer span.End()

	// Add span attributes
	span.SetAttributes(
		attribute.String("cache.key", maskSensitiveKey(key)),
		attribute.String("cache.operation", "setex"),
		attribute.Int64("cache.ttl_seconds", int64(ttl.Seconds())),
	)

	start := time.Now()
	err := rc.client.Set(ctx, key, value, ttl).Err()

	// METRICS-2: Record Redis operation timing
	duration := time.Since(start).Seconds()
	metrics.Get().RedisOperationDuration.WithLabelValues("setex", extractKeyPattern(key)).Observe(duration)

	// Record span duration
	span.SetAttributes(attribute.Float64("cache.duration_seconds", duration))

	status := "success"
	if err != nil {
		status = "error"
		span.SetStatus(codes.Error, err.Error())
		span.RecordError(err)
	} else {
		span.SetStatus(codes.Ok, "")
	}
	metrics.Get().RedisOperationsTotal.WithLabelValues("setex", status).Inc()

	return err
}

// Del deletes one or more keys from Redis
func (rc *RedisClient) Del(ctx context.Context, keys ...string) error {
	start := time.Now()
	err := rc.client.Del(ctx, keys...).Err()

	// METRICS-2: Record Redis operation timing
	duration := time.Since(start).Seconds()
	keyPattern := "del"
	if len(keys) > 0 {
		keyPattern = extractKeyPattern(keys[0])
	}
	metrics.Get().RedisOperationDuration.WithLabelValues("del", keyPattern).Observe(duration)

	status := "success"
	if err != nil {
		status = "error"
	}
	metrics.Get().RedisOperationsTotal.WithLabelValues("del", status).Inc()

	return err
}

// Exists checks if one or more keys exist in Redis
func (rc *RedisClient) Exists(ctx context.Context, keys ...string) (int64, error) {
	return rc.client.Exists(ctx, keys...).Result()
}

// Incr increments a key value in Redis
func (rc *RedisClient) Incr(ctx context.Context, key string) (int64, error) {
	return rc.client.Incr(ctx, key).Result()
}

// Decr decrements a key value in Redis
func (rc *RedisClient) Decr(ctx context.Context, key string) (int64, error) {
	return rc.client.Decr(ctx, key).Result()
}

// IncrBy increments a key by a value
func (rc *RedisClient) IncrBy(ctx context.Context, key string, increment int64) (int64, error) {
	return rc.client.IncrBy(ctx, key, increment).Result()
}

// GetInt retrieves an integer value from Redis
func (rc *RedisClient) GetInt(ctx context.Context, key string) (int64, error) {
	return rc.client.Get(ctx, key).Int64()
}

// LPush pushes a value to the head of a list
func (rc *RedisClient) LPush(ctx context.Context, key string, values ...interface{}) error {
	return rc.client.LPush(ctx, key, values...).Err()
}

// LRange retrieves a range from a list
func (rc *RedisClient) LRange(ctx context.Context, key string, start, stop int64) ([]string, error) {
	return rc.client.LRange(ctx, key, start, stop).Result()
}

// LLen returns the length of a list
func (rc *RedisClient) LLen(ctx context.Context, key string) (int64, error) {
	return rc.client.LLen(ctx, key).Result()
}

// RPop pops a value from the tail of a list
func (rc *RedisClient) RPop(ctx context.Context, key string) (string, error) {
	return rc.client.RPop(ctx, key).Result()
}

// Expire sets an expiration timeout on a key
func (rc *RedisClient) Expire(ctx context.Context, key string, ttl time.Duration) error {
	return rc.client.Expire(ctx, key, ttl).Err()
}

// TTL returns the time-to-live for a key
func (rc *RedisClient) TTL(ctx context.Context, key string) (time.Duration, error) {
	return rc.client.TTL(ctx, key).Result()
}

// Ping tests the Redis connection
func (rc *RedisClient) Ping(ctx context.Context) error {
	return rc.client.Ping(ctx).Err()
}

// FlushDB clears all keys in the current database (use with caution!)
func (rc *RedisClient) FlushDB(ctx context.Context) error {
	return rc.client.FlushDB(ctx).Err()
}

// Keys returns all keys matching a pattern
func (rc *RedisClient) Keys(ctx context.Context, pattern string) ([]string, error) {
	return rc.client.Keys(ctx, pattern).Result()
}

// HSet sets a field in a hash
func (rc *RedisClient) HSet(ctx context.Context, key string, values ...interface{}) error {
	return rc.client.HSet(ctx, key, values...).Err()
}

// HGet gets a field from a hash
func (rc *RedisClient) HGet(ctx context.Context, key string, field string) (string, error) {
	return rc.client.HGet(ctx, key, field).Result()
}

// HGetAll gets all fields from a hash
func (rc *RedisClient) HGetAll(ctx context.Context, key string) (map[string]string, error) {
	return rc.client.HGetAll(ctx, key).Result()
}

// extractKeyPattern extracts the pattern from a Redis key for metrics labeling
// Removes UUIDs and specific IDs to group similar keys together
// e.g., "user:6215c2dc-306d-437c-a9a0-7659a2809f88" → "user:*"
func extractKeyPattern(key string) string {
	// Simple pattern extraction - group by common key prefixes
	if len(key) == 0 {
		return "other"
	}

	// Common Redis key patterns
	patterns := map[string]string{
		"user:":        "user:*",
		"post:":        "post:*",
		"story:":       "story:*",
		"feed:":        "feed:*",
		"cache:":       "cache:*",
		"session:":     "session:*",
		"notification": "notification:*",
	}

	for prefix, pattern := range patterns {
		if len(key) > len(prefix) && key[:len(prefix)] == prefix {
			return pattern
		}
	}

	return "other"
}

// maskSensitiveKey masks sensitive parts of cache keys for logging
// Returns pattern-based key representation to avoid logging sensitive data
func maskSensitiveKey(key string) string {
	pattern := extractKeyPattern(key)
	if pattern == "other" {
		return key[:minInt(10, len(key))] + "..."
	}
	return pattern
}

// minInt returns the minimum of two integers
func minInt(a, b int) int {
	if a < b {
		return a
	}
	return b
}
