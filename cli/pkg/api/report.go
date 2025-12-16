package api

import (
	"fmt"
	"time"

	"github.com/zfogg/sidechain/cli/pkg/client"
	"github.com/zfogg/sidechain/cli/pkg/logger"
)

// Report represents a content report
type Report struct {
	ID            string    `json:"id"`
	Type          string    `json:"type"` // post, comment, user
	TargetID      string    `json:"target_id"`
	ReporterID    string    `json:"reporter_id"`
	Reason        string    `json:"reason"`
	Description   string    `json:"description"`
	Status        string    `json:"status"` // pending, reviewing, resolved, dismissed
	AssignedToID  string    `json:"assigned_to_id,omitempty"`
	CreatedAt     time.Time `json:"created_at"`
	UpdatedAt     time.Time `json:"updated_at"`
	ResolvedAt    *time.Time `json:"resolved_at,omitempty"`
}

type ReportListResponse struct {
	Reports    []Report `json:"reports"`
	TotalCount int      `json:"total_count"`
	Page       int      `json:"page"`
	PageSize   int      `json:"page_size"`
}

// ReportContent submits a report for a piece of content
func ReportContent(reportType, targetID, reason, description string) error {
	logger.Debug("Submitting report", "type", reportType, "target_id", targetID)

	request := struct {
		Type        string `json:"type"`
		TargetID    string `json:"target_id"`
		Reason      string `json:"reason"`
		Description string `json:"description"`
	}{
		Type:        reportType,
		TargetID:    targetID,
		Reason:      reason,
		Description: description,
	}

	resp, err := client.GetClient().
		R().
		SetBody(request).
		Post("/api/v1/reports")

	if err != nil {
		return err
	}

	if !resp.IsSuccess() {
		return fmt.Errorf("failed to submit report: %s", resp.Status())
	}

	return nil
}

// GetReports retrieves reports (admin only)
func GetReports(status string, page, pageSize int) (*ReportListResponse, error) {
	logger.Debug("Fetching reports", "status", status, "page", page)

	var response ReportListResponse

	params := map[string]string{
		"page":      fmt.Sprintf("%d", page),
		"page_size": fmt.Sprintf("%d", pageSize),
	}

	if status != "" {
		params["status"] = status
	}

	resp, err := client.GetClient().
		R().
		SetQueryParams(params).
		SetResult(&response).
		Get("/api/v1/admin/reports")

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to fetch reports: %s", resp.Status())
	}

	return &response, nil
}

// GetReport retrieves a specific report
func GetReport(reportID string) (*Report, error) {
	logger.Debug("Fetching report", "report_id", reportID)

	var response struct {
		Report Report `json:"report"`
	}

	resp, err := client.GetClient().
		R().
		SetResult(&response).
		Get(fmt.Sprintf("/api/v1/admin/reports/%s", reportID))

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to fetch report: %s", resp.Status())
	}

	return &response.Report, nil
}

// ResolveReport marks a report as resolved
func ResolveReport(reportID, action string) error {
	logger.Debug("Resolving report", "report_id", reportID, "action", action)

	request := struct {
		Action string `json:"action"`
	}{
		Action: action,
	}

	resp, err := client.GetClient().
		R().
		SetBody(request).
		Patch(fmt.Sprintf("/api/v1/admin/reports/%s/resolve", reportID))

	if err != nil {
		return err
	}

	if !resp.IsSuccess() {
		return fmt.Errorf("failed to resolve report: %s", resp.Status())
	}

	return nil
}

// DismissReport marks a report as dismissed
func DismissReport(reportID string) error {
	logger.Debug("Dismissing report", "report_id", reportID)

	resp, err := client.GetClient().
		R().
		Patch(fmt.Sprintf("/api/v1/admin/reports/%s/dismiss", reportID))

	if err != nil {
		return err
	}

	if !resp.IsSuccess() {
		return fmt.Errorf("failed to dismiss report: %s", resp.Status())
	}

	return nil
}
