package handlers

import (
	"net/http"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/zfogg/sidechain/backend/internal/alerts"
)

// Global alert manager and evaluator (will be set by main.go)
var alertManager *alerts.AlertManager
var alertEvaluator *alerts.Evaluator

// SetAlertManager sets the global alert manager
func SetAlertManager(manager *alerts.AlertManager) {
	alertManager = manager
}

// SetAlertEvaluator sets the global alert evaluator
func SetAlertEvaluator(evaluator *alerts.Evaluator) {
	alertEvaluator = evaluator
}

// GetAlerts returns all alerts
// GET /api/v1/alerts
func (h *Handlers) GetAlerts(c *gin.Context) {
	if alertManager == nil {
		c.JSON(http.StatusServiceUnavailable, gin.H{"error": "alert_system_unavailable"})
		return
	}

	// Filter by resolved status
	includeResolved := c.DefaultQuery("include_resolved", "false") == "true"

	allAlerts := alertManager.GetAllAlerts()
	if !includeResolved {
		filtered := make([]*alerts.Alert, 0)
		for _, alert := range allAlerts {
			if !alert.IsResolved {
				filtered = append(filtered, alert)
			}
		}
		allAlerts = filtered
	}

	c.JSON(http.StatusOK, gin.H{
		"alerts": allAlerts,
		"count":  len(allAlerts),
		"stats":  alertManager.GetStats(),
	})
}

// GetActiveAlerts returns only active/unresolved alerts
// GET /api/v1/alerts/active
func (h *Handlers) GetActiveAlerts(c *gin.Context) {
	if alertManager == nil {
		c.JSON(http.StatusServiceUnavailable, gin.H{"error": "alert_system_unavailable"})
		return
	}

	allActiveAlerts := alertManager.GetActiveAlerts()

	c.JSON(http.StatusOK, gin.H{
		"alerts": allActiveAlerts,
		"count":  len(allActiveAlerts),
		"stats":  alertManager.GetStats(),
	})
}

// GetAlertsByType returns alerts of a specific type
// GET /api/v1/alerts/type/:type
func (h *Handlers) GetAlertsByType(c *gin.Context) {
	if alertManager == nil {
		c.JSON(http.StatusServiceUnavailable, gin.H{"error": "alert_system_unavailable"})
		return
	}

	alertType := alerts.AlertType(c.Param("type"))
	typeAlerts := alertManager.GetAlertsByType(alertType)

	c.JSON(http.StatusOK, gin.H{
		"alerts": typeAlerts,
		"count":  len(typeAlerts),
		"type":   alertType,
	})
}

// ResolveAlert marks an alert as resolved
// PUT /api/v1/alerts/:id/resolve
func (h *Handlers) ResolveAlert(c *gin.Context) {
	if alertManager == nil {
		c.JSON(http.StatusServiceUnavailable, gin.H{"error": "alert_system_unavailable"})
		return
	}

	alertID := c.Param("id")
	err := alertManager.ResolveAlert(alertID)
	if err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "alert_not_found"})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"message": "Alert resolved",
		"alert_id": alertID,
	})
}

// GetRules returns all alert rules
// GET /api/v1/alerts/rules
func (h *Handlers) GetRules(c *gin.Context) {
	if alertManager == nil {
		c.JSON(http.StatusServiceUnavailable, gin.H{"error": "alert_system_unavailable"})
		return
	}

	allRules := alertManager.GetAllRules()

	c.JSON(http.StatusOK, gin.H{
		"rules": allRules,
		"count": len(allRules),
	})
}

// CreateRule creates a new alert rule
// POST /api/v1/alerts/rules
func (h *Handlers) CreateRule(c *gin.Context) {
	if alertManager == nil {
		c.JSON(http.StatusServiceUnavailable, gin.H{"error": "alert_system_unavailable"})
		return
	}

	var req struct {
		Name        string    `json:"name" binding:"required"`
		Type        alerts.AlertType `json:"type" binding:"required"`
		Enabled     bool      `json:"enabled"`
		Level       alerts.AlertLevel `json:"level" binding:"required"`
		Condition   string    `json:"condition" binding:"required"`
		Threshold   float64   `json:"threshold" binding:"required"`
		Duration    int       `json:"duration_sec" binding:"required"`
		CooldownSec int       `json:"cooldown_sec" binding:"required"`
	}

	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "invalid_request", "details": err.Error()})
		return
	}

	rule := &alerts.AlertRule{
		Name:        req.Name,
		Type:        req.Type,
		Enabled:     req.Enabled,
		Level:       req.Level,
		Condition:   req.Condition,
		Threshold:   req.Threshold,
		Duration:    time.Duration(req.Duration) * time.Second,
		CooldownSec: req.CooldownSec,
		CreatedAt:   time.Now(),
		UpdatedAt:   time.Now(),
	}

	alertManager.AddRule(rule)

	c.JSON(http.StatusCreated, gin.H{
		"message": "Rule created",
		"rule":    rule,
	})
}

// UpdateRule updates an alert rule
// PUT /api/v1/alerts/rules/:id
func (h *Handlers) UpdateRule(c *gin.Context) {
	if alertManager == nil {
		c.JSON(http.StatusServiceUnavailable, gin.H{"error": "alert_system_unavailable"})
		return
	}

	ruleID := c.Param("id")
	rule := alertManager.GetRule(ruleID)
	if rule == nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "rule_not_found"})
		return
	}

	var updates alerts.AlertRule
	if err := c.ShouldBindJSON(&updates); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "invalid_request"})
		return
	}

	err := alertManager.UpdateRule(ruleID, &updates)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"message": "Rule updated",
		"rule":    alertManager.GetRule(ruleID),
	})
}

// GetAlertStats returns alert statistics
// GET /api/v1/alerts/stats
func (h *Handlers) GetAlertStats(c *gin.Context) {
	if alertManager == nil {
		c.JSON(http.StatusServiceUnavailable, gin.H{"error": "alert_system_unavailable"})
		return
	}

	c.JSON(http.StatusOK, alertManager.GetStats())
}
