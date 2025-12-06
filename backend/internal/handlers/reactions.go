package handlers

import (
	"net/http"
	"time"

	"github.com/gin-gonic/gin"
)

// EmojiReact adds an emoji reaction to a post or message
func (h *Handlers) EmojiReact(c *gin.Context) {
	userID := c.GetString("user_id")

	var req struct {
		ActivityID string `json:"activity_id" binding:"required"`
		Emoji      string `json:"emoji" binding:"required"` // Required emoji like 🎵, ❤️, 🔥, 😍, 🚀, 💯
		Type       string `json:"type,omitempty"`           // Optional: "like", "love", "fire", etc.
	}

	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	// Default reaction type based on emoji or use provided type
	reactionType := req.Type
	if reactionType == "" {
		// Map common emojis to reaction types
		switch req.Emoji {
		case "❤️", "💕", "💖":
			reactionType = "love"
		case "🔥":
			reactionType = "fire"
		case "🎵", "🎶":
			reactionType = "music"
		case "😍":
			reactionType = "wow"
		case "🚀":
			reactionType = "hype"
		case "💯":
			reactionType = "perfect"
		default:
			reactionType = "react"
		}
	}

	// Add the emoji reaction
	if err := h.stream.AddReactionWithEmoji(reactionType, userID, req.ActivityID, req.Emoji); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed to add emoji reaction"})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"status":      "reacted",
		"activity_id": req.ActivityID,
		"user_id":     userID,
		"emoji":       req.Emoji,
		"type":        reactionType,
		"timestamp":   time.Now().UTC(),
	})
}
