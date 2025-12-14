#pragma once

#include <JuceHeader.h>
#include <memory>

class NetworkClient;

//==============================================================================
/**
 * WaveformImageView - Displays a waveform from a URL (PNG/SVG)
 *
 * This component downloads and caches waveform images from the backend.
 * Used in feed posts, stories, and upload previews.
 */
class WaveformImageView : public juce::Component
{
public:
    WaveformImageView();
    ~WaveformImageView() override;

    //==========================================================================
    // Image Loading

    /**
     * Load waveform from URL
     * @param url URL to waveform image (PNG or SVG)
     */
    void loadFromUrl(const juce::String& url);

    /**
     * Clear the current waveform
     */
    void clear();

    /**
     * Check if waveform is loaded
     */
    bool isLoaded() const { return waveformImage.isValid(); }

    /**
     * Check if waveform is currently loading
     */
    bool isLoading() const { return loading; }

    //==========================================================================
    // Network Client

    /**
     * Set the network client for downloading images
     */
    void setNetworkClient(NetworkClient* client) { networkClient = client; }

    //==========================================================================
    // Styling

    /**
     * Set background color
     */
    void setBackgroundColour(juce::Colour colour) { backgroundColour = colour; }

    /**
     * Set whether to show loading indicator
     */
    void setShowLoadingIndicator(bool show) { showLoadingIndicator = show; }

    //==========================================================================
    // Component

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    //==========================================================================
    // Image data
    juce::Image waveformImage;
    juce::String currentUrl;

    //==========================================================================
    // State
    bool loading = false;
    bool showLoadingIndicator = true;

    //==========================================================================
    // Styling
    juce::Colour backgroundColour { 0xff26262c };

    //==========================================================================
    // Network
    NetworkClient* networkClient = nullptr;

    //==========================================================================
    // Callbacks
    void onImageDownloaded(const juce::String& url, juce::MemoryBlock imageData);
    void onImageDownloadFailed(const juce::String& url);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveformImageView)
};
