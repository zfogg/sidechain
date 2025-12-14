#include "WaveformImageView.h"
#include "../../network/NetworkClient.h"
#include "../../util/Log.h"
#include "../../util/Colors.h"

WaveformImageView::WaveformImageView()
{
}

WaveformImageView::~WaveformImageView()
{
}

//==============================================================================
void WaveformImageView::loadFromUrl(const juce::String& url)
{
    if (url.isEmpty())
    {
        clear();
        return;
    }

    // Don't reload if already loaded
    if (currentUrl == url && waveformImage.isValid())
    {
        return;
    }

    currentUrl = url;
    loading = true;
    waveformImage = juce::Image();
    repaint();

    Log::debug("WaveformImageView: Loading waveform from " + url);

    if (networkClient == nullptr)
    {
        Log::error("WaveformImageView: NetworkClient not set");
        loading = false;
        repaint();
        return;
    }

    // Download image asynchronously
    networkClient->getBinaryAbsolute(url, [this, url](Outcome<juce::MemoryBlock> result) {
        if (result.isOk() && result.getValue().getSize() > 0)
        {
            onImageDownloaded(url, result.getValue());
        }
        else
        {
            onImageDownloadFailed(url);
        }
    });
}

void WaveformImageView::clear()
{
    currentUrl = "";
    waveformImage = juce::Image();
    loading = false;
    repaint();
}

//==============================================================================
void WaveformImageView::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    // Background
    g.fillAll(backgroundColour);

    if (loading && showLoadingIndicator)
    {
        // Loading indicator
        g.setColour(SidechainColors::textSecondary());
        g.setFont(12.0f);
        g.drawText("Loading waveform...", bounds, juce::Justification::centred);
    }
    else if (waveformImage.isValid())
    {
        // Draw waveform image (scaled to fit)
        g.drawImage(waveformImage, bounds.toFloat(),
                    juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize);
    }
    else if (!currentUrl.isEmpty())
    {
        // Failed to load
        g.setColour(SidechainColors::textMuted());
        g.setFont(10.0f);
        g.drawText("Waveform unavailable", bounds, juce::Justification::centred);
    }
}

void WaveformImageView::resized()
{
    // Nothing to resize
}

//==============================================================================
void WaveformImageView::onImageDownloaded(const juce::String& url, juce::MemoryBlock imageData)
{
    // Only process if URL matches current request (avoid race conditions)
    if (url != currentUrl)
        return;

    Log::debug("WaveformImageView: Downloaded waveform image - " + juce::String((int)imageData.getSize()) + " bytes");

    // Decode PNG image
    waveformImage = juce::ImageFileFormat::loadFrom(imageData.getData(), imageData.getSize());

    loading = false;

    if (waveformImage.isValid())
    {
        Log::info("WaveformImageView: Successfully loaded waveform - " +
                  juce::String(waveformImage.getWidth()) + "x" + juce::String(waveformImage.getHeight()));
    }
    else
    {
        Log::error("WaveformImageView: Failed to decode waveform image");
    }

    repaint();
}

void WaveformImageView::onImageDownloadFailed(const juce::String& url)
{
    // Only process if URL matches current request
    if (url != currentUrl)
        return;

    Log::error("WaveformImageView: Failed to download waveform from " + url);

    loading = false;
    repaint();
}
