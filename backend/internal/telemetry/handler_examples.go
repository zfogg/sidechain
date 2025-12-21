package telemetry

import (
	"context"
	"fmt"

	"go.opentelemetry.io/otel"
	"go.opentelemetry.io/otel/attribute"
)

// This file contains example patterns for using tracing in HTTP handlers
// These examples show best practices for tracing different types of operations

// ============================================================================
// EXAMPLE 1: Simple database query with tracing
// ============================================================================

// ExampleGetUserWithTracing demonstrates tracing a simple database query
//
// Usage in handler:
//	func (h *Handlers) GetUser(c *gin.Context) {
//		ctx, span := TraceGetUser(c.Request.Context(), userID)
//		defer span.End()
//
//		user, err := h.DB.WithContext(ctx).First(&models.User{}, userID).Error
//		if err != nil {
//			span.RecordError(err)
//			return
//		}
//		span.SetAttributes(attribute.String("user.username", user.Username))
//	}
func TraceGetUser(ctx context.Context, userID string) (context.Context, interface{}) {
	tracer := otel.Tracer("handlers")
	ctx, span := tracer.Start(ctx, "get_user",
		// Pass context to DB query to inherit tracing
		// IMPORTANT: Always pass ctx to DB.WithContext(ctx)
	)
	SetUserContext(span, userID, "")
	return ctx, span
}

// ============================================================================
// EXAMPLE 2: External API call with error handling and retry logic
// ============================================================================

// ExampleExternalAPICallWithRetry demonstrates tracing an external API call with retries
//
// Usage pattern:
//	func (h *Handlers) FollowUser(c *gin.Context) {
//		ctx, span := TraceStreamIOCall(c.Request.Context(), "follow", map[string]interface{}{
//			"user_id": currentUserID,
//			"target_id": targetUserID,
//		})
//		defer span.End()
//
//		// Retry logic
//		var result *stream.Activity
//		var lastErr error
//		for attempt := 0; attempt < 3; attempt++ {
//			result, err := h.StreamClient.FollowUser(ctx, currentUserID, targetUserID)
//			if err == nil {
//				RecordServiceSuccess(span, map[string]interface{}{
//					"duration_ms": durationMs,
//				})
//				return
//			}
//			lastErr = err
//			time.Sleep(time.Duration(math.Pow(2, float64(attempt))) * time.Second)
//		}
//
//		RecordServiceError(span, "stream.io", lastErr)
//		span.SetAttributes(attribute.Int("external.retry_count", 3))
//	}
func ExampleExternalAPICallWithRetry() {
	fmt.Println(`
Example: External API Call with Retry

func (h *Handlers) FollowUser(c *gin.Context) {
	ctx, span := TraceStreamIOCall(c.Request.Context(), "follow", map[string]interface{}{
		"user_id": currentUserID,
		"target_id": targetUserID,
	})
	defer span.End()

	var lastErr error
	for attempt := 0; attempt < 3; attempt++ {
		result, err := h.StreamClient.FollowUser(ctx, currentUserID, targetUserID)
		if err == nil {
			RecordServiceSuccess(span, map[string]interface{}{
				"duration_ms": elapsed.Milliseconds(),
			})
			c.JSON(200, result)
			return
		}
		lastErr = err
		time.Sleep(exponentialBackoff(attempt))
	}

	RecordServiceError(span, "stream.io", lastErr)
	span.SetAttributes(attribute.Int("external.retry_count", 3))
	c.JSON(500, gin.H{"error": "Failed to follow user after retries"})
}
	`)
}

// ============================================================================
// EXAMPLE 3: Complex operation with multiple spans
// ============================================================================

// ExampleComplexOperationWithSpans demonstrates tracing a complex operation
// that involves multiple service calls
//
// Usage pattern:
//	func (h *Handlers) CreatePost(c *gin.Context) {
//		ctx, createSpan := GetBusinessEvents().TraceCreatePost(c.Request.Context(), postID, "mp3")
//		defer createSpan.End()
//
//		// Step 1: Save to database
//		{
//			ctx, dbSpan := otel.Tracer("handlers").Start(ctx, "save_to_db")
//			err := h.DB.WithContext(ctx).Create(&post).Error
//			if err != nil {
//				dbSpan.RecordError(err)
//				dbSpan.End()
//				return
//			}
//			dbSpan.End()
//		}
//
//		// Step 2: Upload to S3
//		{
//			ctx, s3Span := TraceS3Call(ctx, "put_object", map[string]interface{}{
//				"bucket": h.S3Bucket,
//				"key": postID,
//				"size_bytes": len(fileData),
//			})
//			err := h.Storage.Upload(ctx, fileData)
//			if err != nil {
//				RecordServiceError(s3Span, "s3", err)
//				s3Span.End()
//				return
//			}
//			s3Span.End()
//		}
//
//		// Step 3: Index in Elasticsearch
//		{
//			ctx, esSpan := TraceElasticsearchCall(ctx, "index", map[string]interface{}{
//				"index": "posts",
//				"doc_id": postID,
//			})
//			err := h.Search.Index(ctx, postID, post)
//			if err != nil {
//				RecordServiceError(esSpan, "elasticsearch", err)
//				esSpan.End()
//				return
//			}
//			esSpan.End()
//		}
//
//		// Step 4: Update recommendations
//		{
//			ctx, gorseSpan := TraceGorseCall(ctx, "insert_user_item", map[string]interface{}{
//				"user_id": userID,
//				"item_id": postID,
//			})
//			err := h.Recommendations.IndexPost(ctx, userID, postID)
//			if err != nil {
//				RecordServiceError(gorseSpan, "gorse", err)
//				gorseSpan.End()
//				return
//			}
//			gorseSpan.End()
//		}
//
//		RecordServiceSuccess(createSpan, map[string]interface{}{
//			"duration_ms": elapsed.Milliseconds(),
//		})
//	}
func ExampleComplexOperationWithSpans() {
	fmt.Println(`
Example: Complex Operation with Multiple Spans

func (h *Handlers) CreatePost(c *gin.Context) {
	ctx, createSpan := GetBusinessEvents().TraceCreatePost(c.Request.Context(), postID, "mp3")
	defer createSpan.End()

	// Each step creates a child span automatically through context inheritance

	// Step 1: Save to DB (creates "db.insert" span automatically via GORM plugin)
	if err := h.DB.WithContext(ctx).Create(&post).Error; err != nil {
		createSpan.RecordError(err)
		return
	}

	// Step 2: Upload to S3
	ctx, s3Span := TraceS3Call(ctx, "put_object", map[string]interface{}{
		"bucket": h.S3Bucket,
		"key": postID,
		"size_bytes": fileSize,
	})
	if err := h.Storage.Upload(ctx, fileData); err != nil {
		RecordServiceError(s3Span, "s3", err)
		s3Span.End()
		return
	}
	s3Span.End()

	// Step 3: Index in Elasticsearch
	ctx, esSpan := TraceElasticsearchCall(ctx, "index", map[string]interface{}{
		"index": "posts",
		"doc_id": postID,
	})
	if err := h.Search.Index(ctx, postID, post); err != nil {
		RecordServiceError(esSpan, "elasticsearch", err)
		esSpan.End()
		return
	}
	esSpan.End()

	// Step 4: Update recommendations
	ctx, gorseSpan := TraceGorseCall(ctx, "insert_user_item", map[string]interface{}{
		"user_id": userID,
		"item_id": postID,
	})
	if err := h.Recommendations.IndexPost(ctx, userID, postID); err != nil {
		RecordServiceError(gorseSpan, "gorse", err)
		gorseSpan.End()
		return
	}
	gorseSpan.End()
}
	`)
}

// ============================================================================
// EXAMPLE 4: Cache operations with fallback
// ============================================================================

// ExampleCacheOperationWithFallback demonstrates tracing cache operations
// with fallback to database
//
// Usage pattern:
//	func (h *Handlers) GetFeedWithCache(c *gin.Context) {
//		ctx, span := GetBusinessEvents().TraceGetFeed(c.Request.Context(), "timeline", FeedEventAttrs{
//			Limit: 20,
//			Offset: 0,
//		})
//		defer span.End()
//
//		cacheKey := "feed:" + userID
//		ctx, cacheSpan := TraceCacheCall(ctx, "get", map[string]interface{}{
//			"key": cacheKey,
//		})
//
//		cachedData, err := h.Cache.Get(ctx, cacheKey)
//		hit := err == nil && cachedData != nil
//		cacheSpan.SetAttributes(attribute.Bool("cache.hit", hit))
//		cacheSpan.End()
//
//		if hit {
//			// Parse and return cached data
//			span.SetAttributes(attribute.Bool("feed.fallback_used", false))
//			return
//		}
//
//		// Cache miss - fetch from database
//		ctx, dbSpan := otel.Tracer("handlers").Start(ctx, "fetch_from_db")
//		feed, err := h.getFeedFromDB(ctx, userID)
//		dbSpan.End()
//
//		if err != nil {
//			span.RecordError(err)
//			return
//		}
//
//		// Cache the result for next time
//		ctx, setCacheSpan := TraceCacheCall(ctx, "set", map[string]interface{}{
//			"key": cacheKey,
//			"ttl_seconds": 300,
//		})
//		_ = h.Cache.Set(ctx, cacheKey, feed, 300*time.Second)
//		setCacheSpan.End()
//	}
func ExampleCacheOperationWithFallback() {
	fmt.Println(`
Example: Cache Operation with DB Fallback

func (h *Handlers) GetFeedWithCache(c *gin.Context) {
	ctx, span := GetBusinessEvents().TraceGetFeed(c.Request.Context(), "timeline", FeedEventAttrs{
		Limit: 20,
		Offset: 0,
	})
	defer span.End()

	cacheKey := "feed:" + userID
	ctx, cacheSpan := TraceCacheCall(ctx, "get", map[string]interface{}{
		"key": cacheKey,
	})

	cachedData, err := h.Cache.Get(ctx, cacheKey)
	hit := err == nil && cachedData != nil
	cacheSpan.SetAttributes(attribute.Bool("cache.hit", hit))
	cacheSpan.End()

	var feed []*models.Post
	if hit {
		// Use cached data
		json.Unmarshal(cachedData, &feed)
	} else {
		// Fetch from database (GORM tracing will create db.select span automatically)
		h.DB.WithContext(ctx).Where("user_id = ?", userID).Find(&feed)

		// Cache for future requests
		if data, _ := json.Marshal(feed); data != nil {
			ctx, setCacheSpan := TraceCacheCall(ctx, "set", map[string]interface{}{
				"key": cacheKey,
				"ttl_seconds": 300,
			})
			h.Cache.Set(ctx, cacheKey, data, 300*time.Second)
			setCacheSpan.End()
		}
	}

	c.JSON(200, feed)
}
	`)
}

// ============================================================================
// EXAMPLE 5: Search operations with fallback
// ============================================================================

// ExampleSearchWithFallback demonstrates tracing search with fallback to database
//
// Usage pattern:
//	func (h *Handlers) SearchPosts(c *gin.Context) {
//		query := c.Query("q")
//		ctx, span := GetBusinessEvents().TraceSearch(c.Request.Context(), SearchEventAttrs{
//			Query: query,
//			Index: "posts",
//		})
//		defer span.End()
//
//		ctx, esSpan := TraceElasticsearchCall(ctx, "search", map[string]interface{}{
//			"query": query,
//			"index": "posts",
//		})
//
//		posts, err := h.Search.Search(ctx, query)
//		fallbackUsed := err != nil
//
//		if fallbackUsed {
//			esSpan.RecordError(err)
//			esSpan.End()
//
//			// Fallback to PostgreSQL full-text search
//			ctx, dbSpan := otel.Tracer("handlers").Start(ctx, "search_fallback")
//			posts, err = h.DB.WithContext(ctx).Where("title ILIKE ? OR description ILIKE ?",
//				"%"+query+"%", "%"+query+"%").Find(&posts).Error
//			dbSpan.End()
//
//			span.SetAttributes(attribute.Bool("search.fallback_used", true))
//		} else {
//			esSpan.SetAttributes(attribute.Int("es.hit_count", len(posts)))
//			esSpan.End()
//		}
//
//		c.JSON(200, posts)
//	}
func ExampleSearchWithFallback() {
	fmt.Println(`
Example: Search with Elasticsearch + Database Fallback

func (h *Handlers) SearchPosts(c *gin.Context) {
	query := c.Query("q")
	ctx, span := GetBusinessEvents().TraceSearch(c.Request.Context(), SearchEventAttrs{
		Query: query,
		Index: "posts",
	})
	defer span.End()

	// Try Elasticsearch first
	ctx, esSpan := TraceElasticsearchCall(ctx, "search", map[string]interface{}{
		"query": query,
		"index": "posts",
	})

	posts, err := h.Search.Search(ctx, query)

	if err != nil {
		// Elasticsearch failed, use fallback
		esSpan.RecordError(err)
		esSpan.End()

		ctx, dbSpan := otel.Tracer("handlers").Start(ctx, "search_fallback_database")
		h.DB.WithContext(ctx).Where("title ILIKE ?", "%"+query+"%").Find(&posts)
		dbSpan.End()

		span.SetAttributes(attribute.Bool("search.fallback_used", true))
	} else {
		esSpan.SetAttributes(attribute.Int("es.hit_count", len(posts)))
		esSpan.End()
	}

	c.JSON(200, posts)
}
	`)
}

// ============================================================================
// KEY BEST PRACTICES
// ============================================================================

// Best Practices for Distributed Tracing in Handlers:
//
// 1. ALWAYS pass context to database queries:
//    ✓ db.WithContext(ctx).Find(&user)
//    ✗ db.Find(&user)  // Context lost, trace chain broken
//
// 2. ALWAYS defer span.End():
//    ✓ defer span.End()
//    ✗ // Not calling End() - span leaked
//
// 3. Use context propagation for external calls:
//    ✓ client.MakeRequest(ctx, req)
//    ✗ client.MakeRequest(context.Background(), req)
//
// 4. Combine related operations under parent spans:
//    ✓ Create parent span, then child spans for each step
//    ✗ Create unrelated top-level spans
//
// 5. Record meaningful attributes:
//    ✓ span.SetAttributes(attribute.String("user.id", userID))
//    ✗ span.SetAttributes(attribute.String("data", largeJSONString))
//
// 6. Set span status appropriately:
//    ✓ RecordServiceSuccess() or RecordServiceError()
//    ✗ Leaving status unset
//
// 7. Use helper functions for consistent attribute names:
//    ✓ TraceStreamIOCall(ctx, "follow", attrs)
//    ✗ span.SetAttributes("steam_io_op", "follow")  // Inconsistent naming
//
// 8. Truncate long values to avoid cardinality explosion:
//    ✓ query := bigQuery[:200] + "..."; span.SetAttributes(query)
//    ✗ span.SetAttributes(attribute.String("query", bigQuery))
//
// 9. Set correlation ID for business transactions:
//    ✓ SetCorrelationID(span, correlationID)
//    ✗ Losing trace of business transaction across requests
//
// 10. Handle errors with stack traces:
//     ✓ span.RecordError(err, trace.WithStackTrace(true))
//     ✗ span.RecordError(err)  // No stack trace
