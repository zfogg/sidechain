package api

import (
	"fmt"

	"github.com/zfogg/sidechain/cli/pkg/client"
	"github.com/zfogg/sidechain/cli/pkg/logger"
)

// ReactionRequest represents a request to add an emoji reaction
type ReactionRequest struct {
	ActivityID string `json:"activity_id"`
	Emoji      string `json:"emoji"`
	Type       string `json:"type,omitempty"`
}

// ReactionResponse represents the response from adding a reaction
type ReactionResponse struct {
	Status      string `json:"status"`
	ActivityID  string `json:"activity_id"`
	UserID      string `json:"user_id"`
	Emoji       string `json:"emoji"`
	Type        string `json:"type"`
	Timestamp   string `json:"timestamp"`
}

// ReactionData represents a single reaction
type ReactionData struct {
	ID        string      `json:"id"`
	Kind      string      `json:"kind"`
	UserID    string      `json:"user_id"`
	CreatedAt string      `json:"created_at"`
	Data      map[string]interface{} `json:"data,omitempty"`
}

// PostReactionsResponse represents reactions on a post
type PostReactionsResponse struct {
	PostID            string                    `json:"post_id"`
	ActivityID        string                    `json:"activity_id"`
	ReactionCounts    map[string]int            `json:"reaction_counts"`
	LatestReactions   map[string][]ReactionData `json:"latest_reactions"`
}

// AddReaction adds an emoji reaction to a post
func AddReaction(postID, emoji string) (*ReactionResponse, error) {
	logger.Debug("Adding reaction", "post_id", postID, "emoji", emoji)

	req := ReactionRequest{
		ActivityID: postID,
		Emoji:      emoji,
	}

	var resp ReactionResponse
	httpResp, err := client.GetClient().
		R().
		SetBody(req).
		SetResult(&resp).
		Post("/api/v1/social/react")

	if err != nil {
		return nil, fmt.Errorf("failed to add reaction: %w", err)
	}

	if !httpResp.IsSuccess() {
		return nil, fmt.Errorf("failed to add reaction: %s", httpResp.Status())
	}

	return &resp, nil
}

// RemoveReaction removes an emoji reaction from a post
func RemoveReaction(postID, emoji string) error {
	logger.Debug("Removing reaction", "post_id", postID, "emoji", emoji)

	// Try the DELETE endpoint first
	resp, err := client.GetClient().
		R().
		Delete(fmt.Sprintf("/api/v1/social/reactions/%s/%s", postID, emoji))

	if err != nil {
		return fmt.Errorf("failed to remove reaction: %w", err)
	}

	if !resp.IsSuccess() {
		return fmt.Errorf("failed to remove reaction: %s", resp.Status())
	}

	return nil
}

// GetPostReactions retrieves all reactions on a post
func GetPostReactions(postID string) (*PostReactionsResponse, error) {
	logger.Debug("Fetching reactions", "post_id", postID)

	var resp PostReactionsResponse
	httpResp, err := client.GetClient().
		R().
		SetResult(&resp).
		Get(fmt.Sprintf("/api/v1/posts/%s/reactions", postID))

	if err != nil {
		return nil, fmt.Errorf("failed to fetch reactions: %w", err)
	}

	if !httpResp.IsSuccess() {
		return nil, fmt.Errorf("failed to fetch reactions: %s", httpResp.Status())
	}

	return &resp, nil
}
