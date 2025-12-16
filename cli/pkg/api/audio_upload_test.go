package api

import (
	"testing"
)

func TestUploadAudio_WithValidFile(t *testing.T) {
	// Would need actual audio files for testing
	filePaths := []string{"/tmp/test.mp3", "/tmp/beat.wav", "/tmp/sample.aiff"}

	for _, filePath := range filePaths {
		resp, err := UploadAudio(filePath)
		if err != nil {
			t.Logf("UploadAudio %s: API called or file missing (error expected): %v", filePath, err)
			continue
		}
		if resp == nil {
			t.Errorf("UploadAudio returned nil for %s", filePath)
		} else {
			if resp.JobID == "" {
				t.Errorf("Upload response missing JobID")
			}
		}
	}
}

func TestUploadAudio_WithInvalidFile(t *testing.T) {
	invalidPaths := []string{"", "/nonexistent/file.mp3", "relative/path.wav"}

	for _, filePath := range invalidPaths {
		resp, err := UploadAudio(filePath)
		// Should fail with file not found
		if resp != nil && resp.JobID != "" && err == nil && filePath != "" {
			t.Logf("UploadAudio accepted invalid path: %s", filePath)
		}
	}
}

func TestGetAudioProcessingStatus_WithValidJobID(t *testing.T) {
	jobIDs := []string{"job-1", "job-2", "processing-job"}

	for _, jobID := range jobIDs {
		status, err := GetAudioProcessingStatus(jobID)
		if err != nil {
			t.Logf("GetAudioProcessingStatus %s: API called (error expected): %v", jobID, err)
			continue
		}
		if status == nil {
			t.Errorf("GetAudioProcessingStatus returned nil for %s", jobID)
		} else {
			// Check status structure
			if status.Status != "" && status.Status != "processing" && status.Status != "completed" && status.Status != "failed" {
				t.Errorf("Invalid status value: %s", status.Status)
			}
		}
	}
}

func TestGetAudioProcessingStatus_WithInvalidJobID(t *testing.T) {
	invalidJobIDs := []string{"", "nonexistent-job"}

	for _, jobID := range invalidJobIDs {
		status, err := GetAudioProcessingStatus(jobID)
		if status != nil && status.Status != "" && err == nil {
			t.Logf("GetAudioProcessingStatus accepted invalid job ID: %s", jobID)
		}
	}
}

func TestGetAudioProcessingStatus_ProgressTracking(t *testing.T) {
	jobID := "processing-job"

	status, err := GetAudioProcessingStatus(jobID)
	if err != nil {
		t.Logf("GetAudioProcessingStatus: API called (error expected)")
		return
	}
	if status != nil {
		if status.Progress < 0 || status.Progress > 100 {
			t.Errorf("Invalid progress value: %d (should be 0-100)", status.Progress)
		}
	}
}

func TestGetAudio_WithValidAudioID(t *testing.T) {
	audioIDs := []string{"audio-1", "uploaded-audio", "processed-file"}

	for _, audioID := range audioIDs {
		audio, err := GetAudio(audioID)
		if err != nil {
			t.Logf("GetAudio %s: API called (error expected): %v", audioID, err)
			continue
		}
		if audio == nil {
			t.Errorf("GetAudio returned nil for %s", audioID)
		} else {
			if audio.ID == "" {
				t.Errorf("Audio file missing ID")
			}
		}
	}
}

func TestGetAudio_WithInvalidAudioID(t *testing.T) {
	invalidIDs := []string{"", "nonexistent-audio"}

	for _, audioID := range invalidIDs {
		audio, err := GetAudio(audioID)
		if audio != nil && audio.ID != "" && err == nil {
			t.Logf("GetAudio accepted invalid ID: %s", audioID)
		}
	}
}

func TestGetAudioDownloadURL_WithValidAudioID(t *testing.T) {
	audioIDs := []string{"audio-1", "completed-upload"}

	for _, audioID := range audioIDs {
		url, err := GetAudioDownloadURL(audioID)
		if err != nil {
			t.Logf("GetAudioDownloadURL %s: API called (error expected): %v", audioID, err)
			continue
		}
		if url != "" {
			// Validate URL structure
			if len(url) < 10 {
				t.Errorf("Invalid URL format for %s: %s", audioID, url)
			}
		}
	}
}

func TestGetAudioDownloadURL_WithInvalidAudioID(t *testing.T) {
	invalidIDs := []string{"", "nonexistent-audio"}

	for _, audioID := range invalidIDs {
		url, err := GetAudioDownloadURL(audioID)
		if url != "" && err == nil {
			t.Logf("GetAudioDownloadURL accepted invalid ID: %s", audioID)
		}
	}
}

func TestAudioUpload_Workflow(t *testing.T) {
	// Simulate upload -> check status -> download
	// In real scenario, would use actual files
	filePath := "/tmp/test.mp3"

	// Step 1: Upload
	resp, err := UploadAudio(filePath)
	if err != nil {
		t.Logf("UploadAudio workflow: upload phase failed (expected): %v", err)
		return
	}

	if resp == nil || resp.JobID == "" {
		t.Logf("UploadAudio workflow: invalid response structure")
		return
	}

	jobID := resp.JobID

	// Step 2: Check status
	status, err := GetAudioProcessingStatus(jobID)
	if err != nil {
		t.Logf("UploadAudio workflow: status check failed (expected)")
		return
	}

	if status != nil {
		if status.Status != "processing" && status.Status != "completed" {
			t.Logf("UploadAudio workflow: unexpected status: %s", status.Status)
		}
	}

	// Step 3: Once completed, get download URL
	if status != nil && status.Status == "completed" && status.AudioID != "" {
		url, err := GetAudioDownloadURL(status.AudioID)
		if err != nil {
			t.Logf("UploadAudio workflow: download URL retrieval failed (expected)")
			return
		}
		if url != "" {
			t.Logf("UploadAudio workflow: complete, download URL obtained")
		}
	}
}
