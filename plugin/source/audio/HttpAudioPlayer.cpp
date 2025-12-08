#include "HttpAudioPlayer.h"
#include "../network/NetworkClient.h"
#include "../util/Constants.h"
#include "../util/Async.h"
#include "../util/Log.h"
#include <memory>

//==============================================================================
HttpAudioPlayer::HttpAudioPlayer()
{
    // Register common audio formats
    formatManager.registerBasicFormats();

    // Create progress timer
    progressTimer = std::make_unique<HttpAudioPlayer::ProgressTimer>(*this);

    Log::info("HttpAudioPlayer: Initialized");
}

HttpAudioPlayer::~HttpAudioPlayer()
{
    Log::debug("HttpAudioPlayer: Destroying");
    progressTimer->stopTimer();
    stop();
}

//==============================================================================
// Transport Controls

void HttpAudioPlayer::loadAndPlay(const juce::String& postId, const juce::String& audioUrl)
{
    // If same post is already playing, just toggle play/pause
    if (postId == currentPostId && readerSource != nullptr)
    {
        Log::debug("HttpAudioPlayer: Toggling play/pause for post: " + postId);
        if (playing)
            pause();
        else
            play();
        return;
    }

    Log::info("HttpAudioPlayer: Loading and playing post: " + postId);

    // Stop current playback
    stop();

    currentPostId = postId;
    currentAudioUrl = audioUrl;

    // Check if we have this audio cached
    if (auto* cachedData = getFromCache(postId))
    {
        Log::debug("HttpAudioPlayer: Using cached audio for post: " + postId);
        loadFromMemory(postId, *cachedData);
        play();
        return;
    }

    // Download the audio
    loading = true;
    Log::info("HttpAudioPlayer: Downloading audio for post: " + postId);
    if (onLoadingStarted)
        onLoadingStarted(postId);

    downloadAudio(postId, audioUrl);
}

void HttpAudioPlayer::play()
{
    if (readerSource == nullptr)
    {
        Log::warn("HttpAudioPlayer: Cannot play - no audio source loaded");
        return;
    }

    playing = true;
    Log::info("HttpAudioPlayer: Playback started - post: " + currentPostId);

    // Start progress timer
    progressTimer->startTimer(50); // Update every 50ms

    if (onPlaybackStarted)
        onPlaybackStarted(currentPostId);
}

void HttpAudioPlayer::pause()
{
    playing = false;
    progressTimer->stopTimer();
    Log::debug("HttpAudioPlayer: Playback paused - post: " + currentPostId);

    if (onPlaybackPaused)
        onPlaybackPaused(currentPostId);
}

void HttpAudioPlayer::stop()
{
    if (playing || !currentPostId.isEmpty())
    {
        Log::info("HttpAudioPlayer: Playback stopped - post: " + currentPostId);
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

void HttpAudioPlayer::togglePlayPause()
{
    if (playing)
        pause();
    else
        play();
}

void HttpAudioPlayer::seekToPosition(double positionSeconds)
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

void HttpAudioPlayer::seekToNormalizedPosition(double normalizedPosition)
{
    double duration = getDurationSeconds();
    if (duration > 0)
        seekToPosition(normalizedPosition * duration);
}

//==============================================================================
// State Queries

double HttpAudioPlayer::getPositionSeconds() const
{
    const juce::ScopedLock sl(audioLock);

    if (readerSource == nullptr)
        return 0.0;

    auto* reader = readerSource->getAudioFormatReader();
    if (reader == nullptr || reader->sampleRate <= 0)
        return 0.0;

    return static_cast<double>(readerSource->getNextReadPosition()) / reader->sampleRate;
}

double HttpAudioPlayer::getDurationSeconds() const
{
    const juce::ScopedLock sl(audioLock);

    if (readerSource == nullptr)
        return 0.0;

    auto* reader = readerSource->getAudioFormatReader();
    if (reader == nullptr || reader->sampleRate <= 0)
        return 0.0;

    return static_cast<double>(reader->lengthInSamples) / reader->sampleRate;
}

double HttpAudioPlayer::getPlaybackProgress() const
{
    double duration = getDurationSeconds();
    if (duration <= 0)
        return 0.0;

    return getPositionSeconds() / duration;
}

bool HttpAudioPlayer::isPostPlaying(const juce::String& postId) const
{
    return playing && currentPostId == postId;
}

//==============================================================================
// Volume Control

void HttpAudioPlayer::setVolume(float newVolume)
{
    volume = juce::jlimit(0.0f, 1.0f, newVolume);
}

void HttpAudioPlayer::setMuted(bool shouldMute)
{
    muted = shouldMute;
}

//==============================================================================
// Audio Processing

void HttpAudioPlayer::processBlock(juce::AudioBuffer<float>& buffer, int numSamples)
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
                    Log::info("HttpAudioPlayer: Playback finished - post: " + finishedPostId);

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

                            Log::debug("HttpAudioPlayer: Auto-playing next post: " + nextPostId);

                            if (onAutoPlayNext)
                                onAutoPlayNext(nextPostId);

                            loadAndPlay(nextPostId, nextUrl);
                            
                            // Pre-buffer the post after the next one for seamless playback
                            if (nextIndex + 1 < playlistPostIds.size())
                            {
                                preloadAudio(playlistPostIds[nextIndex + 1], playlistAudioUrls[nextIndex + 1]);
                            }
                            return;
                        }
                        else
                        {
                            Log::debug("HttpAudioPlayer: End of playlist reached");
                        }
                    }

                    // No auto-play or end of playlist - just stop
                    stop();
                });
        }
    }
}

void HttpAudioPlayer::prepareToPlay(double sampleRate, int blockSize)
{
    currentSampleRate = sampleRate;
    currentBlockSize = blockSize;
    Log::info("HttpAudioPlayer: Prepared - " + juce::String(sampleRate) + "Hz, block size: " + juce::String(blockSize));

    const juce::ScopedLock sl(audioLock);

    if (resamplingSource)
        resamplingSource->prepareToPlay(blockSize, sampleRate);
}

void HttpAudioPlayer::releaseResources()
{
    const juce::ScopedLock sl(audioLock);

    if (resamplingSource)
        resamplingSource->releaseResources();
}

//==============================================================================
// Cache Management

void HttpAudioPlayer::clearCache()
{
    const juce::ScopedLock sl(cacheLock);
    size_t oldSize = currentCacheSize;
    audioCache.clear();
    currentCacheSize = 0;
    Log::info("HttpAudioPlayer: Cache cleared - freed " + juce::String((int)oldSize) + " bytes");
}

void HttpAudioPlayer::setMaxCacheSize(size_t bytes)
{
    maxCacheSize = bytes;
    evictCacheIfNeeded(0);
}

size_t HttpAudioPlayer::getCurrentCacheSize() const
{
    return currentCacheSize;
}

void HttpAudioPlayer::preloadAudio(const juce::String& postId, const juce::String& audioUrl)
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

void HttpAudioPlayer::evictCacheIfNeeded(size_t bytesNeeded)
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

void HttpAudioPlayer::addToCache(const juce::String& postId, std::unique_ptr<juce::MemoryBlock> data)
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

juce::MemoryBlock* HttpAudioPlayer::getFromCache(const juce::String& postId)
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

void HttpAudioPlayer::downloadAudio(const juce::String& postId, const juce::String& url)
{
    Log::debug("HttpAudioPlayer: Starting download - post: " + postId + ", url: " + url);

    Async::runVoid([this, postId, url]()
    {
        bool success = false;
        auto data = std::make_unique<juce::MemoryBlock>();

        // Use NetworkClient if available - it handles HTTPS properly on Linux
        if (networkClient != nullptr)
        {
            Log::debug("HttpAudioPlayer: Using NetworkClient for download");
            auto result = networkClient->makeAbsoluteRequestSync(url, "GET", juce::var(), false, juce::StringPairArray(), data.get());
            success = result.success && data->getSize() > 0;
            if (!success)
            {
                Log::warn("HttpAudioPlayer: NetworkClient download failed - " + result.errorMessage);
            }
        }
        else
        {
            // Fallback to JUCE URL (may fail on Linux with HTTPS)
            Log::debug("HttpAudioPlayer: Using JUCE URL for download (no NetworkClient)");
            juce::URL audioUrl(url);
            auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                .withConnectionTimeoutMs(Constants::Api::DEFAULT_TIMEOUT_MS)
                .withNumRedirectsToFollow(Constants::Api::MAX_REDIRECTS);

            if (auto stream = audioUrl.createInputStream(options))
            {
                stream->readIntoMemoryBlock(*data);
                success = data->getSize() > 0;
            }
        }

        // Back to message thread
        juce::MessageManager::callAsync([this, postId, data = std::move(data), success]() mutable
        {
            loading = false;

            if (success && postId == currentPostId)
            {
                Log::info("HttpAudioPlayer: Download successful - post: " + postId + ", size: " + juce::String((int)data->getSize()) + " bytes");

                // Add to cache
                addToCache(postId, std::make_unique<juce::MemoryBlock>(*data));

                // Load and play
                loadFromMemory(postId, *data);
                play();
            }
            else if (!success)
            {
                Log::error("HttpAudioPlayer: Download failed - post: " + postId);
            }
            else
            {
                Log::warn("HttpAudioPlayer: Download completed but post changed - post: " + postId + ", current: " + currentPostId);
            }

            if (onLoadingComplete)
                onLoadingComplete(postId, success);
        });
    });
}

void HttpAudioPlayer::loadFromMemory(const juce::String& postId, juce::MemoryBlock& audioData)
{
    const juce::ScopedLock sl(audioLock);

    // Create memory input stream
    auto* memStream = new juce::MemoryInputStream(audioData, false);

    // Create audio format reader
    auto* reader = formatManager.createReaderFor(std::unique_ptr<juce::InputStream>(memStream));
    if (reader == nullptr)
    {
        Log::error("HttpAudioPlayer: Failed to create reader for audio data - post: " + postId);
        return;
    }

    // Create reader source
    readerSource = std::make_unique<juce::AudioFormatReaderSource>(reader, true);

    // Create resampling source to match DAW sample rate
    resamplingSource = std::make_unique<juce::ResamplingAudioSource>(readerSource.get(), false, 2);
    resamplingSource->setResamplingRatio(reader->sampleRate / currentSampleRate);
    resamplingSource->prepareToPlay(currentBlockSize, currentSampleRate);

    double duration = static_cast<double>(reader->lengthInSamples) / reader->sampleRate;
    Log::info("HttpAudioPlayer: Loaded audio from memory - post: " + postId +
              ", duration: " + juce::String(duration, 2) + "s, " +
              "sample rate: " + juce::String(reader->sampleRate) + "Hz, " +
              "channels: " + juce::String(reader->numChannels));
}

//==============================================================================
// Playlist and Auto-play

void HttpAudioPlayer::setPlaylist(const juce::StringArray& postIds, const juce::StringArray& audioUrls)
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

int HttpAudioPlayer::getCurrentPlaylistIndex() const
{
    const juce::ScopedLock sl(playlistLock);
    return playlistPostIds.indexOf(currentPostId);
}

void HttpAudioPlayer::playNext()
{
    int currentIndex = getCurrentPlaylistIndex();
    const juce::ScopedLock sl(playlistLock);

    if (currentIndex >= 0 && currentIndex + 1 < playlistPostIds.size())
    {
        int nextIndex = currentIndex + 1;
        loadAndPlay(playlistPostIds[nextIndex], playlistAudioUrls[nextIndex]);
        
        // Pre-buffer the post after the next one for seamless playback
        if (nextIndex + 1 < playlistPostIds.size())
        {
            preloadAudio(playlistPostIds[nextIndex + 1], playlistAudioUrls[nextIndex + 1]);
        }
    }
}

void HttpAudioPlayer::playPrevious()
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

void HttpAudioPlayer::onDAWTransportStarted()
{
    if (!audioFocusEnabled)
        return;

    if (playing)
    {
        wasPlayingBeforeDAW = true;
        pausedByDAW = true;
        pause();
        Log::info("HttpAudioPlayer: Paused due to DAW transport start");
    }
}

void HttpAudioPlayer::onDAWTransportStopped()
{
    if (!audioFocusEnabled)
        return;

    if (pausedByDAW && wasPlayingBeforeDAW)
    {
        pausedByDAW = false;
        wasPlayingBeforeDAW = false;
        play();
        Log::info("HttpAudioPlayer: Resumed after DAW transport stop");
    }
}

//==============================================================================
// ChangeListener

void HttpAudioPlayer::changeListenerCallback(juce::ChangeBroadcaster* /*source*/)
{
    // Not currently used, but available for future extensions
}

//==============================================================================
// Progress Timer

void HttpAudioPlayer::ProgressTimer::timerCallback()
{
    if (player.playing && player.onProgressUpdate)
    {
        player.onProgressUpdate(player.currentPostId, player.getPlaybackProgress());
    }
}
