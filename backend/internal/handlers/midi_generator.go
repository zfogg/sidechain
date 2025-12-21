package handlers

import (
	"bytes"
	"fmt"
	"sort"

	"github.com/zfogg/sidechain/backend/internal/models"
	"gitlab.com/gomidi/midi/v2"
	"gitlab.com/gomidi/midi/v2/smf"
)

// GenerateMIDIFile converts MIDIData to a Standard MIDI File (.mid) format
func GenerateMIDIFile(data *models.MIDIData, name string) ([]byte, error) {
	if data == nil {
		return nil, fmt.Errorf("midi data is nil")
	}

	// Determine ticks per quarter note (standard resolution)
	ticksPerQuarter := smf.MetricTicks(480)

	// Create a new SMF (Standard MIDI File)
	s := smf.New()
	s.TimeFormat = ticksPerQuarter

	// Default to 120 BPM if not specified
	tempo := data.Tempo
	if tempo <= 0 {
		tempo = 120
	}

	// Get time signature (default to 4/4)
	numerator := uint8(4)
	denominator := uint8(4)
	if len(data.TimeSignature) >= 2 {
		numerator = uint8(data.TimeSignature[0])
		denominator = uint8(data.TimeSignature[1])
	}

	// Group events by channel to create separate tracks
	channelEvents := make(map[int][]models.MIDIEvent)
	for _, event := range data.Events {
		channelEvents[event.Channel] = append(channelEvents[event.Channel], event)
	}

	// If no events or all on same channel, use single track format
	if len(channelEvents) <= 1 {
		// Single track format (Type 0)
		var track smf.Track

		// Add track name
		if name != "" {
			track.Add(0, smf.MetaTrackSequenceName(name))
		}

		// Add tempo
		track.Add(0, smf.MetaTempo(float64(tempo)))

		// Add time signature
		// denominator in MIDI is expressed as power of 2 (e.g., 4 = 2^2, so we store 2)
		denomPower := uint8(0)
		for d := denominator; d > 1; d /= 2 {
			denomPower++
		}
		track.Add(0, smf.MetaTimeSig(numerator, denomPower, 24, 8))

		// Sort events by time
		events := data.Events
		sort.Slice(events, func(i, j int) bool {
			return events[i].Time < events[j].Time
		})

		// Convert events to MIDI messages using delta times
		var lastTick uint32
		for _, event := range events {
			absoluteTick := secondsToTicks(event.Time, float64(tempo), ticksPerQuarter)
			deltaTick := absoluteTick - lastTick
			lastTick = absoluteTick

			channel := uint8(event.Channel)
			note := uint8(event.Note)
			velocity := uint8(event.Velocity)

			if event.Type == "note_on" {
				track.Add(deltaTick, midi.NoteOn(channel, note, velocity))
			} else if event.Type == "note_off" {
				track.Add(deltaTick, midi.NoteOff(channel, note))
			}
		}

		// Add end of track
		endTick := secondsToTicks(data.TotalTime, float64(tempo), ticksPerQuarter)
		endDelta := endTick - lastTick
		track.Close(endDelta)

		s.Add(track)
	} else {
		// Multi-track format (Type 1)
		// First track contains tempo and time signature
		var tempoTrack smf.Track
		if name != "" {
			tempoTrack.Add(0, smf.MetaTrackSequenceName(name))
		}
		tempoTrack.Add(0, smf.MetaTempo(float64(tempo)))
		denomPower := uint8(0)
		for d := denominator; d > 1; d /= 2 {
			denomPower++
		}
		tempoTrack.Add(0, smf.MetaTimeSig(numerator, denomPower, 24, 8))
		endTick := secondsToTicks(data.TotalTime, float64(tempo), ticksPerQuarter)
		tempoTrack.Close(endTick)
		s.Add(tempoTrack)

		// Add a track for each channel
		channels := make([]int, 0, len(channelEvents))
		for ch := range channelEvents {
			channels = append(channels, ch)
		}
		sort.Ints(channels)

		for _, ch := range channels {
			events := channelEvents[ch]
			sort.Slice(events, func(i, j int) bool {
				return events[i].Time < events[j].Time
			})

			var track smf.Track
			track.Add(0, smf.MetaTrackSequenceName(fmt.Sprintf("Channel %d", ch+1)))

			var lastTick uint32
			for _, event := range events {
				absoluteTick := secondsToTicks(event.Time, float64(tempo), ticksPerQuarter)
				deltaTick := absoluteTick - lastTick
				lastTick = absoluteTick

				channel := uint8(event.Channel)
				note := uint8(event.Note)
				velocity := uint8(event.Velocity)

				if event.Type == "note_on" {
					track.Add(deltaTick, midi.NoteOn(channel, note, velocity))
				} else if event.Type == "note_off" {
					track.Add(deltaTick, midi.NoteOff(channel, note))
				}
			}

			endDelta := endTick - lastTick
			track.Close(endDelta)
			s.Add(track)
		}
	}

	// Write to buffer
	var buf bytes.Buffer
	_, err := s.WriteTo(&buf)
	if err != nil {
		return nil, fmt.Errorf("failed to write MIDI file: %w", err)
	}

	return buf.Bytes(), nil
}

// secondsToTicks converts time in seconds to MIDI ticks
func secondsToTicks(seconds float64, bpm float64, resolution smf.MetricTicks) uint32 {
	// ticks = seconds * (bpm / 60) * ticksPerQuarter
	ticksPerSecond := (bpm / 60.0) * float64(resolution)
	return uint32(seconds * ticksPerSecond)
}

// ParseMIDIFile parses a Standard MIDI File and returns MIDIData
func ParseMIDIFile(data []byte) (*models.MIDIData, error) {
	s, err := smf.ReadFrom(bytes.NewReader(data))
	if err != nil {
		return nil, fmt.Errorf("failed to parse MIDI file: %w", err)
	}

	result := &models.MIDIData{
		Events:        make([]models.MIDIEvent, 0),
		Tempo:         120,         // Default
		TimeSignature: []int{4, 4}, // Default
	}

	// Get ticks per quarter from the file
	ticksPerQuarter := uint32(480)
	if mt, ok := s.TimeFormat.(smf.MetricTicks); ok {
		ticksPerQuarter = uint32(mt)
	}

	var maxTime float64

	// Process all tracks
	for _, track := range s.Tracks {
		var absoluteTick uint32

		for _, ev := range track {
			absoluteTick += ev.Delta

			// Handle meta events
			var tempoBPM float64
			if ev.Message.GetMetaTempo(&tempoBPM) {
				result.Tempo = int(tempoBPM)
			}

			var num, denomPow, clocksPerClick, thirtySecondNotesPerQuarter uint8
			if ev.Message.GetMetaTimeSig(&num, &denomPow, &clocksPerClick, &thirtySecondNotesPerQuarter) {
				denom := 1 << denomPow
				result.TimeSignature = []int{int(num), denom}
			}

			// Handle note events
			var channel, note, velocity uint8
			if ev.Message.GetNoteOn(&channel, &note, &velocity) {
				timeSeconds := ticksToSeconds(absoluteTick, float64(result.Tempo), ticksPerQuarter)
				if velocity > 0 {
					result.Events = append(result.Events, models.MIDIEvent{
						Time:     timeSeconds,
						Type:     "note_on",
						Note:     int(note),
						Velocity: int(velocity),
						Channel:  int(channel),
					})
				} else {
					// Note on with velocity 0 is equivalent to note off
					result.Events = append(result.Events, models.MIDIEvent{
						Time:     timeSeconds,
						Type:     "note_off",
						Note:     int(note),
						Velocity: 0,
						Channel:  int(channel),
					})
				}
				if timeSeconds > maxTime {
					maxTime = timeSeconds
				}
			}

			if ev.Message.GetNoteOff(&channel, &note, &velocity) {
				timeSeconds := ticksToSeconds(absoluteTick, float64(result.Tempo), ticksPerQuarter)
				result.Events = append(result.Events, models.MIDIEvent{
					Time:     timeSeconds,
					Type:     "note_off",
					Note:     int(note),
					Velocity: int(velocity),
					Channel:  int(channel),
				})
				if timeSeconds > maxTime {
					maxTime = timeSeconds
				}
			}
		}
	}

	result.TotalTime = maxTime

	// Sort events by time
	sort.Slice(result.Events, func(i, j int) bool {
		return result.Events[i].Time < result.Events[j].Time
	})

	return result, nil
}

// ticksToSeconds converts MIDI ticks to time in seconds
func ticksToSeconds(ticks uint32, bpm float64, ticksPerQuarter uint32) float64 {
	// seconds = ticks / (bpm / 60 * ticksPerQuarter)
	ticksPerSecond := (bpm / 60.0) * float64(ticksPerQuarter)
	return float64(ticks) / ticksPerSecond
}
