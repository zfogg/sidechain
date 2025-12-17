package search

import (
	"bytes"
	"context"
	"encoding/json"
	"fmt"
	"os"

	"github.com/elastic/go-elasticsearch/v9"
)

// Index names
const (
	IndexUsers   = "users"
	IndexPosts   = "posts"
	IndexStories = "stories"
)

// Client wraps the Elasticsearch client with Sidechain-specific functionality
type Client struct {
	es *elasticsearch.Client
}

// NewClient creates a new Elasticsearch client
func NewClient() (*Client, error) {
	// Get Elasticsearch URL from environment, default to localhost
	esURL := os.Getenv("ELASTICSEARCH_URL")
	if esURL == "" {
		esURL = "http://localhost:9200"
	}

	cfg := elasticsearch.Config{
		Addresses: []string{esURL},
	}

	es, err := elasticsearch.NewClient(cfg)
	if err != nil {
		return nil, fmt.Errorf("failed to create Elasticsearch client: %w", err)
	}

	client := &Client{es: es}

	// Verify connection
	_, err = es.Info()
	if err != nil {
		return nil, fmt.Errorf("failed to connect to Elasticsearch: %w", err)
	}

	return client, nil
}

// InitializeIndices creates the search indices with proper mappings
func (c *Client) InitializeIndices(ctx context.Context) error {
	// Create users index
	if err := c.createUsersIndex(ctx); err != nil {
		return fmt.Errorf("failed to create users index: %w", err)
	}

	// Create posts index
	if err := c.createPostsIndex(ctx); err != nil {
		return fmt.Errorf("failed to create posts index: %w", err)
	}

	// Create stories index
	if err := c.createStoriesIndex(ctx); err != nil {
		return fmt.Errorf("failed to create stories index: %w", err)
	}

	return nil
}

// createUsersIndex creates the users search index with mapping
func (c *Client) createUsersIndex(ctx context.Context) error {
	mapping := map[string]interface{}{
		"mappings": map[string]interface{}{
			"properties": map[string]interface{}{
				"id": map[string]interface{}{
					"type": "keyword",
				},
				"username": map[string]interface{}{
					"type":     "text",
					"analyzer": "standard",
					"fields": map[string]interface{}{
						"keyword": map[string]interface{}{
							"type": "keyword",
						},
						"suggest": map[string]interface{}{
							"type":     "completion",
							"analyzer": "simple",
						},
					},
				},
				"display_name": map[string]interface{}{
					"type":     "text",
					"analyzer": "standard",
				},
				"bio": map[string]interface{}{
					"type":     "text",
					"analyzer": "standard",
				},
				"genre": map[string]interface{}{
					"type": "keyword",
				},
				"follower_count": map[string]interface{}{
					"type": "integer",
				},
				"created_at": map[string]interface{}{
					"type": "date",
				},
			},
		},
	}

	return c.createIndex(ctx, IndexUsers, mapping)
}

// createPostsIndex creates the posts search index with mapping
func (c *Client) createPostsIndex(ctx context.Context) error {
	mapping := map[string]interface{}{
		"mappings": map[string]interface{}{
			"properties": map[string]interface{}{
				"id": map[string]interface{}{
					"type": "keyword",
				},
				"user_id": map[string]interface{}{
					"type": "keyword",
				},
				"username": map[string]interface{}{
					"type": "keyword",
				},
				"genre": map[string]interface{}{
					"type": "keyword",
				},
				"bpm": map[string]interface{}{
					"type": "integer",
				},
				"key": map[string]interface{}{
					"type": "keyword",
				},
				"daw": map[string]interface{}{
					"type": "keyword",
				},
				"like_count": map[string]interface{}{
					"type": "integer",
				},
				"play_count": map[string]interface{}{
					"type": "integer",
				},
				"comment_count": map[string]interface{}{
					"type": "integer",
				},
				"created_at": map[string]interface{}{
					"type": "date",
				},
			},
		},
	}

	return c.createIndex(ctx, IndexPosts, mapping)
}

// createStoriesIndex creates the stories search index with mapping
func (c *Client) createStoriesIndex(ctx context.Context) error {
	mapping := map[string]interface{}{
		"mappings": map[string]interface{}{
			"properties": map[string]interface{}{
				"id": map[string]interface{}{
					"type": "keyword",
				},
				"user_id": map[string]interface{}{
					"type": "keyword",
				},
				"username": map[string]interface{}{
					"type": "keyword",
				},
				"created_at": map[string]interface{}{
					"type": "date",
				},
				"expires_at": map[string]interface{}{
					"type": "date",
				},
			},
		},
	}

	return c.createIndex(ctx, IndexStories, mapping)
}

// createIndex creates an Elasticsearch index with the given mapping
func (c *Client) createIndex(ctx context.Context, indexName string, mapping map[string]interface{}) error {
	// Check if index exists
	res, err := c.es.Indices.Exists([]string{indexName})
	if err != nil {
		return fmt.Errorf("failed to check if index exists: %w", err)
	}
	res.Body.Close()

	// If index exists (status 200), skip creation
	if res.StatusCode == 200 {
		return nil
	}

	// Create index with mapping
	mappingJSON, err := json.Marshal(mapping)
	if err != nil {
		return fmt.Errorf("failed to marshal mapping: %w", err)
	}

	res, err = c.es.Indices.Create(indexName,
		c.es.Indices.Create.WithBody(bytes.NewReader(mappingJSON)),
		c.es.Indices.Create.WithContext(ctx),
	)
	if err != nil {
		return fmt.Errorf("failed to create index: %w", err)
	}
	defer res.Body.Close()

	if res.IsError() {
		var errResp map[string]interface{}
		if err := json.NewDecoder(res.Body).Decode(&errResp); err != nil {
			return fmt.Errorf("error response [%s]", res.Status())
		}
		return fmt.Errorf("error creating index: [%s] %v", res.Status(), errResp["error"])
	}

	return nil
}

// IndexUser indexes a user document for search
func (c *Client) IndexUser(ctx context.Context, userID string, doc map[string]interface{}) error {
	body, err := json.Marshal(doc)
	if err != nil {
		return fmt.Errorf("failed to marshal user document: %w", err)
	}

	res, err := c.es.Index(IndexUsers, bytes.NewReader(body),
		c.es.Index.WithDocumentID(userID),
		c.es.Index.WithContext(ctx),
	)
	if err != nil {
		return fmt.Errorf("failed to index user: %w", err)
	}
	defer res.Body.Close()

	if res.IsError() {
		var errResp map[string]interface{}
		if err := json.NewDecoder(res.Body).Decode(&errResp); err != nil {
			return fmt.Errorf("error response [%s]", res.Status())
		}
		return fmt.Errorf("error indexing user: [%s] %v", res.Status(), errResp["error"])
	}

	return nil
}

// IndexPost indexes a post document for search
func (c *Client) IndexPost(ctx context.Context, postID string, doc map[string]interface{}) error {
	body, err := json.Marshal(doc)
	if err != nil {
		return fmt.Errorf("failed to marshal post document: %w", err)
	}

	res, err := c.es.Index(IndexPosts, bytes.NewReader(body),
		c.es.Index.WithDocumentID(postID),
		c.es.Index.WithContext(ctx),
	)
	if err != nil {
		return fmt.Errorf("failed to index post: %w", err)
	}
	defer res.Body.Close()

	if res.IsError() {
		var errResp map[string]interface{}
		if err := json.NewDecoder(res.Body).Decode(&errResp); err != nil {
			return fmt.Errorf("error response [%s]", res.Status())
		}
		return fmt.Errorf("error indexing post: [%s] %v", res.Status(), errResp["error"])
	}

	return nil
}

// IndexStory indexes a story document for search
func (c *Client) IndexStory(ctx context.Context, storyID string, doc map[string]interface{}) error {
	body, err := json.Marshal(doc)
	if err != nil {
		return fmt.Errorf("failed to marshal story document: %w", err)
	}

	res, err := c.es.Index(IndexStories, bytes.NewReader(body),
		c.es.Index.WithDocumentID(storyID),
		c.es.Index.WithContext(ctx),
	)
	if err != nil {
		return fmt.Errorf("failed to index story: %w", err)
	}
	defer res.Body.Close()

	if res.IsError() {
		var errResp map[string]interface{}
		if err := json.NewDecoder(res.Body).Decode(&errResp); err != nil {
			return fmt.Errorf("error response [%s]", res.Status())
		}
		return fmt.Errorf("error indexing story: [%s] %v", res.Status(), errResp["error"])
	}

	return nil
}

// DeleteUser deletes a user document from the search index
func (c *Client) DeleteUser(ctx context.Context, userID string) error {
	res, err := c.es.Delete(IndexUsers, userID,
		c.es.Delete.WithContext(ctx),
	)
	if err != nil {
		return fmt.Errorf("failed to delete user: %w", err)
	}
	defer res.Body.Close()

	// 404 is OK - document doesn't exist
	if res.IsError() && res.StatusCode != 404 {
		var errResp map[string]interface{}
		if err := json.NewDecoder(res.Body).Decode(&errResp); err != nil {
			return fmt.Errorf("error response [%s]", res.Status())
		}
		return fmt.Errorf("error deleting user: [%s] %v", res.Status(), errResp["error"])
	}

	return nil
}

// DeletePost deletes a post document from the search index
func (c *Client) DeletePost(ctx context.Context, postID string) error {
	res, err := c.es.Delete(IndexPosts, postID,
		c.es.Delete.WithContext(ctx),
	)
	if err != nil {
		return fmt.Errorf("failed to delete post: %w", err)
	}
	defer res.Body.Close()

	// 404 is OK - document doesn't exist
	if res.IsError() && res.StatusCode != 404 {
		var errResp map[string]interface{}
		if err := json.NewDecoder(res.Body).Decode(&errResp); err != nil {
			return fmt.Errorf("error response [%s]", res.Status())
		}
		return fmt.Errorf("error deleting post: [%s] %v", res.Status(), errResp["error"])
	}

	return nil
}

// DeleteStory deletes a story document from the search index
func (c *Client) DeleteStory(ctx context.Context, storyID string) error {
	res, err := c.es.Delete(IndexStories, storyID,
		c.es.Delete.WithContext(ctx),
	)
	if err != nil {
		return fmt.Errorf("failed to delete story: %w", err)
	}
	defer res.Body.Close()

	// 404 is OK - document doesn't exist
	if res.IsError() && res.StatusCode != 404 {
		var errResp map[string]interface{}
		if err := json.NewDecoder(res.Body).Decode(&errResp); err != nil {
			return fmt.Errorf("error response [%s]", res.Status())
		}
		return fmt.Errorf("error deleting story: [%s] %v", res.Status(), errResp["error"])
	}

	return nil
}

// SearchUsersResult represents a user search result
type SearchUsersResult struct {
	Users []UserSearchHit `json:"users"`
	Total int             `json:"total"`
}

// UserSearchHit represents a single user search hit
type UserSearchHit struct {
	ID            string   `json:"id"`
	Username      string   `json:"username"`
	DisplayName   string   `json:"display_name"`
	Bio           string   `json:"bio"`
	Genre         []string `json:"genre"`
	FollowerCount int      `json:"follower_count"`
	Score         float64  `json:"score"`
}

// SearchUsers searches for users by query
func (c *Client) SearchUsers(ctx context.Context, query string, limit, offset int) (*SearchUsersResult, error) {
	// Build search query
	searchQuery := map[string]interface{}{
		"query": map[string]interface{}{
			"bool": map[string]interface{}{
				"should": []map[string]interface{}{
					{
						"match": map[string]interface{}{
							"username": map[string]interface{}{
								"query":         query,
								"boost":         2.0,
								"fuzziness":     "AUTO",
								"prefix_length": 1,
							},
						},
					},
					{
						"match": map[string]interface{}{
							"display_name": map[string]interface{}{
								"query":     query,
								"boost":     1.5,
								"fuzziness": "AUTO",
							},
						},
					},
					{
						"match": map[string]interface{}{
							"bio": map[string]interface{}{
								"query":     query,
								"boost":     0.5,
								"fuzziness": "AUTO",
							},
						},
					},
				},
				"minimum_should_match": 1,
			},
		},
		"sort": []map[string]interface{}{
			{"_score": map[string]interface{}{"order": "desc"}},
			{"follower_count": map[string]interface{}{"order": "desc"}},
		},
		"from": offset,
		"size": limit,
	}

	return c.executeUserSearch(ctx, searchQuery)
}

// executeUserSearch executes a user search query
func (c *Client) executeUserSearch(ctx context.Context, query map[string]interface{}) (*SearchUsersResult, error) {
	queryJSON, err := json.Marshal(query)
	if err != nil {
		return nil, fmt.Errorf("failed to marshal search query: %w", err)
	}

	res, err := c.es.Search(
		c.es.Search.WithContext(ctx),
		c.es.Search.WithIndex(IndexUsers),
		c.es.Search.WithBody(bytes.NewReader(queryJSON)),
	)
	if err != nil {
		return nil, fmt.Errorf("failed to execute search: %w", err)
	}
	defer res.Body.Close()

	if res.IsError() {
		var errResp map[string]interface{}
		if err := json.NewDecoder(res.Body).Decode(&errResp); err != nil {
			return nil, fmt.Errorf("error response [%s]", res.Status())
		}
		return nil, fmt.Errorf("error searching users: [%s] %v", res.Status(), errResp["error"])
	}

	var searchResp struct {
		Hits struct {
			Total struct {
				Value int `json:"value"`
			} `json:"total"`
			Hits []struct {
				ID     string                 `json:"_id"`
				Score  float64                `json:"_score"`
				Source map[string]interface{} `json:"_source"`
			} `json:"hits"`
		} `json:"hits"`
	}

	if err := json.NewDecoder(res.Body).Decode(&searchResp); err != nil {
		return nil, fmt.Errorf("failed to decode search response: %w", err)
	}

	users := make([]UserSearchHit, 0, len(searchResp.Hits.Hits))
	for _, hit := range searchResp.Hits.Hits {
		user := UserSearchHit{
			ID:    hit.ID,
			Score: hit.Score,
		}

		if username, ok := hit.Source["username"].(string); ok {
			user.Username = username
		}
		if displayName, ok := hit.Source["display_name"].(string); ok {
			user.DisplayName = displayName
		}
		if bio, ok := hit.Source["bio"].(string); ok {
			user.Bio = bio
		}
		if genre, ok := hit.Source["genre"].([]interface{}); ok {
			user.Genre = make([]string, 0, len(genre))
			for _, g := range genre {
				if gs, ok := g.(string); ok {
					user.Genre = append(user.Genre, gs)
				}
			}
		}
		if followerCount, ok := hit.Source["follower_count"].(float64); ok {
			user.FollowerCount = int(followerCount)
		}

		users = append(users, user)
	}

	return &SearchUsersResult{
		Users: users,
		Total: searchResp.Hits.Total.Value,
	}, nil
}

// SearchPostsResult represents a post search result
type SearchPostsResult struct {
	Posts []PostSearchHit `json:"posts"`
	Total int             `json:"total"`
}

// PostSearchHit represents a single post search hit
type PostSearchHit struct {
	ID           string   `json:"id"`
	UserID       string   `json:"user_id"`
	Username     string   `json:"username"`
	Genre        []string `json:"genre"`
	BPM          int      `json:"bpm"`
	Key          string   `json:"key"`
	DAW          string   `json:"daw"`
	LikeCount    int      `json:"like_count"`
	PlayCount    int      `json:"play_count"`
	CommentCount int      `json:"comment_count"`
	CreatedAt    string   `json:"created_at"`
	Score        float64  `json:"score"`
}

// SearchPostsParams contains parameters for post search
type SearchPostsParams struct {
	Query  string
	Genre  []string
	BPMMin *int
	BPMMax *int
	Key    string
	Limit  int
	Offset int
}

// SearchPosts searches for posts with filters
func (c *Client) SearchPosts(ctx context.Context, params SearchPostsParams) (*SearchPostsResult, error) {
	mustClauses := []map[string]interface{}{}
	shouldClauses := []map[string]interface{}{}

	// Text search on filename, username, genre, DAW (7.1.4)
	if params.Query != "" {
		shouldClauses = append(shouldClauses,
			map[string]interface{}{
				"match": map[string]interface{}{
					"filename": map[string]interface{}{
						"query":     params.Query,
						"boost":     3.5,
						"fuzziness": "AUTO",
					},
				},
			},
			map[string]interface{}{
				"match": map[string]interface{}{
					"original_filename": map[string]interface{}{
						"query":     params.Query,
						"boost":     3.0,
						"fuzziness": "AUTO",
					},
				},
			},
			map[string]interface{}{
				"match": map[string]interface{}{
					"username": map[string]interface{}{
						"query":     params.Query,
						"boost":     2.0,
						"fuzziness": "AUTO",
					},
				},
			},
			map[string]interface{}{
				"match": map[string]interface{}{
					"genre": map[string]interface{}{
						"query":     params.Query,
						"boost":     1.5,
						"fuzziness": "AUTO",
					},
				},
			},
			map[string]interface{}{
				"match": map[string]interface{}{
					"daw": map[string]interface{}{
						"query":     params.Query,
						"boost":     1.0,
						"fuzziness": "AUTO",
					},
				},
			},
		)
	}

	// Genre filter
	if len(params.Genre) > 0 {
		mustClauses = append(mustClauses, map[string]interface{}{
			"terms": map[string]interface{}{
				"genre": params.Genre,
			},
		})
	}

	// BPM range filter
	if params.BPMMin != nil || params.BPMMax != nil {
		bpmRange := map[string]interface{}{}
		if params.BPMMin != nil {
			bpmRange["gte"] = *params.BPMMin
		}
		if params.BPMMax != nil {
			bpmRange["lte"] = *params.BPMMax
		}
		mustClauses = append(mustClauses, map[string]interface{}{
			"range": map[string]interface{}{
				"bpm": bpmRange,
			},
		})
	}

	// Key filter
	if params.Key != "" {
		mustClauses = append(mustClauses, map[string]interface{}{
			"term": map[string]interface{}{
				"key": params.Key,
			},
		})
	}

	// Build base query
	var baseQuery map[string]interface{}

	if len(shouldClauses) > 0 || len(mustClauses) > 0 {
		// Build bool query
		boolQuery := map[string]interface{}{}
		if len(mustClauses) > 0 {
			boolQuery["must"] = mustClauses
		}
		if len(shouldClauses) > 0 {
			boolQuery["should"] = shouldClauses
			boolQuery["minimum_should_match"] = 1
		}
		baseQuery = map[string]interface{}{
			"bool": boolQuery,
		}
	} else {
		// No query text and no filters - match all
		baseQuery = map[string]interface{}{
			"match_all": map[string]interface{}{},
		}
	}

	// Ranking: Score by relevance + engagement + recency (7.1.8)
	// Use function_score query to combine relevance with engagement and recency
	scoredQuery := map[string]interface{}{
		"function_score": map[string]interface{}{
			"query": baseQuery,
			"functions": []map[string]interface{}{
				{
					"field_value_factor": map[string]interface{}{
						"field":    "like_count",
						"factor":   3.0,
						"modifier": "log1p",
					},
				},
				{
					"field_value_factor": map[string]interface{}{
						"field":    "play_count",
						"factor":   1.0,
						"modifier": "log1p",
					},
				},
				{
					"field_value_factor": map[string]interface{}{
						"field":    "comment_count",
						"factor":   2.0,
						"modifier": "log1p",
					},
				},
				{
					"exp": map[string]interface{}{
						"created_at": map[string]interface{}{
							"origin": "now",
							"scale":  "30d",
							"decay":  0.5,
						},
					},
					"weight": 0.5,
				},
			},
			"score_mode": "sum",
			"boost_mode": "multiply",
		},
	}

	// Build final query with pagination and sorting
	query := map[string]interface{}{
		"query": scoredQuery,
		"from":  params.Offset,
		"size":  params.Limit,
		"sort": []map[string]interface{}{
			{"_score": map[string]interface{}{"order": "desc"}},
			{"created_at": map[string]interface{}{"order": "desc"}},
		},
	}

	return c.executePostSearch(ctx, query)
}

// executePostSearch executes a post search query
func (c *Client) executePostSearch(ctx context.Context, query map[string]interface{}) (*SearchPostsResult, error) {
	queryJSON, err := json.Marshal(query)
	if err != nil {
		return nil, fmt.Errorf("failed to marshal search query: %w", err)
	}

	res, err := c.es.Search(
		c.es.Search.WithContext(ctx),
		c.es.Search.WithIndex(IndexPosts),
		c.es.Search.WithBody(bytes.NewReader(queryJSON)),
	)
	if err != nil {
		return nil, fmt.Errorf("failed to execute search: %w", err)
	}
	defer res.Body.Close()

	if res.IsError() {
		var errResp map[string]interface{}
		if err := json.NewDecoder(res.Body).Decode(&errResp); err != nil {
			return nil, fmt.Errorf("error response [%s]", res.Status())
		}
		return nil, fmt.Errorf("error searching posts: [%s] %v", res.Status(), errResp["error"])
	}

	var searchResp struct {
		Hits struct {
			Total struct {
				Value int `json:"value"`
			} `json:"total"`
			Hits []struct {
				ID     string                 `json:"_id"`
				Score  float64                `json:"_score"`
				Source map[string]interface{} `json:"_source"`
			} `json:"hits"`
		} `json:"hits"`
	}

	if err := json.NewDecoder(res.Body).Decode(&searchResp); err != nil {
		return nil, fmt.Errorf("failed to decode search response: %w", err)
	}

	posts := make([]PostSearchHit, 0, len(searchResp.Hits.Hits))
	for _, hit := range searchResp.Hits.Hits {
		post := PostSearchHit{
			ID:    hit.ID,
			Score: hit.Score,
		}

		if userID, ok := hit.Source["user_id"].(string); ok {
			post.UserID = userID
		}
		if username, ok := hit.Source["username"].(string); ok {
			post.Username = username
		}
		if genre, ok := hit.Source["genre"].([]interface{}); ok {
			post.Genre = make([]string, 0, len(genre))
			for _, g := range genre {
				if gs, ok := g.(string); ok {
					post.Genre = append(post.Genre, gs)
				}
			}
		}
		if bpm, ok := hit.Source["bpm"].(float64); ok {
			post.BPM = int(bpm)
		}
		if key, ok := hit.Source["key"].(string); ok {
			post.Key = key
		}
		if daw, ok := hit.Source["daw"].(string); ok {
			post.DAW = daw
		}
		if likeCount, ok := hit.Source["like_count"].(float64); ok {
			post.LikeCount = int(likeCount)
		}
		if playCount, ok := hit.Source["play_count"].(float64); ok {
			post.PlayCount = int(playCount)
		}
		if commentCount, ok := hit.Source["comment_count"].(float64); ok {
			post.CommentCount = int(commentCount)
		}
		if createdAt, ok := hit.Source["created_at"].(string); ok {
			post.CreatedAt = createdAt
		}

		posts = append(posts, post)
	}

	return &SearchPostsResult{
		Posts: posts,
		Total: searchResp.Hits.Total.Value,
	}, nil
}

// SuggestUsers returns autocomplete suggestions for usernames
func (c *Client) SuggestUsers(ctx context.Context, query string, limit int) ([]string, error) {
	suggestQuery := map[string]interface{}{
		"suggest": map[string]interface{}{
			"username_suggest": map[string]interface{}{
				"prefix": query,
				"completion": map[string]interface{}{
					"field": "username.suggest",
					"size":  limit,
				},
			},
		},
	}

	queryJSON, err := json.Marshal(suggestQuery)
	if err != nil {
		return nil, fmt.Errorf("failed to marshal suggest query: %w", err)
	}

	res, err := c.es.Search(
		c.es.Search.WithContext(ctx),
		c.es.Search.WithIndex(IndexUsers),
		c.es.Search.WithBody(bytes.NewReader(queryJSON)),
	)
	if err != nil {
		return nil, fmt.Errorf("failed to execute suggest: %w", err)
	}
	defer res.Body.Close()

	if res.IsError() {
		var errResp map[string]interface{}
		if err := json.NewDecoder(res.Body).Decode(&errResp); err != nil {
			return nil, fmt.Errorf("error response [%s]", res.Status())
		}
		return nil, fmt.Errorf("error suggesting users: [%s] %v", res.Status(), errResp["error"])
	}

	var suggestResp struct {
		Suggest struct {
			UsernameSuggest []struct {
				Options []struct {
					Text string `json:"text"`
				} `json:"options"`
			} `json:"username_suggest"`
		} `json:"suggest"`
	}

	if err := json.NewDecoder(res.Body).Decode(&suggestResp); err != nil {
		return nil, fmt.Errorf("failed to decode suggest response: %w", err)
	}

	suggestions := make([]string, 0)
	if len(suggestResp.Suggest.UsernameSuggest) > 0 {
		for _, option := range suggestResp.Suggest.UsernameSuggest[0].Options {
			suggestions = append(suggestions, option.Text)
		}
	}

	return suggestions, nil
}

// TrackSearchQuery tracks a search query for analytics (7.1.9)
func (c *Client) TrackSearchQuery(ctx context.Context, query string, resultCount int, filters map[string]interface{}) error {
	// In a production system, you'd index this to a separate "search_analytics" index
	// For now, we'll just log it - can be expanded later
	fmt.Printf("🔍 Search query tracked: query='%s', results=%d, filters=%v\n", query, resultCount, filters)
	return nil
}
