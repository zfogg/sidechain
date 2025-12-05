#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "util/Log.h"

//==============================================================================
SidechainAudioProcessor::SidechainAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
    // Simple initialization for now
    Log::info("SidechainAudioProcessor: Plugin initialized");
}

SidechainAudioProcessor::~SidechainAudioProcessor()
{
    Log::debug("SidechainAudioProcessor: Destroying");
}

//==============================================================================
const juce::String SidechainAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool SidechainAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool SidechainAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool SidechainAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double SidechainAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int SidechainAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int SidechainAudioProcessor::getCurrentProgram()
{
    return 0;
}

void SidechainAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String SidechainAudioProcessor::getProgramName (int index)
{
    return {};
}

void SidechainAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void SidechainAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBlockSize = samplesPerBlock;

    // Prepare audio capture with current settings
    int numChannels = getTotalNumInputChannels();
    audioCapture.prepare(sampleRate, samplesPerBlock, numChannels);

    // Prepare audio player for feed playback
    audioPlayer.prepareToPlay(sampleRate, samplesPerBlock);

    Log::info("SidechainAudioProcessor: Prepared - " + juce::String(sampleRate) + "Hz, " +
        juce::String(samplesPerBlock) + " samples, " +
        juce::String(numChannels) + " channels");
}

void SidechainAudioProcessor::releaseResources()
{
    Log::debug("SidechainAudioProcessor: Releasing resources");
    audioPlayer.releaseResources();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool SidechainAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void SidechainAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Clear any output channels that don't contain input data
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // Read transport info from DAW via AudioPlayHead
    if (auto* playHead = getPlayHead())
    {
        if (auto position = playHead->getPosition())
        {
            // Get BPM
            if (auto bpm = position->getBpm())
            {
                currentBPM.store(*bpm);
                bpmAvailable.store(true);
            }
            else
            {
                bpmAvailable.store(false);
            }

            // Detect DAW transport state changes for audio focus
            bool isDAWPlaying = position->getIsPlaying();
            bool wasDAWPlaying = dawTransportPlaying.load();

            if (isDAWPlaying != wasDAWPlaying)
            {
                dawTransportPlaying.store(isDAWPlaying);
                Log::debug("SidechainAudioProcessor: DAW transport state changed - playing: " + juce::String(isDAWPlaying ? "true" : "false"));

                // Notify audio player on message thread (not audio thread)
                juce::MessageManager::callAsync([this, isDAWPlaying]()
                {
                    if (isDAWPlaying)
                        audioPlayer.onDAWTransportStarted();
                    else
                        audioPlayer.onDAWTransportStopped();
                });
            }
        }
    }

    // Capture audio for recording (lock-free, called on audio thread)
    // This captures the incoming audio before any processing
    audioCapture.captureAudio(buffer);

    // Mix in feed audio playback (adds to the output buffer)
    // This allows users to hear posts while working in their DAW
    audioPlayer.processBlock(buffer, buffer.getNumSamples());

    juce::ignoreUnused(midiMessages);
}

//==============================================================================
bool SidechainAudioProcessor::hasEditor() const
{
    return true; // We want a UI for the social feed
}

juce::AudioProcessorEditor* SidechainAudioProcessor::createEditor()
{
    Log::info("SidechainAudioProcessor: Creating editor");
    return new SidechainAudioProcessorEditor (*this);
}

//==============================================================================
void SidechainAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // Save plugin state (simplified)
    Log::debug("SidechainAudioProcessor: Saving state");
    auto state = juce::ValueTree("SidechainState");
    state.setProperty("authenticated", authenticated, nullptr);
    
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void SidechainAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // Restore plugin state (simplified)
    Log::debug("SidechainAudioProcessor: Restoring state");
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        auto state = juce::ValueTree::fromXml (*xml);
        if (state.isValid())
        {
            authenticated = state.getProperty("authenticated", false);
            Log::debug("SidechainAudioProcessor: State restored - authenticated: " + juce::String(authenticated ? "true" : "false"));
        }
        else
        {
            Log::warn("SidechainAudioProcessor: Invalid state data");
        }
    }
    else
    {
        Log::warn("SidechainAudioProcessor: Failed to parse state data");
    }
}

//==============================================================================
// Audio Recording API
//==============================================================================
void SidechainAudioProcessor::startRecording()
{
    // Generate a unique recording ID
    juce::String recordingId = juce::Uuid().toString();
    audioCapture.startRecording(recordingId);
    Log::info("SidechainAudioProcessor: Started recording - ID: " + recordingId);
}

void SidechainAudioProcessor::stopRecording()
{
    lastRecordedAudio = audioCapture.stopRecording();
    double duration = static_cast<double>(lastRecordedAudio.getNumSamples()) / currentSampleRate;
    Log::info("SidechainAudioProcessor: Stopped recording - " + 
              juce::String(lastRecordedAudio.getNumSamples()) + " samples, " +
              juce::String(duration, 2) + " seconds");
}

juce::AudioBuffer<float> SidechainAudioProcessor::getRecordedAudio()
{
    return lastRecordedAudio;
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SidechainAudioProcessor();
}