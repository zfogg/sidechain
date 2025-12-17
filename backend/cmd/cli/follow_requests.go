package main

import (
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"text/tabwriter"

	"github.com/spf13/cobra"
)

var followRequestsCmd = &cobra.Command{
	Use:   "follow-requests",
	Short: "Manage follow requests for your private account",
	Long:  "Commands for managing pending follow requests if your account is private",
}

var listFollowRequestsCmd = &cobra.Command{
	Use:   "list",
	Short: "List pending follow requests",
	RunE: func(cmd *cobra.Command, args []string) error {
		return listFollowRequests()
	},
}

var acceptFollowRequestCmd = &cobra.Command{
	Use:   "accept <request-id>",
	Short: "Accept a follow request",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		return acceptFollowRequest(args[0])
	},
}

var rejectFollowRequestCmd = &cobra.Command{
	Use:   "reject <request-id>",
	Short: "Reject a follow request",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		return rejectFollowRequest(args[0])
	},
}

func init() {
	followRequestsCmd.AddCommand(listFollowRequestsCmd)
	followRequestsCmd.AddCommand(acceptFollowRequestCmd)
	followRequestsCmd.AddCommand(rejectFollowRequestCmd)
}

type FollowRequest struct {
	ID        string `json:"id"`
	RequesterID string `json:"requester_id"`
	Username  string `json:"username,omitempty"`
	Status    string `json:"status"`
	CreatedAt string `json:"created_at"`
}

type FollowRequestsResponse struct {
	FollowRequests []FollowRequest `json:"follow_requests"`
	Total          int             `json:"total"`
	Offset         int             `json:"offset"`
	Limit          int             `json:"limit"`
}

func listFollowRequests() error {
	req, err := http.NewRequest("GET", apiURL+"/api/v1/users/me/follow-requests?limit=50&offset=0", nil)
	if err != nil {
		return fmt.Errorf("failed to create request: %w", err)
	}

	req.Header.Set("Authorization", "Bearer "+authToken)

	client := &http.Client{}
	resp, err := client.Do(req)
	if err != nil {
		return fmt.Errorf("failed to make request: %w", err)
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return fmt.Errorf("failed to read response: %w", err)
	}

	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		var errResp map[string]interface{}
		json.Unmarshal(body, &errResp)
		if msg, ok := errResp["message"].(string); ok {
			return fmt.Errorf("API error: %s", msg)
		}
		return fmt.Errorf("API error: status %d", resp.StatusCode)
	}

	var result FollowRequestsResponse
	if err := json.Unmarshal(body, &result); err != nil {
		return fmt.Errorf("failed to parse response: %w", err)
	}

	if output == "json" {
		fmt.Println(string(body))
	} else {
		if result.Total == 0 {
			fmt.Printf("✓ No pending follow requests\n")
			return nil
		}

		fmt.Printf("\n📝 Pending Follow Requests (%d)\n", result.Total)
		fmt.Printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n")

		w := tabwriter.NewWriter(nil, 0, 0, 2, ' ', 0)
		fmt.Fprintln(w, "ID\tUSERNAME\tSTATUS\tCREATED")

		for _, req := range result.FollowRequests {
			fmt.Fprintf(w, "%s\t%s\t%s\t%s\n",
				truncateString(req.ID, 8),
				req.Username,
				req.Status,
				req.CreatedAt)
		}

		w.Flush()
		fmt.Printf("\nUse: sidechain follow-requests accept <id>\n")
		fmt.Printf("     sidechain follow-requests reject <id>\n")
	}

	return nil
}

func acceptFollowRequest(requestID string) error {
	req, err := http.NewRequest("POST", apiURL+"/api/v1/follow-requests/"+requestID+"/accept", nil)
	if err != nil {
		return fmt.Errorf("failed to create request: %w", err)
	}

	req.Header.Set("Authorization", "Bearer "+authToken)
	req.Header.Set("Content-Type", "application/json")

	client := &http.Client{}
	resp, err := client.Do(req)
	if err != nil {
		return fmt.Errorf("failed to make request: %w", err)
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return fmt.Errorf("failed to read response: %w", err)
	}

	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		var errResp map[string]interface{}
		json.Unmarshal(body, &errResp)
		if msg, ok := errResp["message"].(string); ok {
			return fmt.Errorf("API error: %s", msg)
		}
		return fmt.Errorf("API error: status %d", resp.StatusCode)
	}

	if output == "json" {
		fmt.Println(string(body))
	} else {
		fmt.Printf("✓ Follow request accepted\n")
	}

	return nil
}

func rejectFollowRequest(requestID string) error {
	req, err := http.NewRequest("POST", apiURL+"/api/v1/follow-requests/"+requestID+"/reject", nil)
	if err != nil {
		return fmt.Errorf("failed to create request: %w", err)
	}

	req.Header.Set("Authorization", "Bearer "+authToken)
	req.Header.Set("Content-Type", "application/json")

	client := &http.Client{}
	resp, err := client.Do(req)
	if err != nil {
		return fmt.Errorf("failed to make request: %w", err)
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return fmt.Errorf("failed to read response: %w", err)
	}

	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		var errResp map[string]interface{}
		json.Unmarshal(body, &errResp)
		if msg, ok := errResp["message"].(string); ok {
			return fmt.Errorf("API error: %s", msg)
		}
		return fmt.Errorf("API error: status %d", resp.StatusCode)
	}

	if output == "json" {
		fmt.Println(string(body))
	} else {
		fmt.Printf("✓ Follow request rejected\n")
	}

	return nil
}

func truncateString(s string, maxLen int) string {
	if len(s) > maxLen {
		return s[:maxLen] + "..."
	}
	return s
}
