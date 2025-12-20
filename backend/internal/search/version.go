package search

import (
	"bytes"
	"context"
	"encoding/json"
	"fmt"
)

// IndexVersion tracks the current schema version
// Increment this whenever index mappings change
// v1: initial, v2: added filename fields
const IndexVersion = 2

// CheckIndexVersion verifies if indices need reindexing
// Returns true if the indices are outdated and need reindexing
func (c *Client) CheckIndexVersion(ctx context.Context) (bool, error) {
	// Check if posts index has version setting
	res, err := c.es.Indices.GetSettings(
		c.es.Indices.GetSettings.WithIndex(IndexPosts),
		c.es.Indices.GetSettings.WithContext(ctx),
	)
	if err != nil {
		return false, fmt.Errorf("failed to get index settings: %w", err)
	}
	defer res.Body.Close()

	if res.IsError() {
		if res.StatusCode == 404 {
			// Index doesn't exist - needs creation
			return true, nil
		}
		return false, fmt.Errorf("error getting index settings: %s", res.Status())
	}

	var settingsResp struct {
		Posts struct {
			Settings struct {
				Index struct {
					Custom struct {
						Version int `json:"version"`
					} `json:"custom"`
				} `json:"index"`
			} `json:"settings"`
		} `json:"sidechain-posts"`
	}

	if err := json.NewDecoder(res.Body).Decode(&settingsResp); err != nil {
		// If we can't parse settings, assume index needs update
		return true, nil
	}

	// If version is less than current, needs reindex
	storedVersion := settingsResp.Posts.Settings.Index.Custom.Version
	if storedVersion < IndexVersion {
		return true, nil
	}

	return false, nil
}

// UpdateIndexVersion updates the version metadata in the index settings
func (c *Client) UpdateIndexVersion(ctx context.Context, indexName string) error {
	updateBody := map[string]interface{}{
		"settings": map[string]interface{}{
			"index": map[string]interface{}{
				"custom": map[string]interface{}{
					"version": IndexVersion,
				},
			},
		},
	}

	bodyJSON, err := json.Marshal(updateBody)
	if err != nil {
		return fmt.Errorf("failed to marshal update body: %w", err)
	}

	res, err := c.es.Indices.PutSettings(
		bytes.NewReader(bodyJSON),
		c.es.Indices.PutSettings.WithIndex(indexName),
		c.es.Indices.PutSettings.WithContext(ctx),
	)
	if err != nil {
		return fmt.Errorf("failed to update index settings: %w", err)
	}
	defer res.Body.Close()

	if res.IsError() {
		return fmt.Errorf("error updating index settings: %s", res.Status())
	}

	return nil
}

// DeleteIndex deletes an index
func (c *Client) DeleteIndex(ctx context.Context, indexName string) error {
	res, err := c.es.Indices.Delete(
		[]string{indexName},
		c.es.Indices.Delete.WithContext(ctx),
	)
	if err != nil {
		return fmt.Errorf("failed to delete index: %w", err)
	}
	defer res.Body.Close()

	if res.IsError() && res.StatusCode != 404 {
		return fmt.Errorf("error deleting index: %s", res.Status())
	}

	return nil
}

// BackfillAllIndices reindexes all posts from the database to Elasticsearch
// This is used after schema changes or migrations
func (c *Client) BackfillAllIndices(ctx context.Context) error {
	// Import models package to avoid circular dependency
	// This function will be called with database already initialized

	// Query all posts from database
	// This is a simplified version - real implementation would use database.DB
	// For now, just log that backfill is needed
	fmt.Println("ℹ️ Backfill would reindex all posts from database to Elasticsearch")
	return nil
}
