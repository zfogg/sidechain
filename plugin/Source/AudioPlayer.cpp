#include "AudioPlayer.h"

//==============================================================================
AudioPlayer::AudioPlayer()
{
    // Register common audio formats
    formatManager.registerBasicFormats();

    // Create progress timer
    progressTimer = std::make_unique<ProgressTimer>(*this);
}

AudioPlayer::~AudioPlayer()
{
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
        if (playing)
            pause();
        else
            play();
        return;
    }

    // Stop current playback
    stop();

    currentPostId = postId;
    currentAudioUrl = audioUrl;

    // Check if we have this audio cached
    if (auto* cachedData = getFromCache(postId))
    {
        loadFromMemory(postId, *cachedData);
        play();
        return;
    }

    // Download the audio
    loading = true;
    if (onLoadingStarted)
        onLoadingStarted(postId);

    downloadAudio(postId, audioUrl);
}

void AudioPlayer::play()
{
    if (readerSource == nullptr)
        return;

    playing = true;

    // Start progress timer
    progressTimer->startTimer(50); // Update every 50ms

    if (onPlaybackStarted)
        onPlaybackStarted(currentPostId);
}

void AudioPlayer::pause()
{
    playing = false;
    progressTimer->stopTimer();

    if (onPlaybackPaused)
        onPlaybackPaused(currentPostId);
}

void AudioPlayer::stop()
{
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
            // Schedule stop on message thread
            juce::MessageManager::callAsync([this]()
            {
                stop();
            });
        }
    }
}

void AudioPlayer::prepareToPlay(double sampleRate, int blockSize)
{
    currentSampleRate = sampleRate;
    currentBlockSize = blockSize;

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
    audioCache.clear();
    currentCacheSize = 0;
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
    juce::Thread::launch([this, postId, audioUrl]()
    {
        juce::URL url(audioUrl);
        auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
            .withConnectionTimeoutMs(10000)
            .withNumRedirectsToFollow(5);

        if (auto stream = url.createInputStream(options))
        {
            auto data = std::make_unique<juce::MemoryBlock>();
            stream->readIntoMemoryBlock(*data);

            if (data->getSize() > 0)
            {
                addToCache(postId, std::move(data));
            }
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
    juce::Thread::launch([this, postId, url]()
    {
        juce::URL audioUrl(url);
        auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
            .withConnectionTimeoutMs(30000)
            .withNumRedirectsToFollow(5);

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
                // Add to cache
                addToCache(postId, std::make_unique<juce::MemoryBlock>(*data));

                // Load and play
                loadFromMemory(postId, *data);
                play();
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
        DBG("AudioPlayer: Failed to create reader for audio data");
        return;
    }

    // Create reader source
    readerSource = std::make_unique<juce::AudioFormatReaderSource>(reader, true);

    // Create resampling source to match DAW sample rate
    resamplingSource = std::make_unique<juce::ResamplingAudioSource>(readerSource.get(), false, 2);
    resamplingSource->setResamplingRatio(reader->sampleRate / currentSampleRate);
    resamplingSource->prepareToPlay(currentBlockSize, currentSampleRate);

    DBG("AudioPlayer: Loaded audio - Duration: " << (reader->lengthInSamples / reader->sampleRate) << "s");
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
