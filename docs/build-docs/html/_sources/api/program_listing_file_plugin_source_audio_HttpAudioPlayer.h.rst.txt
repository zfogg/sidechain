
.. _program_listing_file_plugin_source_audio_HttpAudioPlayer.h:

Program Listing for File HttpAudioPlayer.h
==========================================

|exhale_lsh| :ref:`Return to documentation for file <file_plugin_source_audio_HttpAudioPlayer.h>` (``plugin/source/audio/HttpAudioPlayer.h``)

.. |exhale_lsh| unicode:: U+021B0 .. UPWARDS ARROW WITH TIP LEFTWARDS

.. code-block:: cpp

   #pragma once
   
   #include <JuceHeader.h>
   #include <functional>
   #include <memory>
   #include <map>
   
   class NetworkClient;
   
   //==============================================================================
   class HttpAudioPlayer : public juce::ChangeListener
   {
   public:
       HttpAudioPlayer();
       ~HttpAudioPlayer() override;
   
       //==============================================================================
       // Transport Controls
   
       void loadAndPlay(const juce::String& postId, const juce::String& audioUrl);
   
       void play();
   
       void pause();
   
       void stop();
   
       void togglePlayPause();
   
       void seekToPosition(double positionSeconds);
   
       void seekToNormalizedPosition(double normalizedPosition);
   
       //==============================================================================
       // State Queries
   
       bool isPlaying() const { return playing; }
       bool isLoading() const { return loading; }
   
       double getPositionSeconds() const;
   
       double getDurationSeconds() const;
   
       double getPlaybackProgress() const;
   
       juce::String getCurrentPostId() const { return currentPostId; }
   
       bool isPostPlaying(const juce::String& postId) const;
   
       //==============================================================================
       // Volume Control
   
       void setVolume(float newVolume);
   
       float getVolume() const { return volume; }
   
       void setMuted(bool shouldMute);
       bool isMuted() const { return muted; }
   
       //==============================================================================
       // Auto-play and Queue Management
   
       void setAutoPlayEnabled(bool enabled) { autoPlayEnabled = enabled; }
       bool isAutoPlayEnabled() const { return autoPlayEnabled; }
   
       void setPlaylist(const juce::StringArray& postIds, const juce::StringArray& audioUrls);
   
       int getCurrentPlaylistIndex() const;
   
       void playNext();
   
       void playPrevious();
   
       //==============================================================================
       // Audio Focus (DAW awareness)
   
       void onDAWTransportStarted();
   
       void onDAWTransportStopped();
   
       void setAudioFocusEnabled(bool enabled) { audioFocusEnabled = enabled; }
       bool isAudioFocusEnabled() const { return audioFocusEnabled; }
   
       //==============================================================================
       // Audio Processing (called from PluginProcessor::processBlock)
   
       void processBlock(juce::AudioBuffer<float>& buffer, int numSamples);
   
       void prepareToPlay(double sampleRate, int blockSize);
   
       void releaseResources();
   
       //==============================================================================
       // Callbacks
   
       std::function<void(const juce::String& postId)> onPlaybackStarted;
   
       std::function<void(const juce::String& postId)> onPlaybackPaused;
   
       std::function<void(const juce::String& postId)> onPlaybackStopped;
   
       std::function<void(const juce::String& postId)> onLoadingStarted;
   
       std::function<void(const juce::String& postId, bool success)> onLoadingComplete;
   
       std::function<void(const juce::String& postId, double progress)> onProgressUpdate;
   
       std::function<void(const juce::String& postId)> onPlaybackFinished;
   
       std::function<void(const juce::String& postId)> onAutoPlayNext;
   
       //==============================================================================
       // Cache Management
   
       void clearCache();
   
       void setMaxCacheSize(size_t bytes);
   
       size_t getCurrentCacheSize() const;
   
       void preloadAudio(const juce::String& postId, const juce::String& audioUrl);
   
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
           ProgressTimer(HttpAudioPlayer& p) : player(p) {}
           void timerCallback() override;
       private:
           HttpAudioPlayer& player;
       };
   
       std::unique_ptr<ProgressTimer> progressTimer;
   
       JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HttpAudioPlayer)
   };
