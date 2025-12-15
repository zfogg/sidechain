package main

import (
	"bytes"
	"fmt"
	"io"
	"log"
	"net/http"
	"os"
	"os/exec"

	"github.com/go-audio/wav"
)

func main() {
	// Download first post's audio
	resp, _ := http.Get("https://sidechain-media.s3.us-east-1.amazonaws.com/uploads/b3516927-1e37-40b9-a6d6-d83ea3517ca4/93cf4f27-a59c-42af-a8e6-2a7158c3ad31.mp3")
	audioData, _ := io.ReadAll(resp.Body)
	resp.Body.Close()

	fmt.Printf("Downloaded %d bytes\n", len(audioData))

	// Convert to WAV
	cmd := exec.Command("ffmpeg",
		"-i", "pipe:0",
		"-f", "wav",
		"-acodec", "pcm_s16le",
		"-ar", "44100",
		"-ac", "1",
		"-loglevel", "error",
		"pipe:1",
	)
	cmd.Stdin = bytes.NewReader(audioData)
	var stdout bytes.Buffer
	cmd.Stdout = &stdout
	cmd.Run()

	fmt.Printf("Converted to %d bytes WAV\n", stdout.Len())

	// Save to file
	os.WriteFile("/tmp/test.wav", stdout.Bytes(), 0644)
	fmt.Println("Saved to /tmp/test.wav")

	// Try to decode
	wavReader := bytes.NewReader(stdout.Bytes())
	decoder := wav.NewDecoder(wavReader)

	fmt.Printf("Is valid: %v\n", decoder.IsValidFile())
	fmt.Printf("Sample rate: %d\n", decoder.SampleRate)
	fmt.Printf("Bit depth: %d\n", decoder.BitDepth)
	fmt.Printf("Channels: %d\n", decoder.NumChans)

	buf, err := decoder.FullPCMBuffer()
	if err != nil {
		log.Fatalf("Decode error: %v", err)
	}

	if buf == nil {
		log.Fatal("Buffer is nil")
	}

	fmt.Printf("Buffer data length: %d\n", len(buf.Data))
	if buf.Format != nil {
		fmt.Printf("Buffer format: SampleRate=%d, Channels=%d\n",
			buf.Format.SampleRate, buf.Format.NumChannels)
	}
}
