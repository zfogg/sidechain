#pragma once

#include <JuceHeader.h>

// ==============================================================================
/**
 * ErrorState - A reusable component for displaying error states with retry
 *
 * Features:
 * - Different error types (network, auth, generic, empty)
 * - Icon + message + description layout
 * - Primary action button (e.g., "Retry", "Sign In")
 * - Optional secondary action
 * - Consistent styling across the app
 */
class ErrorState : public juce::Component {
public:
  // ==========================================================================
  // Error types with different icons and default messages
  enum class ErrorType {
    // Network & Server errors
    Network,     // Connection issues, can't reach server
    Timeout,     // Request timed out
    Offline,     // Device is offline
    ServerError, // Server errors (500+)
    RateLimit,   // Too many requests (429)

    // Auth & Permission errors
    Auth,       // Authentication/authorization errors (401, 403)
    Permission, // Permission denied (microphone, storage, etc.)

    // Resource errors
    NotFound, // Resource not found (404)
    Empty,    // Empty state (not really an error, but similar UI)

    // Data errors
    Validation, // Form/input validation errors
    Parsing,    // JSON parsing, unexpected data format

    // Media errors
    Upload, // File upload failed
    Audio,  // Audio playback/processing errors

    // Catch-all
    Generic // Generic/unknown errors
  };

  // ==========================================================================
  ErrorState();
  ~ErrorState() override = default;

  // ==========================================================================
  void paint(juce::Graphics &g) override;
  void resized() override;
  void mouseUp(const juce::MouseEvent &event) override;

  // ==========================================================================
  // Configuration

  /** Set the error type (changes icon and default message) */
  void setErrorType(ErrorType type);

  /** Get the current error type */
  ErrorType getErrorType() const {
    return errorType;
  }

  /** Detect error type from an error message string */
  static ErrorType detectErrorType(const juce::String &errorMessage);

  /** Configure this error state from an error message (detects type and sets
   * message) */
  void configureFromError(const juce::String &errorMessage);

  /** Set custom title (overrides default for error type) */
  void setTitle(const juce::String &title);

  /** Set custom message/description */
  void setMessage(const juce::String &message);

  /** Set primary action button text and callback */
  void setPrimaryAction(const juce::String &buttonText, std::function<void()> callback);

  /** Set secondary action button text and callback */
  void setSecondaryAction(const juce::String &buttonText, std::function<void()> callback);

  /** Clear secondary action */
  void clearSecondaryAction();

  /** Show/hide the icon */
  void setShowIcon(bool show) {
    showIcon = show;
    repaint();
  }

  /** Set compact mode (smaller layout for inline use) */
  void setCompact(bool compact) {
    compactMode = compact;
    repaint();
  }

  // ==========================================================================
  // Convenience factory methods

  /** Create a network error state with retry */
  static std::unique_ptr<ErrorState> networkError(std::function<void()> onRetry);

  /** Create an auth error state */
  static std::unique_ptr<ErrorState> authError(std::function<void()> onSignIn);

  /** Create a not found error state */
  static std::unique_ptr<ErrorState> notFoundError(const juce::String &resourceName = "content");

  /** Create a generic error state with retry */
  static std::unique_ptr<ErrorState> genericError(const juce::String &message, std::function<void()> onRetry);

  /** Create an empty state (for lists with no content) */
  static std::unique_ptr<ErrorState> emptyState(const juce::String &title, const juce::String &message,
                                                const juce::String &actionText = "",
                                                std::function<void()> onAction = nullptr);

  /** Create a timeout error state with retry */
  static std::unique_ptr<ErrorState> timeoutError(std::function<void()> onRetry);

  /** Create an offline error state */
  static std::unique_ptr<ErrorState> offlineError(std::function<void()> onRetry);

  /** Create a rate limit error state */
  static std::unique_ptr<ErrorState> rateLimitError(std::function<void()> onRetry);

  /** Create a permission error state */
  static std::unique_ptr<ErrorState> permissionError(const juce::String &permissionName,
                                                     std::function<void()> onSettings = nullptr);

  /** Create a validation error state */
  static std::unique_ptr<ErrorState> validationError(const juce::String &message);

  /** Create a parsing error state (unexpected data format) */
  static std::unique_ptr<ErrorState> parsingError(const juce::String &context, std::function<void()> onRetry = nullptr);

  /** Create an upload error state */
  static std::unique_ptr<ErrorState> uploadError(const juce::String &message, std::function<void()> onRetry);

  /** Create an audio error state */
  static std::unique_ptr<ErrorState> audioError(const juce::String &message, std::function<void()> onRetry = nullptr);

private:
  // ==========================================================================
  ErrorType errorType = ErrorType::Generic;
  juce::String title;
  juce::String message;

  bool showIcon = true;
  bool compactMode = false;

  // Primary action (e.g., "Retry")
  juce::String primaryButtonText;
  std::function<void()> onPrimaryAction;

  // Secondary action (e.g., "Go Back")
  juce::String secondaryButtonText;
  std::function<void()> onSecondaryAction;

  // ==========================================================================
  // Drawing helpers
  void drawIcon(juce::Graphics &g, juce::Rectangle<int> bounds);
  juce::String getIconForType() const;
  juce::String getDefaultTitleForType() const;
  juce::String getDefaultMessageForType() const;
  juce::Colour getIconColorForType() const;

  // Button bounds
  juce::Rectangle<int> getPrimaryButtonBounds() const;
  juce::Rectangle<int> getSecondaryButtonBounds() const;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ErrorState)
};
