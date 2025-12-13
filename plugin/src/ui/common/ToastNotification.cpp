#include "ToastNotification.h"
#include "../../util/Colors.h"

//==============================================================================
// ToastNotification Implementation
//==============================================================================

ToastNotification::ToastNotification(const juce::String& msg, ToastType type, int durationMs)
    : message(msg), toastType(type), duration(durationMs)
{
    createTime = juce::Time::currentTimeMillis();
    startTimer(50); // Update timer for animations and auto-dismiss
}

ToastNotification::~ToastNotification()
{
    stopTimer();
}

void ToastNotification::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Apply dismiss animation (fade out and slide)
    float alpha = dismissing ? (1.0f - dismissProgress) : 1.0f;
    float slideOffset = dismissing ? (dismissProgress * 20.0f) : 0.0f;

    bounds = bounds.translated(0, -slideOffset);

    // Background with rounded corners
    g.setColour(getBackgroundColor().withAlpha(alpha * 0.95f));
    g.fillRoundedRectangle(bounds, 8.0f);

    // Border
    g.setColour(getIconColor().withAlpha(alpha * 0.5f));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 8.0f, 1.0f);

    // Icon on left
    auto iconBounds = bounds.removeFromLeft(40.0f);
    g.setColour(getIconColor().withAlpha(alpha));
    g.setFont(juce::Font(juce::FontOptions().withHeight(18.0f)));
    g.drawText(getIcon(), iconBounds.toNearestInt(), juce::Justification::centred);

    // Message text
    auto textBounds = bounds.reduced(8.0f, 0.0f);
    g.setColour(SidechainColors::textPrimary().withAlpha(alpha));
    g.setFont(juce::Font(juce::FontOptions().withHeight(13.0f)));
    g.drawFittedText(message, textBounds.toNearestInt(), juce::Justification::centredLeft, 2);

    // Progress bar at bottom (shows time remaining)
    if (!dismissing && duration > 0)
    {
        int64_t elapsed = juce::Time::currentTimeMillis() - createTime;
        float progress = juce::jmin(1.0f, static_cast<float>(elapsed) / static_cast<float>(duration));
        float barWidth = getWidth() * (1.0f - progress);

        g.setColour(getIconColor().withAlpha(alpha * 0.3f));
        g.fillRect(0.0f, static_cast<float>(getHeight() - 3), barWidth, 3.0f);
    }
}

void ToastNotification::mouseUp(const juce::MouseEvent& /*event*/)
{
    dismiss();
}

void ToastNotification::timerCallback()
{
    if (dismissing)
    {
        dismissProgress += 0.15f;
        if (dismissProgress >= 1.0f)
        {
            stopTimer();
            if (onDismiss)
                onDismiss(this);
        }
        repaint();
    }
    else
    {
        // Check if duration has elapsed
        int64_t elapsed = juce::Time::currentTimeMillis() - createTime;
        if (duration > 0 && elapsed >= duration)
        {
            dismiss();
        }
        repaint(); // Update progress bar
    }
}

int ToastNotification::getRemainingTime() const
{
    if (dismissing || duration <= 0)
        return 0;

    int64_t elapsed = juce::Time::currentTimeMillis() - createTime;
    return juce::jmax(0, duration - static_cast<int>(elapsed));
}

void ToastNotification::dismiss()
{
    if (!dismissing)
    {
        dismissing = true;
        dismissProgress = 0.0f;
    }
}

juce::Colour ToastNotification::getBackgroundColor() const
{
    switch (toastType)
    {
        case ToastType::Success:
            return SidechainColors::success().darker(0.7f);
        case ToastType::Warning:
            return SidechainColors::warning().darker(0.7f);
        case ToastType::Error:
            return SidechainColors::error().darker(0.5f);
        case ToastType::Info:
        default:
            return SidechainColors::backgroundLight();
    }
}

juce::Colour ToastNotification::getIconColor() const
{
    switch (toastType)
    {
        case ToastType::Success:
            return SidechainColors::success();
        case ToastType::Warning:
            return SidechainColors::warning();
        case ToastType::Error:
            return SidechainColors::error();
        case ToastType::Info:
        default:
            return SidechainColors::skyBlue();
    }
}

juce::String ToastNotification::getIcon() const
{
    switch (toastType)
    {
        case ToastType::Success:
            return juce::String(juce::CharPointer_UTF8("\xE2\x9C\x93")); // Checkmark
        case ToastType::Warning:
            return juce::String(juce::CharPointer_UTF8("\xE2\x9A\xA0")); // Warning
        case ToastType::Error:
            return juce::String(juce::CharPointer_UTF8("\xE2\x9C\x97")); // X mark
        case ToastType::Info:
        default:
            return juce::String(juce::CharPointer_UTF8("\xE2\x84\xB9")); // Info
    }
}

//==============================================================================
// ToastManager Implementation
//==============================================================================

ToastManager& ToastManager::getInstance()
{
    static ToastManager instance;
    return instance;
}

ToastManager::ToastManager()
{
    setInterceptsMouseClicks(false, true); // Pass through clicks except to children
    startTimer(100); // Periodic cleanup
}

ToastManager::~ToastManager()
{
    stopTimer();
    toasts.clear();
}

void ToastManager::paint(juce::Graphics& /*g*/)
{
    // Background is transparent - toasts draw themselves
}

void ToastManager::resized()
{
    updateLayout();
}

void ToastManager::timerCallback()
{
    // Remove any toasts that have finished dismissing
    toasts.erase(
        std::remove_if(toasts.begin(), toasts.end(),
            [](const std::unique_ptr<ToastNotification>& t) {
                return t->isDismissing() && t->getRemainingTime() <= 0;
            }),
        toasts.end());

    // If we have too many toasts, dismiss oldest ones
    while (static_cast<int>(toasts.size()) > maxVisibleToasts)
    {
        if (!toasts.empty())
            toasts.front()->dismiss();
    }
}

void ToastManager::showInfo(const juce::String& message, int durationMs)
{
    showToast(message, ToastNotification::ToastType::Info, durationMs);
}

void ToastManager::showSuccess(const juce::String& message, int durationMs)
{
    showToast(message, ToastNotification::ToastType::Success, durationMs);
}

void ToastManager::showWarning(const juce::String& message, int durationMs)
{
    showToast(message, ToastNotification::ToastType::Warning, durationMs);
}

void ToastManager::showError(const juce::String& message, int durationMs)
{
    showToast(message, ToastNotification::ToastType::Error, durationMs);
}

void ToastManager::showToast(const juce::String& message, ToastNotification::ToastType type, int durationMs)
{
    auto toast = std::make_unique<ToastNotification>(message, type, durationMs);
    addToast(std::move(toast));
}

void ToastManager::clearAll()
{
    for (auto& toast : toasts)
    {
        toast->dismiss();
    }
}

void ToastManager::addToast(std::unique_ptr<ToastNotification> toast)
{
    toast->onDismiss = [this](ToastNotification* t) {
        removeToast(t);
    };

    addAndMakeVisible(toast.get());
    toasts.push_back(std::move(toast));
    updateLayout();
}

void ToastManager::removeToast(ToastNotification* toast)
{
    toasts.erase(
        std::remove_if(toasts.begin(), toasts.end(),
            [toast](const std::unique_ptr<ToastNotification>& t) {
                return t.get() == toast;
            }),
        toasts.end());
    updateLayout();
}

void ToastManager::updateLayout()
{
    auto bounds = getLocalBounds();
    int toastX = bounds.getCentreX() - TOAST_WIDTH / 2;
    int currentY = positionFromTop ? edgeMargin : (bounds.getHeight() - edgeMargin - TOAST_HEIGHT);
    int direction = positionFromTop ? 1 : -1;

    for (auto& toast : toasts)
    {
        toast->setBounds(toastX, currentY, TOAST_WIDTH, TOAST_HEIGHT);
        currentY += direction * (TOAST_HEIGHT + TOAST_SPACING);
    }
}
