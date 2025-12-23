package handlers

import (
	"net/http"

	"github.com/gin-gonic/gin"
	"github.com/zfogg/sidechain/backend/internal/database"
	"github.com/zfogg/sidechain/backend/internal/logger"
	"github.com/zfogg/sidechain/backend/internal/models"
	"github.com/zfogg/sidechain/backend/internal/util"
	"go.uber.org/zap"
	"gorm.io/gorm"
)

// GetFollowRequests returns pending follow requests for the authenticated user
// GET /api/v1/users/me/follow-requests
func (h *Handlers) GetFollowRequests(c *gin.Context) {
	currentUser, ok := util.GetUserFromContext(c)
	if !ok {
		return
	}

	var requests []models.FollowRequest
	if err := database.DB.Where("target_id = ? AND status = ?", currentUser.ID, models.FollowRequestStatusPending).
		Preload("Requester").
		Order("created_at DESC").
		Find(&requests).Error; err != nil {
		util.RespondInternalError(c, "fetch_failed", "Failed to fetch follow requests")
		return
	}

	// Build response with requester info
	type FollowRequestResponse struct {
		ID          string `json:"id"`
		RequesterID string `json:"requester_id"`
		Username    string `json:"username"`
		DisplayName string `json:"display_name"`
		AvatarURL   string `json:"avatar_url"`
		Bio         string `json:"bio"`
		CreatedAt   string `json:"created_at"`
	}

	response := make([]FollowRequestResponse, len(requests))
	for i, req := range requests {
		response[i] = FollowRequestResponse{
			ID:          req.ID,
			RequesterID: req.RequesterID,
			Username:    req.Requester.Username,
			DisplayName: req.Requester.DisplayName,
			AvatarURL:   req.Requester.GetAvatarURL(),
			Bio:         req.Requester.Bio,
			CreatedAt:   req.CreatedAt.Format("2006-01-02T15:04:05Z"),
		}
	}

	c.JSON(http.StatusOK, gin.H{
		"requests": response,
		"count":    len(response),
	})
}

// GetPendingFollowRequests returns follow requests the user has sent that are pending
// GET /api/v1/users/me/pending-requests
func (h *Handlers) GetPendingFollowRequests(c *gin.Context) {
	currentUser, ok := util.GetUserFromContext(c)
	if !ok {
		return
	}

	var requests []models.FollowRequest
	if err := database.DB.Where("requester_id = ? AND status = ?", currentUser.ID, models.FollowRequestStatusPending).
		Preload("Target").
		Order("created_at DESC").
		Find(&requests).Error; err != nil {
		util.RespondInternalError(c, "fetch_failed", "Failed to fetch pending requests")
		return
	}

	// Build response with target info
	type PendingRequestResponse struct {
		ID          string `json:"id"`
		TargetID    string `json:"target_id"`
		Username    string `json:"username"`
		DisplayName string `json:"display_name"`
		AvatarURL   string `json:"avatar_url"`
		CreatedAt   string `json:"created_at"`
	}

	response := make([]PendingRequestResponse, len(requests))
	for i, req := range requests {
		response[i] = PendingRequestResponse{
			ID:          req.ID,
			TargetID:    req.TargetID,
			Username:    req.Target.Username,
			DisplayName: req.Target.DisplayName,
			AvatarURL:   req.Target.GetAvatarURL(),
			CreatedAt:   req.CreatedAt.Format("2006-01-02T15:04:05Z"),
		}
	}

	c.JSON(http.StatusOK, gin.H{
		"requests": response,
		"count":    len(response),
	})
}

// AcceptFollowRequest accepts a pending follow request
// POST /api/v1/follow-requests/:id/accept
func (h *Handlers) AcceptFollowRequest(c *gin.Context) {
	currentUser, ok := util.GetUserFromContext(c)
	if !ok {
		return
	}

	requestID := c.Param("id")

	// Find the follow request
	var request models.FollowRequest
	if err := database.DB.Where("id = ? AND target_id = ? AND status = ?",
		requestID, currentUser.ID, models.FollowRequestStatusPending).
		Preload("Requester").
		First(&request).Error; err != nil {
		if err == gorm.ErrRecordNotFound {
			util.RespondNotFound(c, "request")
			return
		}
		util.RespondInternalError(c, "fetch_failed", "Failed to fetch follow request")
		return
	}

	// Start a transaction
	tx := database.DB.Begin()

	// Update the request status
	if err := tx.Model(&request).Update("status", models.FollowRequestStatusAccepted).Error; err != nil {
		tx.Rollback()
		util.RespondInternalError(c, "update_failed", "Failed to update request status")
		return
	}

	// Create the follow relationship via Stream.io
	// Use database IDs (not Stream IDs) to match CheckIsFollowing behavior
	if h.stream != nil {
		if err := h.stream.FollowUser(request.RequesterID, currentUser.ID); err != nil {
			tx.Rollback()
			util.RespondInternalError(c, "follow_failed", "Failed to create follow relationship")
			return
		}
	}

	tx.Commit()

	// Sync follow event to Gorse for recommendations (async, don't block response)
	if h.gorse != nil {
		go func() {
			if err := h.gorse.SyncFollowEvent(request.RequesterID, currentUser.ID); err != nil {
				logger.Warn("Failed to sync follow to Gorse",
					zap.String("requester_id", request.RequesterID),
					zap.String("target_id", currentUser.ID),
					zap.Error(err))
			}
		}()
	}

	// Send notification to requester that their follow request was accepted
	if h.stream != nil {
		go func() {
			if err := h.stream.NotifyFollowRequestAccepted(currentUser.ID, request.RequesterID); err != nil {
				logger.Warn("Failed to send follow request accepted notification",
					zap.String("target_id", currentUser.ID),
					zap.String("requester_id", request.RequesterID),
					zap.Error(err))
			}
		}()
	}

	// Send WebSocket notification for real-time update
	if h.wsHandler != nil {
		h.wsHandler.NotifyFollowRequestAccepted(request.RequesterID, currentUser.ID, currentUser.Username)
	}

	c.JSON(http.StatusOK, gin.H{
		"status":  "accepted",
		"message": "Follow request accepted",
	})
}

// RejectFollowRequest rejects a pending follow request
// POST /api/v1/follow-requests/:id/reject
func (h *Handlers) RejectFollowRequest(c *gin.Context) {
	currentUser, ok := util.GetUserFromContext(c)
	if !ok {
		return
	}

	requestID := c.Param("id")

	// Find the follow request
	var request models.FollowRequest
	if err := database.DB.Where("id = ? AND target_id = ? AND status = ?",
		requestID, currentUser.ID, models.FollowRequestStatusPending).
		First(&request).Error; err != nil {
		if err == gorm.ErrRecordNotFound {
			util.RespondNotFound(c, "request")
			return
		}
		util.RespondInternalError(c, "fetch_failed", "Failed to fetch follow request")
		return
	}

	// Update the request status
	if err := database.DB.Model(&request).Update("status", models.FollowRequestStatusRejected).Error; err != nil {
		util.RespondInternalError(c, "update_failed", "Failed to update request status")
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"status":  "rejected",
		"message": "Follow request rejected",
	})
}

// CancelFollowRequest cancels a pending follow request that you sent
// DELETE /api/v1/follow-requests/:id
func (h *Handlers) CancelFollowRequest(c *gin.Context) {
	currentUser, ok := util.GetUserFromContext(c)
	if !ok {
		return
	}

	requestID := c.Param("id")

	// Find the follow request (must be from the current user and pending)
	var request models.FollowRequest
	if err := database.DB.Where("id = ? AND requester_id = ? AND status = ?",
		requestID, currentUser.ID, models.FollowRequestStatusPending).
		First(&request).Error; err != nil {
		if err == gorm.ErrRecordNotFound {
			util.RespondNotFound(c, "request")
			return
		}
		util.RespondInternalError(c, "fetch_failed", "Failed to fetch follow request")
		return
	}

	// Delete the request
	if err := database.DB.Delete(&request).Error; err != nil {
		util.RespondInternalError(c, "delete_failed", "Failed to cancel follow request")
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"status":  "cancelled",
		"message": "Follow request cancelled",
	})
}

// GetFollowRequestCount returns the count of pending follow requests
// GET /api/v1/users/me/follow-requests/count
func (h *Handlers) GetFollowRequestCount(c *gin.Context) {
	currentUser, ok := util.GetUserFromContext(c)
	if !ok {
		return
	}

	var count int64
	if err := database.DB.Model(&models.FollowRequest{}).
		Where("target_id = ? AND status = ?", currentUser.ID, models.FollowRequestStatusPending).
		Count(&count).Error; err != nil {
		util.RespondInternalError(c, "count_failed", "Failed to count follow requests")
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"count": count,
	})
}

// CheckFollowRequestStatus checks if there's a pending follow request between users
// GET /api/v1/users/:id/follow-request-status
func (h *Handlers) CheckFollowRequestStatus(c *gin.Context) {
	currentUser, ok := util.GetUserFromContext(c)
	if !ok {
		return
	}

	targetUserID := c.Param("id")

	// Check if there's a pending request from current user to target
	var request models.FollowRequest
	err := database.DB.Where("requester_id = ? AND target_id = ?", currentUser.ID, targetUserID).
		First(&request).Error

	if err == gorm.ErrRecordNotFound {
		c.JSON(http.StatusOK, gin.H{
			"status":      "none",
			"has_request": false,
		})
		return
	}

	if err != nil {
		util.RespondInternalError(c, "check_failed", "Failed to check follow request status")
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"status":      request.Status,
		"has_request": true,
		"request_id":  request.ID,
		"created_at":  request.CreatedAt.Format("2006-01-02T15:04:05Z"),
	})
}
