package service

import (
	"bufio"
	"fmt"
	"os"
	"strings"

	"github.com/zfogg/sidechain/cli/pkg/api"
	"github.com/zfogg/sidechain/cli/pkg/formatter"
)

// SoundPagesService provides sound pages and discovery operations
type SoundPagesService struct{}

// NewSoundPagesService creates a new sound pages service
func NewSoundPagesService() *SoundPagesService {
	return &SoundPagesService{}
}

// BrowseFeaturedSounds displays featured sounds
func (sps *SoundPagesService) BrowseFeaturedSounds(page, pageSize int) error {
	sounds, err := api.GetFeaturedSounds(page, pageSize)
	if err != nil {
		return err
	}

	if len(sounds.Sounds) == 0 {
		fmt.Println("No featured sounds found")
		return nil
	}

	formatter.PrintInfo("⭐ Featured Sounds")
	fmt.Printf("\n")

	sps.displaySoundsList(sounds)
	fmt.Printf("Total: %d sounds\n", sounds.TotalCount)

	return nil
}

// BrowseRecentSounds displays recently uploaded sounds
func (sps *SoundPagesService) BrowseRecentSounds(page, pageSize int) error {
	sounds, err := api.GetRecentSounds(page, pageSize)
	if err != nil {
		return err
	}

	if len(sounds.Sounds) == 0 {
		fmt.Println("No recent sounds found")
		return nil
	}

	formatter.PrintInfo("🆕 Recent Sounds")
	fmt.Printf("\n")

	sps.displaySoundsList(sounds)
	fmt.Printf("Total: %d sounds\n", sounds.TotalCount)

	return nil
}

// UpdateSoundMetadata updates sound name and description
func (sps *SoundPagesService) UpdateSoundMetadata() error {
	reader := bufio.NewReader(os.Stdin)

	fmt.Print("Sound ID: ")
	soundID, _ := reader.ReadString('\n')
	soundID = strings.TrimSpace(soundID)

	if soundID == "" {
		return fmt.Errorf("sound ID cannot be empty")
	}

	fmt.Print("New name: ")
	name, _ := reader.ReadString('\n')
	name = strings.TrimSpace(name)

	fmt.Print("New description (optional): ")
	description, _ := reader.ReadString('\n')
	description = strings.TrimSpace(description)

	if name == "" {
		return fmt.Errorf("sound name cannot be empty")
	}

	sound, err := api.UpdateSoundMetadata(soundID, name, description)
	if err != nil {
		return err
	}

	formatter.PrintSuccess("Sound updated: %s", sound.Name)
	return nil
}

// Helper functions

func (sps *SoundPagesService) displaySoundsList(sounds *api.SoundSearchResponse) {
	for i, sound := range sounds.Sounds {
		fmt.Printf("[%d] %s\n", i+1, sound.Name)
		if sound.Description != "" {
			fmt.Printf("    Description: %s\n", truncate(sound.Description, 60))
		}
		if len(sound.Genre) > 0 {
			fmt.Printf("    Genre: %s\n", strings.Join(sound.Genre, ", "))
		}
		if sound.BPM > 0 {
			fmt.Printf("    BPM: %d\n", sound.BPM)
		}
		if sound.Key != "" {
			fmt.Printf("    Key: %s\n", sound.Key)
		}
		fmt.Printf("    Duration: %d seconds | Plays: %d | Downloads: %d\n\n", sound.Duration, sound.PlayCount, sound.DownloadCount)
	}
}
