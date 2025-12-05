#pragma once

#include <JuceHeader.h>
#include <functional>
#include <memory>
#include <map>

class NetworkClient;

//==============================================================================
/**
 * AudioPlayer - Handles audio playback for the feed
 *
 * In a VST plugin context, we can't use standard audio output devices.
 * Instead, this class:
 * 1. Downloads audio from URLs to memory
 * 2. Decodes using JUCE's AudioFormatReader
 * 3. Mixes playback into the plugin's processBlock output
 *
 * Key features:
 * - URL-based audio streaming
 * - Transport controls (play, pause, seek)
 * - Playback progress tracking
 * - Volume control
 * - Audio caching (LRU with memory limit)
 */
class AudioPlayer : public juce::ChangeListener
{
public:
    AudioPlayer();
    ~AudioPlayer() override;

    //==============================================================================
    // Transport Controls

    /**
     * Load and play audio from a URL
     * @param postId Unique identifier for the post
     * @param audioUrl URL to the audio file (MP3, WAV, etc.)
     */
    void loadAndPlay(const juce::String& postId, const juce::String& audioUrl);

    /**
     * Play the currently loaded audio
     */
    void play();

    /**
     * Pause playback
     */
    void pause();

    /**
     * Stop playback and reset position
     */
    void stop();

    /**
     * Toggle between play and pause
     */
    void togglePlayPause();

    /**
     * Seek to a position
     * @param positionSeconds Position in seconds
     */
    void seekToPosition(double positionSeconds);

    /**
     * Seek to a normalized position (0.0 to 1.0)
     * @param normalizedPosition Position from 0 to 1
     */
    void seekToNormalizedPosition(double normalizedPosition);

    //==============================================================================
    // State Queries

    bool isPlaying() const { return playing; }
    bool isLoading() const { return loading; }

    /** Get current playback position in seconds */
    double getPositionSeconds() const;

    /** Get total duration in seconds */
    double getDurationSeconds() const;

    /** Get playback progress (0.0 to 1.0) */
    double getPlaybackProgress() const;

    /** Get the currently playing post ID */
    juce::String getCurrentPostId() const { return currentPostId; }

    /** Check if a specific post is currently playing */
    bool isPostPlaying(const juce::String& postId) const;

    //==============================================================================
    // Volume Control

    /** Set volume (0.0 to 1.0) */
    void setVolume(float newVolume);

    /** Get current volume */
    float getVolume() const { return volume; }

    /** Mute/unmute */
    void setMuted(bool shouldMute);
    bool isMuted() const { return muted; }

    //==============================================================================
    // Auto-play and Queue Management

    /** Enable/disable auto-play next post when current finishes */
    void setAutoPlayEnabled(bool enabled) { autoPlayEnabled = enabled; }
    bool isAutoPlayEnabled() const { return autoPlayEnabled; }

    /** Set the playlist of posts for auto-play and pre-buffering */
    void setPlaylist(const juce::StringArray& postIds, const juce::StringArray& audioUrls);

    /** Get index of current post in playlist (-1 if not in playlist) */
    int getCurrentPlaylistIndex() const;

    /** Skip to next post in playlist */
    void playNext();

    /** Skip to previous post in playlist */
    void playPrevious();

    //==============================================================================
    // Audio Focus (DAW awareness)

    /**
     * Notify that DAW transport is playing
     * If audio focus is enabled, this will pause feed playback
     */
    void onDAWTransportStarted();

    /**
     * Notify that DAW transport has stopped
     * If audio focus is enabled and we were playing before, resume playback
     */
    void onDAWTransportStopped();

    /** Enable/disable audio focus (pause when DAW plays) */
    void setAudioFocusEnabled(bool enabled) { audioFocusEnabled = enabled; }
    bool isAudioFocusEnabled() const { return audioFocusEnabled; }

    //==============================================================================
    // Audio Processing (called from PluginProcessor::processBlock)

    /**
     * Process and mix playback audio into the output buffer
     * Should be called from the audio thread only
     * @param buffer The audio buffer to mix into
     * @param numSamples Number of samples to process
     */
    void processBlock(juce::AudioBuffer<float>& buffer, int numSamples);

    /**
     * Prepare for playback
     * Should be called from PluginProcessor::prepareToPlay
     * @param sampleRate The DAW's sample rate
     * @param blockSize Maximum block size
     */
    void prepareToPlay(double sampleRate, int blockSize);

    /**
     * Release resources
     * Should be called from PluginProcessor::releaseResources
     */
    void releaseResources();

    //==============================================================================
    // Callbacks

    /** Called when playback starts */
    std::function<void(const juce::String& postId)> onPlaybackStarted;

    /** Called when playback pauses */
    std::function<void(const juce::String& postId)> onPlaybackPaused;

    /** Called when playback stops (end of file) */
    std::function<void(const juce::String& postId)> onPlaybackStopped;

    /** Called when loading starts */
    std::function<void(const juce::String& postId)> onLoadingStarted;

    /** Called when loading completes */
    std::function<void(const juce::String& postId, bool success)> onLoadingComplete;

    /** Called periodically with playback progress */
    std::function<void(const juce::String& postId, double progress)> onProgressUpdate;

    /** Called when playback finishes (reached end of audio) - for auto-play */
    std::function<void(const juce::String& postId)> onPlaybackFinished;

    /** Called when auto-play moves to next post */
    std::function<void(const juce::String& postId)> onAutoPlayNext;

    //==============================================================================
    // Cache Management

    /** Clear the audio cache */
    void clearCache();

    /** Set maximum cache size in bytes (default: 50MB) */
    void setMaxCacheSize(size_t bytes);

    /** Get current cache size in bytes */
    size_t getCurrentCacheSize() const;

    /** Preload audio for a post (for seamless playback) */
    void preloadAudio(const juce::String& postId, const juce::String& audioUrl);

    /** Set NetworkClient for HTTP requests */
    void setNetworkClient(NetworkClient* client) { networkClient = client; }

    //==============================================================================
    // ChangeListener
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

private:
    //==============================================================================
    // Audio playback
    juce::AudioFormatManager formatManager;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    std::unique_ptr<juce::ResamplingAudioSource> resamplingSource;

    // Transport state
    std::atomic<bool> playing { false };
    std::atomic<bool> loading { false };
    std::atomic<bool> muted { false };
    std::atomic<float> volume { 0.8f };

    // Auto-play state
    std::atomic<bool> autoPlayEnabled { true };
    juce::StringArray playlistPostIds;
    juce::StringArray playlistAudioUrls;
    juce::CriticalSection playlistLock;

    // Audio focus state (pause when DAW plays)
    std::atomic<bool> audioFocusEnabled { true };
    std::atomic<bool> pausedByDAW { false };
    std::atomic<bool> wasPlayingBeforeDAW { false };

    // Current playback info
    juce::String currentPostId;
    juce::String currentAudioUrl;
    double currentSampleRate = 44100.0;
    int currentBlockSize = 512;
    
    // Listen duration tracking
    juce::Time playbackStartTime;
    bool playbackStarted = false;

    // Thread safety
    juce::CriticalSection audioLock;

    //==============================================================================
    // Audio Cache
    struct CachedAudio
    {
        std::unique_ptr<juce::MemoryBlock> audioData;
        juce::String mimeType;
        size_t sizeBytes = 0;
        juce::int64 lastAccessTime = 0;
    };

    std::map<juce::String, CachedAudio> audioCache;
    size_t maxCacheSize = 50 * 1024 * 1024; // 50MB default
    size_t currentCacheSize = 0;
    juce::CriticalSection cacheLock;

    // NetworkClient for HTTP requests
    NetworkClient* networkClient = nullptr;

    void evictCacheIfNeeded(size_t bytesNeeded);
    void addToCache(const juce::String& postId, std::unique_ptr<juce::MemoryBlock> data);
    juce::MemoryBlock* getFromCache(const juce::String& postId);

    //==============================================================================
    // Loading
    void downloadAudio(const juce::String& postId, const juce::String& url);
    void loadFromMemory(const juce::String& postId, juce::MemoryBlock& audioData);

    //==============================================================================
    // Progress timer
    class ProgressTimer : public juce::Timer
    {
    public:
        ProgressTimer(AudioPlayer& p) : player(p) {}
        void timerCallback() override;
    private:
        AudioPlayer& player;
    };

    std::unique_ptr<ProgressTimer> progressTimer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPlayer)
};
