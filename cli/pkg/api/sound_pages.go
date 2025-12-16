package api

import (
	"fmt"

	"github.com/zfogg/sidechain/cli/pkg/client"
	"github.com/zfogg/sidechain/cli/pkg/logger"
)

// GetFeaturedSounds retrieves featured sounds
func GetFeaturedSounds(page, pageSize int) (*SoundSearchResponse, error) {
	logger.Debug("Getting featured sounds", "page", page, "page_size", pageSize)

	var result SoundSearchResponse
	resp, err := client.GetClient().
		R().
		SetQueryParams(map[string]string{
			"page":      fmt.Sprintf("%d", page),
			"page_size": fmt.Sprintf("%d", pageSize),
		}).
		SetResult(&result).
		Get("/api/v1/sounds/featured")

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to get featured sounds: %s", resp.Status())
	}

	return &result, nil
}

// GetRecentSounds retrieves recently uploaded sounds
func GetRecentSounds(page, pageSize int) (*SoundSearchResponse, error) {
	logger.Debug("Getting recent sounds", "page", page, "page_size", pageSize)

	var result SoundSearchResponse
	resp, err := client.GetClient().
		R().
		SetQueryParams(map[string]string{
			"page":      fmt.Sprintf("%d", page),
			"page_size": fmt.Sprintf("%d", pageSize),
			"sort":      "recent",
		}).
		SetResult(&result).
		Get("/api/v1/sounds")

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to get recent sounds: %s", resp.Status())
	}

	return &result, nil
}

// UpdateSoundMetadata updates sound metadata (name, description)
func UpdateSoundMetadata(soundID string, name string, description string) (*Sound, error) {
	logger.Debug("Updating sound metadata", "sound_id", soundID)

	request := map[string]string{
		"name":        name,
		"description": description,
	}

	var result Sound
	resp, err := client.GetClient().
		R().
		SetBody(request).
		SetResult(&result).
		Patch(fmt.Sprintf("/api/v1/sounds/%s", soundID))

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to update sound: %s", resp.Status())
	}

	return &result, nil
}
