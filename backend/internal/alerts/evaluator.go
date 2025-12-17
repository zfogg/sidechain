package alerts

import (
	"fmt"
	"sync"
	"time"

	"github.com/zfogg/sidechain/backend/internal/metrics"
)

// Evaluator evaluates alert rules against metrics
type Evaluator struct {
	manager   *AlertManager
	mu        sync.RWMutex
	lastCheck map[string]time.Time // Track last time each rule was checked
}

// NewEvaluator creates a new alert evaluator
func NewEvaluator(manager *AlertManager) *Evaluator {
	return &Evaluator{
		manager:   manager,
		lastCheck: make(map[string]time.Time),
	}
}

// EvaluateRules checks all enabled rules against current metrics
func (e *Evaluator) EvaluateRules() {
	e.mu.Lock()
	rules := e.manager.GetAllRules()
	e.mu.Unlock()

	metrics := metrics.GetManager().GetSearchStats()

	for _, rule := range rules {
		if !rule.Enabled {
			continue
		}

		// Check if rule is in cooldown period
		if rule.LastTriggered != nil {
			timeSinceLastTrigger := time.Since(*rule.LastTriggered).Seconds()
			if timeSinceLastTrigger < float64(rule.CooldownSec) {
				continue
			}
		}

		// Evaluate rule condition
		triggered, details := e.evaluateRule(rule, metrics)

		if triggered {
			e.manager.TriggerAlert(
				rule.Type,
				rule.Level,
				fmt.Sprintf("[%s] %s", rule.Name, rule.Condition),
				details,
				rule.ID,
			)

			// Update last triggered time
			now := time.Now()
			rule.LastTriggered = &now
		}
	}
}

// evaluateRule checks a specific rule against metrics
func (e *Evaluator) evaluateRule(rule *AlertRule, stats map[string]interface{}) (bool, map[string]interface{}) {
	details := make(map[string]interface{})

	switch rule.Type {
	case AlertTypeHighErrorRate:
		errorRate, ok := stats["error_rate"].(float64)
		if ok && errorRate >= rule.Threshold {
			details["current_error_rate"] = errorRate
			details["threshold"] = rule.Threshold
			return true, details
		}

	case AlertTypeSlowQueries:
		avgTime, ok := stats["avg_query_time_ms"].(float64)
		if ok && avgTime >= rule.Threshold {
			details["avg_query_time_ms"] = avgTime
			details["threshold"] = rule.Threshold
			details["p95_query_time_ms"] = stats["p95_query_time_ms"]
			details["p99_query_time_ms"] = stats["p99_query_time_ms"]
			return true, details
		}

	case AlertTypeLowCacheHitRate:
		cacheHitRate, ok := stats["cache_hit_rate"].(float64)
		if ok && cacheHitRate <= rule.Threshold {
			details["current_cache_hit_rate"] = cacheHitRate
			details["threshold"] = rule.Threshold
			details["cache_hits"] = stats["cache_hits"]
			details["cache_misses"] = stats["cache_misses"]
			return true, details
		}

	case AlertTypeHighTimeoutRate:
		timeoutCount, ok := stats["timeout_count"].(int64)
		totalQueries, ok2 := stats["total_queries"].(int64)
		if ok && ok2 && totalQueries > 0 {
			timeoutRate := float64(timeoutCount) / float64(totalQueries) * 100
			if timeoutRate >= rule.Threshold {
				details["current_timeout_rate"] = timeoutRate
				details["threshold"] = rule.Threshold
				details["timeout_count"] = timeoutCount
				details["total_queries"] = totalQueries
				return true, details
			}
		}

	case AlertTypeHighRejectionRate:
		// This would need to be tracked from rate limiter
		// For now, placeholder
		details["message"] = "Rate limit rejections not yet tracked"
		return false, details
	}

	return false, details
}

// InitializeDefaultRules sets up default alert rules
func (e *Evaluator) InitializeDefaultRules() {
	rules := []*AlertRule{
		{
			Name:        "High Error Rate",
			Type:        AlertTypeHighErrorRate,
			Enabled:     true,
			Level:       AlertLevelCritical,
			Condition:   "Error rate > 5%",
			Threshold:   5.0,
			Duration:    5 * time.Minute,
			CooldownSec: 300, // 5 minute cooldown
		},
		{
			Name:        "Slow Queries",
			Type:        AlertTypeSlowQueries,
			Enabled:     true,
			Level:       AlertLevelWarning,
			Condition:   "Average query time > 100ms",
			Threshold:   100.0,
			Duration:    5 * time.Minute,
			CooldownSec: 300,
		},
		{
			Name:        "Low Cache Hit Rate",
			Type:        AlertTypeLowCacheHitRate,
			Enabled:     true,
			Level:       AlertLevelInfo,
			Condition:   "Cache hit rate < 50%",
			Threshold:   50.0,
			Duration:    10 * time.Minute,
			CooldownSec: 600,
		},
		{
			Name:        "High Timeout Rate",
			Type:        AlertTypeHighTimeoutRate,
			Enabled:     true,
			Level:       AlertLevelWarning,
			Condition:   "Timeout rate > 2%",
			Threshold:   2.0,
			Duration:    5 * time.Minute,
			CooldownSec: 300,
		},
	}

	for _, rule := range rules {
		e.manager.AddRule(rule)
	}
}

// StartEvaluationLoop starts periodic evaluation of rules
func (e *Evaluator) StartEvaluationLoop(interval time.Duration) chan struct{} {
	stop := make(chan struct{})

	go func() {
		ticker := time.NewTicker(interval)
		defer ticker.Stop()

		for {
			select {
			case <-ticker.C:
				e.EvaluateRules()
			case <-stop:
				return
			}
		}
	}()

	return stop
}
