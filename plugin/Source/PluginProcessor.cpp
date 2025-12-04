#include "PluginProcessor.h"
#include "PluginEditor.h"

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
    DBG("Sidechain plugin initialized");
}

SidechainAudioProcessor::~SidechainAudioProcessor()
{
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

    DBG("Sidechain prepared: " + juce::String(sampleRate) + "Hz, " +
        juce::String(samplesPerBlock) + " samples, " +
        juce::String(numChannels) + " channels");
}

void SidechainAudioProcessor::releaseResources()
{
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

    // Read BPM from DAW via AudioPlayHead (lock-free atomic store)
    if (auto* playHead = getPlayHead())
    {
        if (auto position = playHead->getPosition())
        {
            if (auto bpm = position->getBpm())
            {
                currentBPM.store(*bpm);
                bpmAvailable.store(true);
            }
            else
            {
                bpmAvailable.store(false);
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
    return new SidechainAudioProcessorEditor (*this);
}

//==============================================================================
void SidechainAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // Save plugin state (simplified)
    auto state = juce::ValueTree("SidechainState");
    state.setProperty("authenticated", authenticated, nullptr);
    
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void SidechainAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // Restore plugin state (simplified)
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        auto state = juce::ValueTree::fromXml (*xml);
        if (state.isValid())
        {
            authenticated = state.getProperty("authenticated", false);
        }
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
    DBG("Started recording: " + recordingId);
}

void SidechainAudioProcessor::stopRecording()
{
    lastRecordedAudio = audioCapture.stopRecording();
    DBG("Stopped recording: " + juce::String(lastRecordedAudio.getNumSamples()) + " samples");
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