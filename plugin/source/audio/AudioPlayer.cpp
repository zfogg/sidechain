#include "AudioPlayer.h"
#include "../network/NetworkClient.h"
#include "../util/Constants.h"
#include "../util/Async.h"
#include "../util/Log.h"
#include <memory>

//==============================================================================
AudioPlayer::AudioPlayer()
{
    // Register common audio formats
    formatManager.registerBasicFormats();

    // Create progress timer
    progressTimer = std::make_unique<AudioPlayer::ProgressTimer>(*this);
    
    Log::info("AudioPlayer: Initialized");
}

AudioPlayer::~AudioPlayer()
{
    Log::debug("AudioPlayer: Destroying");
    progressTimer->stopTimer();
    stop();
}

//==============================================================================
// Transport Controls

void AudioPlayer::loadAndPlay(const juce::String& postId, const juce::String& audioUrl)
{
    // If same post is already playing, just toggle play/pause
    if (postId == currentPostId && readerSource != nullptr)
    {
        Log::debug("AudioPlayer: Toggling play/pause for post: " + postId);
        if (playing)
            pause();
        else
            play();
        return;
    }

    Log::info("AudioPlayer: Loading and playing post: " + postId);

    // Stop current playback
    stop();

    currentPostId = postId;
    currentAudioUrl = audioUrl;

    // Check if we have this audio cached
    if (auto* cachedData = getFromCache(postId))
    {
        Log::debug("AudioPlayer: Using cached audio for post: " + postId);
        loadFromMemory(postId, *cachedData);
        play();
        return;
    }

    // Download the audio
    loading = true;
    Log::info("AudioPlayer: Downloading audio for post: " + postId);
    if (onLoadingStarted)
        onLoadingStarted(postId);

    downloadAudio(postId, audioUrl);
}

void AudioPlayer::play()
{
    if (readerSource == nullptr)
    {
        Log::warn("AudioPlayer: Cannot play - no audio source loaded");
        return;
    }

    playing = true;
    Log::info("AudioPlayer: Playback started - post: " + currentPostId);

    // Start progress timer
    progressTimer->startTimer(50); // Update every 50ms

    if (onPlaybackStarted)
        onPlaybackStarted(currentPostId);
}

void AudioPlayer::pause()
{
    playing = false;
    progressTimer->stopTimer();
    Log::debug("AudioPlayer: Playback paused - post: " + currentPostId);

    if (onPlaybackPaused)
        onPlaybackPaused(currentPostId);
}

void AudioPlayer::stop()
{
    if (playing || !currentPostId.isEmpty())
    {
        Log::info("AudioPlayer: Playback stopped - post: " + currentPostId);
    }

    playing = false;
    progressTimer->stopTimer();

    {
        const juce::ScopedLock sl(audioLock);

        if (resamplingSource)
            resamplingSource->releaseResources();

        resamplingSource.reset();
        readerSource.reset();
    }

    if (!currentPostId.isEmpty() && onPlaybackStopped)
        onPlaybackStopped(currentPostId);

    currentPostId = "";
    currentAudioUrl = "";
}

void AudioPlayer::togglePlayPause()
{
    if (playing)
        pause();
    else
        play();
}

void AudioPlayer::seekToPosition(double positionSeconds)
{
    const juce::ScopedLock sl(audioLock);

    if (readerSource == nullptr)
        return;

    auto* reader = readerSource->getAudioFormatReader();
    if (reader == nullptr)
        return;

    juce::int64 samplePosition = static_cast<juce::int64>(positionSeconds * reader->sampleRate);
    samplePosition = juce::jlimit<juce::int64>(0, reader->lengthInSamples, samplePosition);

    readerSource->setNextReadPosition(samplePosition);
}

void AudioPlayer::seekToNormalizedPosition(double normalizedPosition)
{
    double duration = getDurationSeconds();
    if (duration > 0)
        seekToPosition(normalizedPosition * duration);
}

//==============================================================================
// State Queries

double AudioPlayer::getPositionSeconds() const
{
    const juce::ScopedLock sl(audioLock);

    if (readerSource == nullptr)
        return 0.0;

    auto* reader = readerSource->getAudioFormatReader();
    if (reader == nullptr || reader->sampleRate <= 0)
        return 0.0;

    return static_cast<double>(readerSource->getNextReadPosition()) / reader->sampleRate;
}

double AudioPlayer::getDurationSeconds() const
{
    const juce::ScopedLock sl(audioLock);

    if (readerSource == nullptr)
        return 0.0;

    auto* reader = readerSource->getAudioFormatReader();
    if (reader == nullptr || reader->sampleRate <= 0)
        return 0.0;

    return static_cast<double>(reader->lengthInSamples) / reader->sampleRate;
}

double AudioPlayer::getPlaybackProgress() const
{
    double duration = getDurationSeconds();
    if (duration <= 0)
        return 0.0;

    return getPositionSeconds() / duration;
}

bool AudioPlayer::isPostPlaying(const juce::String& postId) const
{
    return playing && currentPostId == postId;
}

//==============================================================================
// Volume Control

void AudioPlayer::setVolume(float newVolume)
{
    volume = juce::jlimit(0.0f, 1.0f, newVolume);
}

void AudioPlayer::setMuted(bool shouldMute)
{
    muted = shouldMute;
}

//==============================================================================
// Audio Processing

void AudioPlayer::processBlock(juce::AudioBuffer<float>& buffer, int numSamples)
{
    if (!playing || muted)
        return;

    const juce::ScopedLock sl(audioLock);

    if (resamplingSource == nullptr)
        return;

    // Create temp buffer for playback audio
    juce::AudioSourceChannelInfo info;
    juce::AudioBuffer<float> tempBuffer(buffer.getNumChannels(), numSamples);
    tempBuffer.clear();
    info.buffer = &tempBuffer;
    info.startSample = 0;
    info.numSamples = numSamples;

    // Get audio from the source
    resamplingSource->getNextAudioBlock(info);

    // Apply volume and mix into output buffer
    float vol = volume.load();
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        buffer.addFrom(channel, 0, tempBuffer, channel % tempBuffer.getNumChannels(), 0, numSamples, vol);
    }

    // Check if playback has ended
    if (readerSource != nullptr)
    {
        auto* reader = readerSource->getAudioFormatReader();
        if (reader != nullptr && readerSource->getNextReadPosition() >= reader->lengthInSamples)
        {
                // Schedule end-of-playback handling on message thread
                juce::MessageManager::callAsync([this]()
                {
                    juce::String finishedPostId = currentPostId;
                    Log::info("AudioPlayer: Playback finished - post: " + finishedPostId);

                    // Notify that playback finished
                    if (onPlaybackFinished)
                        onPlaybackFinished(finishedPostId);

                    // Handle auto-play
                    if (autoPlayEnabled)
                    {
                        int nextIndex = getCurrentPlaylistIndex() + 1;
                        const juce::ScopedLock sl(playlistLock);
                        if (nextIndex >= 0 && nextIndex < playlistPostIds.size())
                        {
                            // Play next post
                            juce::String nextPostId = playlistPostIds[nextIndex];
                            juce::String nextUrl = playlistAudioUrls[nextIndex];

                            Log::debug("AudioPlayer: Auto-playing next post: " + nextPostId);

                            if (onAutoPlayNext)
                                onAutoPlayNext(nextPostId);

                            loadAndPlay(nextPostId, nextUrl);
                            return;
                        }
                        else
                        {
                            Log::debug("AudioPlayer: End of playlist reached");
                        }
                    }

                    // No auto-play or end of playlist - just stop
                    stop();
                });
        }
    }
}

void AudioPlayer::prepareToPlay(double sampleRate, int blockSize)
{
    currentSampleRate = sampleRate;
    currentBlockSize = blockSize;
    Log::info("AudioPlayer: Prepared - " + juce::String(sampleRate) + "Hz, block size: " + juce::String(blockSize));

    const juce::ScopedLock sl(audioLock);

    if (resamplingSource)
        resamplingSource->prepareToPlay(blockSize, sampleRate);
}

void AudioPlayer::releaseResources()
{
    const juce::ScopedLock sl(audioLock);

    if (resamplingSource)
        resamplingSource->releaseResources();
}

//==============================================================================
// Cache Management

void AudioPlayer::clearCache()
{
    const juce::ScopedLock sl(cacheLock);
    size_t oldSize = currentCacheSize;
    audioCache.clear();
    currentCacheSize = 0;
    Log::info("AudioPlayer: Cache cleared - freed " + juce::String((int)oldSize) + " bytes");
}

void AudioPlayer::setMaxCacheSize(size_t bytes)
{
    maxCacheSize = bytes;
    evictCacheIfNeeded(0);
}

size_t AudioPlayer::getCurrentCacheSize() const
{
    return currentCacheSize;
}

void AudioPlayer::preloadAudio(const juce::String& postId, const juce::String& audioUrl)
{
    // Don't preload if already cached
    if (getFromCache(postId) != nullptr)
        return;

    // Download in background
    Async::runVoid([this, postId, audioUrl]()
    {
        juce::MemoryBlock data;
        bool success = false;

        // Use NetworkClient if available, otherwise fall back to JUCE URL
        if (networkClient != nullptr)
        {
            auto result = networkClient->makeAbsoluteRequestSync(audioUrl, "GET", juce::var(), false, juce::StringPairArray(), &data);
            success = result.success && data.getSize() > 0;
        }
        else
        {
            // Fallback to JUCE URL
            juce::URL url(audioUrl);
            auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                .withConnectionTimeoutMs(Constants::Api::IMAGE_TIMEOUT_MS)
                .withNumRedirectsToFollow(Constants::Api::MAX_REDIRECTS);

            if (auto stream = url.createInputStream(options))
            {
                stream->readIntoMemoryBlock(data);
                success = data.getSize() > 0;
            }
        }

        if (success)
        {
            auto dataPtr = std::make_unique<juce::MemoryBlock>(data);
            addToCache(postId, std::move(dataPtr));
        }
    });
}

void AudioPlayer::evictCacheIfNeeded(size_t bytesNeeded)
{
    const juce::ScopedLock sl(cacheLock);

    while (currentCacheSize + bytesNeeded > maxCacheSize && !audioCache.empty())
    {
        // Find oldest entry (LRU eviction)
        juce::String oldestKey;
        juce::int64 oldestTime = std::numeric_limits<juce::int64>::max();

        for (const auto& [key, cached] : audioCache)
        {
            if (cached.lastAccessTime < oldestTime && key != currentPostId)
            {
                oldestTime = cached.lastAccessTime;
                oldestKey = key;
            }
        }

        if (oldestKey.isEmpty())
            break;

        currentCacheSize -= audioCache[oldestKey].sizeBytes;
        audioCache.erase(oldestKey);
    }
}

void AudioPlayer::addToCache(const juce::String& postId, std::unique_ptr<juce::MemoryBlock> data)
{
    const juce::ScopedLock sl(cacheLock);

    size_t dataSize = data->getSize();
    evictCacheIfNeeded(dataSize);

    CachedAudio cached;
    cached.audioData = std::move(data);
    cached.sizeBytes = dataSize;
    cached.lastAccessTime = juce::Time::currentTimeMillis();

    audioCache[postId] = std::move(cached);
    currentCacheSize += dataSize;
}

juce::MemoryBlock* AudioPlayer::getFromCache(const juce::String& postId)
{
    const juce::ScopedLock sl(cacheLock);

    auto it = audioCache.find(postId);
    if (it != audioCache.end())
    {
        it->second.lastAccessTime = juce::Time::currentTimeMillis();
        return it->second.audioData.get();
    }
    return nullptr;
}

//==============================================================================
// Loading

void AudioPlayer::downloadAudio(const juce::String& postId, const juce::String& url)
{
    Log::debug("AudioPlayer: Starting download - post: " + postId + ", url: " + url);
    
    Async::runVoid([this, postId, url]()
    {
        juce::URL audioUrl(url);
        auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
            .withConnectionTimeoutMs(Constants::Api::DEFAULT_TIMEOUT_MS)
            .withNumRedirectsToFollow(Constants::Api::MAX_REDIRECTS);

        bool success = false;
        auto data = std::make_unique<juce::MemoryBlock>();

        if (auto stream = audioUrl.createInputStream(options))
        {
            stream->readIntoMemoryBlock(*data);
            success = data->getSize() > 0;
        }

        // Back to message thread
        juce::MessageManager::callAsync([this, postId, data = std::move(data), success]() mutable
        {
            loading = false;

            if (success && postId == currentPostId)
            {
                Log::info("AudioPlayer: Download successful - post: " + postId + ", size: " + juce::String((int)data->getSize()) + " bytes");
                
                // Add to cache
                addToCache(postId, std::make_unique<juce::MemoryBlock>(*data));

                // Load and play
                loadFromMemory(postId, *data);
                play();
            }
            else if (!success)
            {
                Log::error("AudioPlayer: Download failed - post: " + postId);
            }
            else
            {
                Log::warn("AudioPlayer: Download completed but post changed - post: " + postId + ", current: " + currentPostId);
            }

            if (onLoadingComplete)
                onLoadingComplete(postId, success);
        });
    });
}

void AudioPlayer::loadFromMemory(const juce::String& postId, juce::MemoryBlock& audioData)
{
    const juce::ScopedLock sl(audioLock);

    // Create memory input stream
    auto* memStream = new juce::MemoryInputStream(audioData, false);

    // Create audio format reader
    auto* reader = formatManager.createReaderFor(std::unique_ptr<juce::InputStream>(memStream));
    if (reader == nullptr)
    {
        Log::error("AudioPlayer: Failed to create reader for audio data - post: " + postId);
        return;
    }

    // Create reader source
    readerSource = std::make_unique<juce::AudioFormatReaderSource>(reader, true);

    // Create resampling source to match DAW sample rate
    resamplingSource = std::make_unique<juce::ResamplingAudioSource>(readerSource.get(), false, 2);
    resamplingSource->setResamplingRatio(reader->sampleRate / currentSampleRate);
    resamplingSource->prepareToPlay(currentBlockSize, currentSampleRate);

    double duration = static_cast<double>(reader->lengthInSamples) / reader->sampleRate;
    Log::info("AudioPlayer: Loaded audio from memory - post: " + postId + 
              ", duration: " + juce::String(duration, 2) + "s, " +
              "sample rate: " + juce::String(reader->sampleRate) + "Hz, " +
              "channels: " + juce::String(reader->numChannels));
}

//==============================================================================
// Playlist and Auto-play

void AudioPlayer::setPlaylist(const juce::StringArray& postIds, const juce::StringArray& audioUrls)
{
    const juce::ScopedLock sl(playlistLock);
    playlistPostIds = postIds;
    playlistAudioUrls = audioUrls;

    // Pre-buffer next post if we're currently playing
    if (playing && !currentPostId.isEmpty())
    {
        int currentIndex = getCurrentPlaylistIndex();
        if (currentIndex >= 0 && currentIndex + 1 < playlistPostIds.size())
        {
            preloadAudio(playlistPostIds[currentIndex + 1], playlistAudioUrls[currentIndex + 1]);
        }
    }
}

int AudioPlayer::getCurrentPlaylistIndex() const
{
    const juce::ScopedLock sl(playlistLock);
    return playlistPostIds.indexOf(currentPostId);
}

void AudioPlayer::playNext()
{
    int currentIndex = getCurrentPlaylistIndex();
    const juce::ScopedLock sl(playlistLock);

    if (currentIndex >= 0 && currentIndex + 1 < playlistPostIds.size())
    {
        loadAndPlay(playlistPostIds[currentIndex + 1], playlistAudioUrls[currentIndex + 1]);
    }
}

void AudioPlayer::playPrevious()
{
    // If we're more than 3 seconds in, restart current track
    if (getPositionSeconds() > 3.0)
    {
        seekToPosition(0.0);
        return;
    }

    int currentIndex = getCurrentPlaylistIndex();
    const juce::ScopedLock sl(playlistLock);

    if (currentIndex > 0)
    {
        loadAndPlay(playlistPostIds[currentIndex - 1], playlistAudioUrls[currentIndex - 1]);
    }
    else
    {
        // At start of playlist, just restart
        seekToPosition(0.0);
    }
}

//==============================================================================
// Audio Focus (DAW awareness)

void AudioPlayer::onDAWTransportStarted()
{
    if (!audioFocusEnabled)
        return;

    if (playing)
    {
        wasPlayingBeforeDAW = true;
        pausedByDAW = true;
        pause();
        Log::info("AudioPlayer: Paused due to DAW transport start");
    }
}

void AudioPlayer::onDAWTransportStopped()
{
    if (!audioFocusEnabled)
        return;

    if (pausedByDAW && wasPlayingBeforeDAW)
    {
        pausedByDAW = false;
        wasPlayingBeforeDAW = false;
        play();
        Log::info("AudioPlayer: Resumed after DAW transport stop");
    }
}

//==============================================================================
// ChangeListener

void AudioPlayer::changeListenerCallback(juce::ChangeBroadcaster* /*source*/)
{
    // Not currently used, but available for future extensions
}

//==============================================================================
// Progress Timer

void AudioPlayer::ProgressTimer::timerCallback()
{
    if (player.playing && player.onProgressUpdate)
    {
        player.onProgressUpdate(player.currentPostId, player.getPlaybackProgress());
    }
}
