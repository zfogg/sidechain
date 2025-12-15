#pragma once

#include <JuceHeader.h>
#include <vector>
#include <memory>

//==============================================================================
/**
 * ToastNotification - A transient notification that auto-dismisses
 *
 * Individual toast component that displays a message and can be clicked to dismiss.
 */
class ToastNotification : public juce::Component,
                          public juce::Timer
{
public:
    //==========================================================================
    enum class ToastType
    {
        Info,       // Blue - General information
        Success,    // Green - Operation succeeded
        Warning,    // Yellow - Warning message
        Error       // Red - Operation failed
    };

    //==========================================================================
    ToastNotification(const juce::String& message, ToastType type = ToastType::Info,
                      int durationMs = 3000);
    ~ToastNotification() override;

    //==========================================================================
    void paint(juce::Graphics& g) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void timerCallback() override;

    //==========================================================================
    /** Callback when toast should be removed */
    std::function<void(ToastNotification*)> onDismiss;

    /** Get the toast type */
    ToastType getType() const { return toastType; }

    /** Get remaining time before auto-dismiss (0 if already dismissing) */
    int getRemainingTime() const;

    /** Manually dismiss this toast */
    void dismiss();

    /** Check if toast is currently being dismissed (animating out) */
    bool isDismissing() const { return dismissing; }

private:
    juce::String message;
    ToastType toastType;
    int duration;
    int64_t createTime;
    bool dismissing = false;
    float dismissProgress = 0.0f;

    juce::Colour getBackgroundColor() const;
    juce::Colour getIconColor() const;
    juce::String getIcon() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ToastNotification)
};

//==============================================================================
/**
 * ToastManager - Manages a stack of toast notifications
 *
 * Usage:
 *   // Get the manager instance
 *   auto& toasts = ToastManager::getInstance();
 *
 *   // Show a toast
 *   toasts.showError("Failed to like post");
 *   toasts.showSuccess("Post uploaded!");
 *   toasts.showInfo("New message received");
 *
 *   // In your main component's resized():
 *   toasts.setBounds(getLocalBounds());
 */
class ToastManager : public juce::Component,
                     public juce::Timer,
                     public juce::DeletedAtShutdown
{
public:
    //==========================================================================
    /** Get the singleton instance */
    static ToastManager& getInstance();

    //==========================================================================
    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

    //==========================================================================
    // Show toast methods

    /** Show an info toast */
    void showInfo(const juce::String& message, int durationMs = 3000);

    /** Show a success toast */
    void showSuccess(const juce::String& message, int durationMs = 3000);

    /** Show a warning toast */
    void showWarning(const juce::String& message, int durationMs = 4000);

    /** Show an error toast */
    void showError(const juce::String& message, int durationMs = 3000);

    /** Show a custom toast */
    void showToast(const juce::String& message, ToastNotification::ToastType type, int durationMs);

    //==========================================================================
    /** Clear all toasts immediately */
    void clearAll();

    /** Get number of active toasts */
    int getToastCount() const { return static_cast<int>(toasts.size()); }

    //==========================================================================
    // Configuration

    /** Set maximum number of visible toasts (oldest dismissed first) */
    void setMaxVisibleToasts(int max) { maxVisibleToasts = max; }

    /** Set position (from top or bottom of parent) */
    void setPosition(bool fromTop) { positionFromTop = fromTop; updateLayout(); }

    /** Set margin from edge */
    void setMargin(int margin) { edgeMargin = margin; updateLayout(); }

private:
    //==========================================================================
    ToastManager();
    ~ToastManager() override;

    std::vector<std::unique_ptr<ToastNotification>> toasts;
    int maxVisibleToasts = 5;
    bool positionFromTop = true;
    int edgeMargin = 20;

    static constexpr int TOAST_HEIGHT = 50;
    static constexpr int TOAST_WIDTH = 320;
    static constexpr int TOAST_SPACING = 8;

    void addToast(std::unique_ptr<ToastNotification> toast);
    void removeToast(ToastNotification* toast);
    void updateLayout();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ToastManager)
};
