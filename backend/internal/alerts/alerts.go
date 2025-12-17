package alerts

import (
	"fmt"
	"sync"
	"time"
)

// AlertLevel represents the severity of an alert
type AlertLevel string

const (
	AlertLevelInfo     AlertLevel = "info"
	AlertLevelWarning  AlertLevel = "warning"
	AlertLevelCritical AlertLevel = "critical"
)

// AlertType represents the type of alert
type AlertType string

const (
	AlertTypeHighErrorRate     AlertType = "high_error_rate"
	AlertTypeSlowQueries       AlertType = "slow_queries"
	AlertTypeLowCacheHitRate   AlertType = "low_cache_hit_rate"
	AlertTypeHighTimeoutRate   AlertType = "high_timeout_rate"
	AlertTypeHighRejectionRate AlertType = "high_rejection_rate" // Rate limit rejections
)

// Alert represents a triggered alert
type Alert struct {
	ID          string      `json:"id"`
	Type        AlertType   `json:"type"`
	Level       AlertLevel  `json:"level"`
	Message     string      `json:"message"`
	Details     interface{} `json:"details"`
	Timestamp   time.Time   `json:"timestamp"`
	ResolvedAt  *time.Time  `json:"resolved_at,omitempty"`
	IsResolved  bool        `json:"is_resolved"`
	RuleID      string      `json:"rule_id"`
}

// AlertRule defines conditions that trigger an alert
type AlertRule struct {
	ID          string      `json:"id"`
	Name        string      `json:"name"`
	Type        AlertType   `json:"type"`
	Enabled     bool        `json:"enabled"`
	Level       AlertLevel  `json:"level"`
	Condition   string      `json:"condition"` // Human-readable condition
	Threshold   float64     `json:"threshold"`
	Duration    time.Duration `json:"duration"` // How long condition must be true
	CooldownSec int         `json:"cooldown_sec"` // Prevent alert spamming
	LastTriggered *time.Time `json:"last_triggered,omitempty"`
	CreatedAt   time.Time   `json:"created_at"`
	UpdatedAt   time.Time   `json:"updated_at"`
}

// AlertManager manages alerts and rules
type AlertManager struct {
	mu       sync.RWMutex
	alerts   map[string]*Alert
	rules    map[string]*AlertRule
	maxAlerts int
}

// NewAlertManager creates a new alert manager
func NewAlertManager() *AlertManager {
	return &AlertManager{
		alerts:    make(map[string]*Alert),
		rules:     make(map[string]*AlertRule),
		maxAlerts: 1000,
	}
}

// TriggerAlert creates and stores a new alert
func (am *AlertManager) TriggerAlert(alertType AlertType, level AlertLevel, message string, details interface{}, ruleID string) *Alert {
	am.mu.Lock()
	defer am.mu.Unlock()

	alert := &Alert{
		ID:         fmt.Sprintf("alert_%d_%s", time.Now().UnixNano(), alertType),
		Type:       alertType,
		Level:      level,
		Message:    message,
		Details:    details,
		Timestamp:  time.Now(),
		IsResolved: false,
		RuleID:     ruleID,
	}

	am.alerts[alert.ID] = alert

	// Keep only recent alerts to avoid unbounded memory growth
	if len(am.alerts) > am.maxAlerts {
		am.pruneOldAlerts()
	}

	return alert
}

// ResolveAlert marks an alert as resolved
func (am *AlertManager) ResolveAlert(alertID string) error {
	am.mu.Lock()
	defer am.mu.Unlock()

	alert, exists := am.alerts[alertID]
	if !exists {
		return fmt.Errorf("alert not found: %s", alertID)
	}

	now := time.Now()
	alert.ResolvedAt = &now
	alert.IsResolved = true

	return nil
}

// GetActiveAlerts returns all unresolved alerts
func (am *AlertManager) GetActiveAlerts() []*Alert {
	am.mu.RLock()
	defer am.mu.RUnlock()

	var active []*Alert
	for _, alert := range am.alerts {
		if !alert.IsResolved {
			active = append(active, alert)
		}
	}

	return active
}

// GetAlertsByType returns alerts of a specific type
func (am *AlertManager) GetAlertsByType(alertType AlertType) []*Alert {
	am.mu.RLock()
	defer am.mu.RUnlock()

	var filtered []*Alert
	for _, alert := range am.alerts {
		if alert.Type == alertType {
			filtered = append(filtered, alert)
		}
	}

	return filtered
}

// GetAlertsBySeverity returns alerts of a specific severity level
func (am *AlertManager) GetAlertsBySeverity(level AlertLevel) []*Alert {
	am.mu.RLock()
	defer am.mu.RUnlock()

	var filtered []*Alert
	for _, alert := range am.alerts {
		if alert.Level == level && !alert.IsResolved {
			filtered = append(filtered, alert)
		}
	}

	return filtered
}

// GetAllAlerts returns all alerts
func (am *AlertManager) GetAllAlerts() []*Alert {
	am.mu.RLock()
	defer am.mu.RUnlock()

	alerts := make([]*Alert, 0, len(am.alerts))
	for _, alert := range am.alerts {
		alerts = append(alerts, alert)
	}

	return alerts
}

// AddRule adds a new alert rule
func (am *AlertManager) AddRule(rule *AlertRule) {
	am.mu.Lock()
	defer am.mu.Unlock()

	rule.ID = fmt.Sprintf("rule_%d", time.Now().UnixNano())
	rule.CreatedAt = time.Now()
	rule.UpdatedAt = time.Now()

	am.rules[rule.ID] = rule
}

// GetRule retrieves a rule by ID
func (am *AlertManager) GetRule(ruleID string) *AlertRule {
	am.mu.RLock()
	defer am.mu.RUnlock()

	return am.rules[ruleID]
}

// GetAllRules returns all alert rules
func (am *AlertManager) GetAllRules() []*AlertRule {
	am.mu.RLock()
	defer am.mu.RUnlock()

	rules := make([]*AlertRule, 0, len(am.rules))
	for _, rule := range am.rules {
		rules = append(rules, rule)
	}

	return rules
}

// UpdateRule updates an existing rule
func (am *AlertManager) UpdateRule(ruleID string, updates *AlertRule) error {
	am.mu.Lock()
	defer am.mu.Unlock()

	rule, exists := am.rules[ruleID]
	if !exists {
		return fmt.Errorf("rule not found: %s", ruleID)
	}

	if updates.Name != "" {
		rule.Name = updates.Name
	}
	if updates.Threshold >= 0 {
		rule.Threshold = updates.Threshold
	}
	if updates.Duration > 0 {
		rule.Duration = updates.Duration
	}
	if updates.CooldownSec >= 0 {
		rule.CooldownSec = updates.CooldownSec
	}
	if updates.Condition != "" {
		rule.Condition = updates.Condition
	}

	rule.Enabled = updates.Enabled
	rule.UpdatedAt = time.Now()

	return nil
}

// pruneOldAlerts removes old alerts to keep memory bounded
func (am *AlertManager) pruneOldAlerts() {
	// Keep only last 1000 alerts, remove oldest ones
	if len(am.alerts) > am.maxAlerts {
		// Convert to slice and sort by timestamp
		alerts := make([]*Alert, 0, len(am.alerts))
		for _, alert := range am.alerts {
			alerts = append(alerts, alert)
		}

		// Simple removal of oldest resolved alerts first
		toRemove := len(am.alerts) - am.maxAlerts
		removed := 0

		for _, alert := range alerts {
			if removed >= toRemove {
				break
			}
			if alert.IsResolved {
				delete(am.alerts, alert.ID)
				removed++
			}
		}

		// If still over limit, remove oldest alerts regardless of status
		if len(am.alerts) > am.maxAlerts {
			for _, alert := range alerts {
				if removed >= toRemove {
					break
				}
				delete(am.alerts, alert.ID)
				removed++
			}
		}
	}
}

// GetStats returns alert statistics
func (am *AlertManager) GetStats() map[string]interface{} {
	am.mu.RLock()
	defer am.mu.RUnlock()

	activeCount := 0
	criticalCount := 0
	warningCount := 0
	infoCount := 0

	for _, alert := range am.alerts {
		if !alert.IsResolved {
			activeCount++
			switch alert.Level {
			case AlertLevelCritical:
				criticalCount++
			case AlertLevelWarning:
				warningCount++
			case AlertLevelInfo:
				infoCount++
			}
		}
	}

	return map[string]interface{}{
		"total_alerts":     len(am.alerts),
		"active_alerts":    activeCount,
		"critical_count":   criticalCount,
		"warning_count":    warningCount,
		"info_count":       infoCount,
		"total_rules":      len(am.rules),
		"timestamp":        time.Now().Unix(),
	}
}
