package handlers

import (
	"net/http"

	"github.com/gin-gonic/gin"
	"github.com/zfogg/sidechain/backend/internal/util"
)

// GetNotifications gets the user's notifications with unseen/unread counts
// GET /api/notifications
func (h *Handlers) GetNotifications(c *gin.Context) {
	currentUser, ok := util.GetUserFromContext(c)
	if !ok {
		return
	}

	limit := util.ParseInt(c.DefaultQuery("limit", "20"), 20)
	offset := util.ParseInt(c.DefaultQuery("offset", "0"), 0)

	notifs, err := h.kernel.Stream().GetNotifications(currentUser.StreamUserID, limit, offset)
	if err != nil {
		util.RespondInternalError(c, "failed_to_get_notifications", err.Error())
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
	currentUser, ok := util.GetUserFromContext(c)
	if !ok {
		return
	}

	unseen, unread, err := h.kernel.Stream().GetNotificationCounts(currentUser.StreamUserID)
	if err != nil {
		util.RespondInternalError(c, "failed_to_get_notification_counts", err.Error())
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
	currentUser, ok := util.GetUserFromContext(c)
	if !ok {
		return
	}

	if err := h.kernel.Stream().MarkNotificationsRead(currentUser.StreamUserID); err != nil {
		util.RespondInternalError(c, "failed_to_mark_read", err.Error())
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
	currentUser, ok := util.GetUserFromContext(c)
	if !ok {
		return
	}

	if err := h.kernel.Stream().MarkNotificationsSeen(currentUser.StreamUserID); err != nil {
		util.RespondInternalError(c, "failed_to_mark_seen", err.Error())
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"status":  "success",
		"message": "all_notifications_marked_seen",
	})
}
