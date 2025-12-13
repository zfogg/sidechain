#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
 * Draft - Represents a saved recording draft
 *
 * Contains all the metadata needed to resume editing a recording:
 * - Audio data (stored as WAV file)
 * - Form fields (title, BPM, key, genre)
 * - MIDI data (if captured during recording)
 * - Timestamps for sorting and display
 */
struct Draft
{
    juce::String id;              // UUID for the draft
    juce::String title;           // User-entered title (may be empty)
    double bpm = 0.0;             // BPM (from DAW or manual)
    int keyIndex = 0;             // Musical key index (0 = Not set)
    int genreIndex = 0;           // Genre index
    int commentAudienceIndex = 0; // Comment audience setting
    double sampleRate = 44100.0;  // Audio sample rate
    int numSamples = 0;           // Number of audio samples
    int numChannels = 2;          // Number of audio channels
    juce::var midiData;           // MIDI data (if any)
    juce::Time createdAt;         // When draft was first created
    juce::Time updatedAt;         // When draft was last updated
    juce::String audioFilePath;   // Path to the WAV file

    /** Check if draft has valid audio */
    bool hasAudio() const { return numSamples > 0; }

    /** Check if draft has MIDI data */
    bool hasMidi() const { return !midiData.isVoid() && !midiData.isUndefined(); }

    /** Get duration in seconds */
    double getDurationSeconds() const
    {
        if (sampleRate <= 0.0) return 0.0;
        return static_cast<double>(numSamples) / sampleRate;
    }

    /** Format duration as MM:SS */
    juce::String getFormattedDuration() const
    {
        double seconds = getDurationSeconds();
        int mins = static_cast<int>(seconds) / 60;
        int secs = static_cast<int>(seconds) % 60;
        return juce::String::formatted("%02d:%02d", mins, secs);
    }

    /** Create from JSON metadata */
    static Draft fromJson(const juce::var& json);

    /** Convert to JSON for storage */
    juce::var toJson() const;
};

//==============================================================================
/**
 * DraftStorage - Local storage for recording drafts
 *
 * Stores drafts in the user's local data directory:
 * - Linux: ~/.local/share/Sidechain/drafts/
 * - macOS: ~/Library/Application Support/Sidechain/drafts/
 * - Windows: %APPDATA%/Sidechain/drafts/
 *
 * Each draft consists of:
 * - <uuid>.wav - The audio data
 * - <uuid>.json - Metadata (title, BPM, key, etc.)
 *
 * Features:
 * - Save/load drafts with audio and metadata
 * - List all drafts sorted by date
 * - Delete drafts
 * - Auto-recovery draft for crash protection
 */
class DraftStorage
{
public:
    DraftStorage();
    ~DraftStorage();

    //==========================================================================
    // Draft operations

    /** Save a new draft or update existing one
     *  @param draft Draft metadata (id will be generated if empty)
     *  @param audioBuffer Audio data to save
     *  @return The draft with updated id and file path
     */
    Draft saveDraft(const Draft& draft, const juce::AudioBuffer<float>& audioBuffer);

    /** Load a draft by ID
     *  @param draftId UUID of the draft
     *  @param audioBuffer [out] Audio data loaded from file
     *  @return Draft metadata, or empty draft if not found
     */
    Draft loadDraft(const juce::String& draftId, juce::AudioBuffer<float>& audioBuffer);

    /** Get all drafts (metadata only, no audio)
     *  @return Array of drafts sorted by updatedAt (newest first)
     */
    juce::Array<Draft> getAllDrafts();

    /** Delete a draft
     *  @param draftId UUID of the draft to delete
     *  @return true if successfully deleted
     */
    bool deleteDraft(const juce::String& draftId);

    /** Get count of drafts */
    int getDraftCount();

    //==========================================================================
    // Auto-recovery (crash protection)

    /** Save auto-recovery draft (overwrites previous)
     *  Called periodically during recording or when app is closing
     */
    void saveAutoRecoveryDraft(const Draft& draft, const juce::AudioBuffer<float>& audioBuffer);

    /** Check if auto-recovery draft exists */
    bool hasAutoRecoveryDraft();

    /** Load auto-recovery draft
     *  @param audioBuffer [out] Audio data
     *  @return Draft metadata
     */
    Draft loadAutoRecoveryDraft(juce::AudioBuffer<float>& audioBuffer);

    /** Clear auto-recovery draft (call after successful upload or explicit discard) */
    void clearAutoRecoveryDraft();

    //==========================================================================
    // Storage info

    /** Get drafts directory path */
    juce::File getDraftsDirectory() const;

    /** Get total size of all drafts in bytes */
    juce::int64 getTotalStorageUsed();

private:
    juce::File draftsDir;
    static constexpr const char* AUTO_RECOVERY_ID = "_auto_recovery";

    /** Generate a new UUID for a draft */
    juce::String generateDraftId();

    /** Get paths for a draft */
    juce::File getAudioFile(const juce::String& draftId) const;
    juce::File getMetadataFile(const juce::String& draftId) const;

    /** Write audio to WAV file */
    bool writeAudioFile(const juce::File& file, const juce::AudioBuffer<float>& buffer, double sampleRate);

    /** Read audio from WAV file */
    bool readAudioFile(const juce::File& file, juce::AudioBuffer<float>& buffer, double& sampleRate);

    /** Ensure drafts directory exists */
    void ensureDraftsDirectory();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DraftStorage)
};
