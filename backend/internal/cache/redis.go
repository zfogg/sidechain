package cache

import (
	"context"
	"fmt"
	"time"

	"github.com/redis/go-redis/v9"
	"github.com/zfogg/sidechain/backend/internal/logger"
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
	return rc.client.Get(ctx, key).Result()
}

// Set stores a value in Redis
func (rc *RedisClient) Set(ctx context.Context, key string, value interface{}) error {
	return rc.client.Set(ctx, key, value, 0).Err()
}

// SetEx stores a value in Redis with expiration
func (rc *RedisClient) SetEx(ctx context.Context, key string, value interface{}, ttl time.Duration) error {
	return rc.client.Set(ctx, key, value, ttl).Err()
}

// Del deletes one or more keys from Redis
func (rc *RedisClient) Del(ctx context.Context, keys ...string) error {
	return rc.client.Del(ctx, keys...).Err()
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
