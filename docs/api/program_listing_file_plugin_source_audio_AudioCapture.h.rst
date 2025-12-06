
.. _program_listing_file_plugin_source_audio_AudioCapture.h:

Program Listing for File AudioCapture.h
=======================================

|exhale_lsh| :ref:`Return to documentation for file <file_plugin_source_audio_AudioCapture.h>` (``plugin/source/audio/AudioCapture.h``)

.. |exhale_lsh| unicode:: U+021B0 .. UPWARDS ARROW WITH TIP LEFTWARDS

.. code-block:: cpp

   #pragma once
   
   #include <JuceHeader.h>
   
   //==============================================================================
   class AudioCapture
   {
   public:
       AudioCapture();
       ~AudioCapture();
   
       //==============================================================================
       // Configuration - call from prepareToPlay() or message thread
       void prepare(double sampleRate, int samplesPerBlock, int numChannels);
       void reset();
   
       //==============================================================================
       // Recording control - call from MESSAGE THREAD only
       void startRecording(const juce::String& recordingId = "");
       juce::AudioBuffer<float> stopRecording();
       bool isRecording() const { return recording.load(); }
   
       //==============================================================================
       // Audio capture - call from AUDIO THREAD (processBlock) only
       // MUST be lock-free and allocation-free
       void captureAudio(const juce::AudioBuffer<float>& buffer);
   
       //==============================================================================
       // Recording info - thread-safe reads
       double getRecordingLengthSeconds() const;
       int getRecordingLengthSamples() const { return recordingPosition.load(); }
       int getMaxRecordingSamples() const { return maxRecordingSamples; }
       double getMaxRecordingLengthSeconds() const;
       float getRecordingProgress() const; // 0.0 - 1.0
       bool isBufferFull() const;
   
       //==============================================================================
       // Level metering - thread-safe, updated during captureAudio()
       float getPeakLevel(int channel) const;
       float getRMSLevel(int channel) const;
       void resetLevels();
   
       //==============================================================================
       // Export utilities - call from MESSAGE THREAD
       juce::String generateWaveformSVG(const juce::AudioBuffer<float>& buffer, int width = 400, int height = 100);
   
       // Get sample rate for export
       double getSampleRate() const { return currentSampleRate; }
       int getNumChannels() const { return currentNumChannels; }
   
       //==============================================================================
       // Audio file export - call from MESSAGE THREAD
   
       enum class ExportFormat
       {
           WAV_16bit,   
           WAV_24bit,   
           WAV_32bit,   
           FLAC_16bit,  
           FLAC_24bit   
       };
   
       static bool saveBufferToFile(const juce::File& file,
                                    const juce::AudioBuffer<float>& buffer,
                                    double sampleRate,
                                    ExportFormat format = ExportFormat::WAV_16bit);
   
       static bool saveBufferToWavFile(const juce::File& file,
                                       const juce::AudioBuffer<float>& buffer,
                                       double sampleRate,
                                       ExportFormat format = ExportFormat::WAV_16bit);
   
       static bool saveBufferToFlacFile(const juce::File& file,
                                        const juce::AudioBuffer<float>& buffer,
                                        double sampleRate,
                                        ExportFormat format = ExportFormat::FLAC_16bit,
                                        int quality = 5);
   
       bool saveRecordedAudioToFile(const juce::File& file,
                                    ExportFormat format = ExportFormat::WAV_16bit) const;
   
       // Legacy method - kept for backward compatibility
       bool saveRecordedAudioToWavFile(const juce::File& file,
                                       ExportFormat format = ExportFormat::WAV_16bit) const;
   
       static bool isFlacFormat(ExportFormat format);
   
       static juce::String getExtensionForFormat(ExportFormat format);
   
       static juce::File getTempAudioFile(const juce::String& extension = ".wav");
   
       bool hasRecordedAudio() const { return hasRecordedData && recordedAudio.getNumSamples() > 0; }
   
       const juce::AudioBuffer<float>& getRecordedAudioBuffer() const { return recordedAudio; }
   
       //==============================================================================
       // Duration and size utilities
   
       static juce::String formatDuration(double seconds);
   
       static juce::String formatDurationWithMs(double seconds);
   
       static juce::String formatFileSize(juce::int64 bytes);
   
       static juce::int64 estimateFileSize(int numSamples, int numChannels, ExportFormat format);
   
       juce::int64 getEstimatedFileSize(ExportFormat format) const;
   
       //==============================================================================
       // Audio processing utilities - all return NEW buffers (non-destructive)
   
       static juce::AudioBuffer<float> trimBuffer(const juce::AudioBuffer<float>& buffer,
                                                   int startSample,
                                                   int endSample = -1);
   
       static juce::AudioBuffer<float> trimBufferByTime(const juce::AudioBuffer<float>& buffer,
                                                         double sampleRate,
                                                         double startSeconds,
                                                         double endSeconds = -1.0);
   
       enum class FadeType
       {
           Linear,      
           Exponential, 
           SCurve       
       };
   
       static void applyFadeIn(juce::AudioBuffer<float>& buffer,
                               int fadeSamples,
                               FadeType fadeType = FadeType::Linear);
   
       static void applyFadeOut(juce::AudioBuffer<float>& buffer,
                                int fadeSamples,
                                FadeType fadeType = FadeType::Linear);
   
       static void applyFadeInByTime(juce::AudioBuffer<float>& buffer,
                                      double sampleRate,
                                      double fadeSeconds,
                                      FadeType fadeType = FadeType::Linear);
   
       static void applyFadeOutByTime(juce::AudioBuffer<float>& buffer,
                                       double sampleRate,
                                       double fadeSeconds,
                                       FadeType fadeType = FadeType::Linear);
   
       static float getBufferPeakLevel(const juce::AudioBuffer<float>& buffer);
   
       static float getBufferPeakLevelDB(const juce::AudioBuffer<float>& buffer);
   
       static float normalizeBuffer(juce::AudioBuffer<float>& buffer,
                                     float targetPeakDB = -1.0f);
   
       static float dbToLinear(float db);
   
       static float linearToDb(float linear);
   
   private:
       //==============================================================================
       // Thread-safe state (accessed from both threads)
       std::atomic<bool> recording { false };
       std::atomic<int> recordingPosition { 0 };
   
       // Level metering (written on audio thread, read on message thread)
       static constexpr int MaxChannels = 2;
       std::atomic<float> peakLevels[MaxChannels] { {0.0f}, {0.0f} };
       std::atomic<float> rmsLevels[MaxChannels] { {0.0f}, {0.0f} };
   
       // RMS calculation state (audio thread only)
       float rmsSums[MaxChannels] = { 0.0f, 0.0f };
       int rmsSampleCount = 0;
       static constexpr int RMSWindowSamples = 2048; // ~46ms @ 44.1kHz
   
       //==============================================================================
       // Configuration (set on message thread before recording)
       juce::String currentRecordingId;
       double currentSampleRate = 44100.0;
       int currentNumChannels = 2;
       int maxRecordingSamples = 0; // 60 seconds max
   
       //==============================================================================
       // Recording buffer (allocated on message thread, written on audio thread)
       juce::AudioBuffer<float> recordingBuffer;
   
       // Recorded data (message thread only)
       juce::AudioBuffer<float> recordedAudio;
       bool hasRecordedData = false;
   
       //==============================================================================
       void initializeBuffers();
       void updateLevels(const juce::AudioBuffer<float>& buffer, int numSamples);
   
       JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioCapture)
   };
