package service

import (
	"bufio"
	"fmt"
	"os"
	"path/filepath"
	"strings"

	"github.com/zfogg/sidechain/cli/pkg/api"
	"github.com/zfogg/sidechain/cli/pkg/formatter"
)

// ProjectFileService provides operations for managing project files
type ProjectFileService struct{}

// NewProjectFileService creates a new project file service
func NewProjectFileService() *ProjectFileService {
	return &ProjectFileService{}
}

// ListProjectFiles lists all project files with optional filtering
func (pfs *ProjectFileService) ListProjectFiles(limit, offset int, dawType string) error {
	if limit < 1 || limit > 100 {
		limit = 20
	}
	if offset < 0 {
		offset = 0
	}

	resp, err := api.ListProjectFiles(limit, offset, dawType)
	if err != nil {
		return fmt.Errorf("failed to list project files: %w", err)
	}

	if len(resp.Files) == 0 {
		fmt.Println("No project files found")
		return nil
	}

	// Create table data
	headers := []string{"Filename", "DAW Type", "Size (MB)", "Downloads", "Visibility", "Created"}
	rows := make([][]string, len(resp.Files))

	for i, file := range resp.Files {
		visibility := "Private"
		if file.IsPublic {
			visibility = "Public"
		}
		rows[i] = []string{
			truncate(file.Filename, 25),
			file.DAWType,
			fmt.Sprintf("%.2f", float64(file.FileSize)/(1024*1024)),
			fmt.Sprintf("%d", file.DownloadCount),
			visibility,
			file.CreatedAt,
		}
	}

	formatter.PrintTable(headers, rows)
	fmt.Printf("\nPage: %d | Total: %d | Showing %d-%d\n", offset/limit+1, resp.TotalCount, offset+1, offset+len(resp.Files))

	return nil
}

// ViewProjectFile displays details for a specific project file
func (pfs *ProjectFileService) ViewProjectFile() error {
	reader := bufio.NewReader(os.Stdin)

	fmt.Print("Project File ID: ")
	fileID, _ := reader.ReadString('\n')
	fileID = strings.TrimSpace(fileID)

	if fileID == "" {
		return fmt.Errorf("file ID cannot be empty")
	}

	// Fetch file details
	file, err := api.GetProjectFile(fileID)
	if err != nil {
		return fmt.Errorf("failed to fetch project file: %w", err)
	}

	formatter.PrintInfo("📁 Project File Details")
	fmt.Printf("\n")

	visibility := "Private"
	if file.IsPublic {
		visibility = "Public"
	}

	keyValues := map[string]interface{}{
		"ID":           file.ID,
		"Filename":     file.Filename,
		"DAW Type":     file.DAWType,
		"File Size":    fmt.Sprintf("%.2f MB", float64(file.FileSize)/(1024*1024)),
		"Visibility":   visibility,
		"Downloads":    file.DownloadCount,
		"Description":  file.Description,
		"Created":      file.CreatedAt,
		"Updated":      file.UpdatedAt,
	}
	formatter.PrintKeyValue(keyValues)

	fmt.Printf("\nDownload URL: %s\n", file.FileURL)

	return nil
}

// DeleteProjectFile deletes a project file
func (pfs *ProjectFileService) DeleteProjectFile() error {
	reader := bufio.NewReader(os.Stdin)

	fmt.Print("Project File ID to delete: ")
	fileID, _ := reader.ReadString('\n')
	fileID = strings.TrimSpace(fileID)

	if fileID == "" {
		return fmt.Errorf("file ID cannot be empty")
	}

	// Confirm deletion
	fmt.Print("Are you sure you want to delete this file? (yes/no): ")
	confirm, _ := reader.ReadString('\n')
	confirm = strings.TrimSpace(strings.ToLower(confirm))

	if confirm != "yes" && confirm != "y" {
		fmt.Println("Deletion cancelled")
		return nil
	}

	// Delete the file
	err := api.DeleteProjectFile(fileID)
	if err != nil {
		return fmt.Errorf("failed to delete project file: %w", err)
	}

	formatter.PrintSuccess("✓ Project file deleted successfully!")
	fmt.Printf("\n")
	keyValues := map[string]interface{}{
		"File ID": fileID,
	}
	formatter.PrintKeyValue(keyValues)

	return nil
}

// GetDAWTypeFromExtension detects DAW type from file extension
func (pfs *ProjectFileService) GetDAWTypeFromExtension(filename string) string {
	ext := strings.ToLower(filepath.Ext(filename))

	dawMap := map[string]string{
		".als":      "ableton",
		".alp":      "ableton",
		".flp":      "fl_studio",
		".logic":    "logic",
		".logicx":   "logic",
		".ptx":      "pro_tools",
		".ptf":      "pro_tools",
		".cpr":      "cubase",
		".song":     "studio_one",
		".rpp":      "reaper",
		".bwproject": "bitwig",
	}

	if daw, ok := dawMap[ext]; ok {
		return daw
	}
	return "other"
}

// Helper function to truncate long strings
func truncateFile(s string, length int) string {
	if len(s) > length {
		return s[:length-3] + "..."
	}
	return s
}
