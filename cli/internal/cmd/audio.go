package cmd

import (
	"github.com/spf13/cobra"
	"github.com/zfogg/sidechain/cli/pkg/service"
)

var audioCmd = &cobra.Command{
	Use:   "audio",
	Short: "Manage audio files",
	Long:  "Upload, manage, and retrieve audio files",
}

var audioUploadCmd = &cobra.Command{
	Use:   "upload",
	Short: "Upload an audio file",
	Long:  "Upload an MP3, WAV, AAC, FLAC, or M4A audio file to the server",
	RunE: func(cmd *cobra.Command, args []string) error {
		svc := service.NewAudioUploadService()
		return svc.UploadAudio()
	},
}

var audioStatusCmd = &cobra.Command{
	Use:   "status [job-id]",
	Short: "Check audio processing status",
	Long:  "Check the processing status of an uploaded audio file",
	RunE: func(cmd *cobra.Command, args []string) error {
		jobID := ""
		if len(args) > 0 {
			jobID = args[0]
		}
		svc := service.NewAudioUploadService()
		return svc.CheckAudioStatus(jobID)
	},
}

var audioGetCmd = &cobra.Command{
	Use:   "get",
	Short: "Get audio file details",
	Long:  "Retrieve details and download URL for an audio file",
	RunE: func(cmd *cobra.Command, args []string) error {
		svc := service.NewAudioUploadService()
		return svc.GetAudio()
	},
}

func init() {
	// Add subcommands
	audioCmd.AddCommand(audioUploadCmd)
	audioCmd.AddCommand(audioStatusCmd)
	audioCmd.AddCommand(audioGetCmd)

	// Add audio command to root
	rootCmd.AddCommand(audioCmd)
}
