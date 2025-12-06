
.. _program_listing_file_plugin_source_audio_BufferAudioPlayer.h:

Program Listing for File BufferAudioPlayer.h
============================================

|exhale_lsh| :ref:`Return to documentation for file <file_plugin_source_audio_BufferAudioPlayer.h>` (``plugin/source/audio/BufferAudioPlayer.h``)

.. |exhale_lsh| unicode:: U+021B0 .. UPWARDS ARROW WITH TIP LEFTWARDS

.. code-block:: cpp

   #pragma once
   
   #include <JuceHeader.h>
   #include <functional>
   #include <memory>
   
   //==============================================================================
   class BufferAudioPlayer
   {
   public:
       BufferAudioPlayer();
       ~BufferAudioPlayer();
   
       //==============================================================================
       // Loading
   
       void loadBuffer(const juce::AudioBuffer<float>& buffer, double sampleRate);
   
       void clear();
   
       //==============================================================================
       // Transport Controls
   
       void play();
   
       void pause();
   
       void stop();
   
       void togglePlayPause();
   
       void seekToPosition(double positionSeconds);
   
       void seekToNormalizedPosition(double normalizedPosition);
   
       //==============================================================================
       // State Queries
   
       bool isPlaying() const { return playing; }
       bool hasBuffer() const;
   
       double getPositionSeconds() const;
   
       double getDurationSeconds() const;
   
       double getPlaybackProgress() const;
   
       //==============================================================================
       // Volume Control
   
       void setVolume(float newVolume);
   
       float getVolume() const { return volume; }
   
       void setMuted(bool shouldMute);
       bool isMuted() const { return muted; }
   
       //==============================================================================
       // Audio Processing (called from PluginProcessor::processBlock)
   
       void processBlock(juce::AudioBuffer<float>& buffer, int numSamples);
   
       void prepareToPlay(double sampleRate, int blockSize);
   
       void releaseResources();
   
       //==============================================================================
       // Callbacks
   
       std::function<void()> onPlaybackStarted;
   
       std::function<void()> onPlaybackPaused;
   
       std::function<void()> onPlaybackStopped;
   
       std::function<void(double progress)> onProgressUpdate;
   
       std::function<void()> onPlaybackFinished;
   
   private:
       //==============================================================================
       // Audio data
       juce::AudioBuffer<float> audioBuffer;
       double bufferSampleRate = 44100.0;
       int numChannels = 0;
       int totalSamples = 0;
   
       // Playback state
       std::atomic<bool> playing { false };
       std::atomic<bool> muted { false };
       std::atomic<float> volume { 0.8f };
   
       // Position tracking (in samples at buffer sample rate)
       std::atomic<juce::int64> currentSamplePosition { 0 };
   
       // Resampling state
       double outputSampleRate = 44100.0;
       double resamplingRatio = 1.0;
       juce::LagrangeInterpolator interpolator;
   
       // Thread safety
       juce::CriticalSection bufferLock;
   
       // Progress timer
       class ProgressTimer : public juce::Timer
       {
       public:
           ProgressTimer(BufferAudioPlayer& p) : player(p) {}
           void timerCallback() override;
       private:
           BufferAudioPlayer& player;
       };
   
       std::unique_ptr<ProgressTimer> progressTimer;
   
       // Helper methods
       double samplePositionToSeconds(juce::int64 samplePos) const;
       juce::int64 secondsToSamplePosition(double seconds) const;
   
       JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BufferAudioPlayer)
   };
