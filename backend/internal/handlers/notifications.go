package handlers

import (
	"net/http"
	"strconv"

	"github.com/gin-gonic/gin"
	"github.com/zfogg/sidechain/backend/internal/models"
)

// GetNotifications gets the user's notifications with unseen/unread counts
// GET /api/notifications
func (h *Handlers) GetNotifications(c *gin.Context) {
	user, exists := c.Get("user")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "not_authenticated"})
		return
	}
	currentUser := user.(*models.User)

	limit, _ := strconv.Atoi(c.DefaultQuery("limit", "20"))
	offset, _ := strconv.Atoi(c.DefaultQuery("offset", "0"))

	notifs, err := h.stream.GetNotifications(currentUser.StreamUserID, limit, offset)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed_to_get_notifications", "message": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"groups": notifs.Groups,
		"unseen": notifs.Unseen,
		"unread": notifs.Unread,
		"meta": gin.H{
			"limit":  limit,
			"offset": offset,
			"count":  len(notifs.Groups),
		},
	})
}

// GetNotificationCounts gets just the unseen/unread counts for badge display
// GET /api/notifications/counts
func (h *Handlers) GetNotificationCounts(c *gin.Context) {
	user, exists := c.Get("user")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "not_authenticated"})
		return
	}
	currentUser := user.(*models.User)

	unseen, unread, err := h.stream.GetNotificationCounts(currentUser.StreamUserID)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed_to_get_notification_counts", "message": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"unseen": unseen,
		"unread": unread,
	})
}

// MarkNotificationsRead marks all notifications as read
// POST /api/notifications/read
func (h *Handlers) MarkNotificationsRead(c *gin.Context) {
	user, exists := c.Get("user")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "not_authenticated"})
		return
	}
	currentUser := user.(*models.User)

	if err := h.stream.MarkNotificationsRead(currentUser.StreamUserID); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed_to_mark_read", "message": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"status":  "success",
		"message": "all_notifications_marked_read",
	})
}

// MarkNotificationsSeen marks all notifications as seen (clears badge)
// POST /api/notifications/seen
func (h *Handlers) MarkNotificationsSeen(c *gin.Context) {
	user, exists := c.Get("user")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "not_authenticated"})
		return
	}
	currentUser := user.(*models.User)

	if err := h.stream.MarkNotificationsSeen(currentUser.StreamUserID); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed_to_mark_seen", "message": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"status":  "success",
		"message": "all_notifications_marked_seen",
	})
}
