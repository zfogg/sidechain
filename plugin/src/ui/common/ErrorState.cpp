#include "ErrorState.h"
#include "../../util/Colors.h"

//==============================================================================
ErrorState::ErrorState()
{
    setErrorType(ErrorType::Generic);
}

//==============================================================================
void ErrorState::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    if (compactMode)
    {
        // Compact layout: horizontal with smaller elements
        bounds = bounds.reduced(10);

        int iconSize = 24;
        int spacing = 10;

        // Icon on left
        if (showIcon)
        {
            auto iconBounds = bounds.removeFromLeft(iconSize);
            drawIcon(g, iconBounds);
            bounds.removeFromLeft(spacing);
        }

        // Text content
        auto textBounds = bounds;
        if (onPrimaryAction)
            textBounds = textBounds.withTrimmedRight(80); // Leave space for button

        // Title
        g.setColour(SidechainColors::textPrimary());
        g.setFont(juce::Font(juce::FontOptions().withHeight(14.0f).withStyle("Bold")));
        juce::String displayTitle = title.isEmpty() ? getDefaultTitleForType() : title;
        g.drawText(displayTitle, textBounds, juce::Justification::centredLeft);

        // Primary button on right
        if (onPrimaryAction && primaryButtonText.isNotEmpty())
        {
            auto buttonBounds = getPrimaryButtonBounds();
            g.setColour(SidechainColors::primary());
            g.fillRoundedRectangle(buttonBounds.toFloat(), 4.0f);
            g.setColour(SidechainColors::textPrimary());
            g.setFont(juce::Font(juce::FontOptions().withHeight(12.0f).withStyle("Bold")));
            g.drawText(primaryButtonText, buttonBounds, juce::Justification::centred);
        }
    }
    else
    {
        // Full layout: centered, vertical
        int iconSize = 48;
        int spacing = 16;
        int buttonHeight = 40;
        int buttonWidth = 140;

        // Calculate content height
        int contentHeight = 0;
        if (showIcon) contentHeight += iconSize + spacing;
        contentHeight += 24; // Title
        contentHeight += spacing / 2;
        contentHeight += 40; // Message (approx 2 lines)
        if (onPrimaryAction) contentHeight += spacing + buttonHeight;
        if (onSecondaryAction) contentHeight += spacing / 2 + buttonHeight;

        // Center content vertically
        int startY = (bounds.getHeight() - contentHeight) / 2;
        int centerX = bounds.getCentreX();

        // Icon
        if (showIcon)
        {
            auto iconBounds = juce::Rectangle<int>(centerX - iconSize / 2, startY, iconSize, iconSize);
            drawIcon(g, iconBounds);
            startY += iconSize + spacing;
        }

        // Title
        juce::String displayTitle = title.isEmpty() ? getDefaultTitleForType() : title;
        g.setColour(SidechainColors::textPrimary());
        g.setFont(juce::Font(juce::FontOptions().withHeight(18.0f).withStyle("Bold")));
        g.drawText(displayTitle, bounds.withY(startY).withHeight(24), juce::Justification::centredTop);
        startY += 24 + spacing / 2;

        // Message
        juce::String displayMessage = message.isEmpty() ? getDefaultMessageForType() : message;
        g.setColour(SidechainColors::textSecondary());
        g.setFont(juce::Font(juce::FontOptions().withHeight(14.0f)));
        auto messageBounds = bounds.withY(startY).withHeight(40).reduced(20, 0);
        g.drawFittedText(displayMessage, messageBounds, juce::Justification::centredTop, 2);
        startY += 40 + spacing;

        // Primary button
        if (onPrimaryAction && primaryButtonText.isNotEmpty())
        {
            auto buttonBounds = getPrimaryButtonBounds();
            g.setColour(SidechainColors::primary());
            g.fillRoundedRectangle(buttonBounds.toFloat(), 8.0f);
            g.setColour(SidechainColors::textPrimary());
            g.setFont(juce::Font(juce::FontOptions().withHeight(14.0f).withStyle("Bold")));
            g.drawText(primaryButtonText, buttonBounds, juce::Justification::centred);
            startY += buttonHeight + spacing / 2;
        }

        // Secondary button (text style)
        if (onSecondaryAction && secondaryButtonText.isNotEmpty())
        {
            auto buttonBounds = getSecondaryButtonBounds();
            g.setColour(SidechainColors::textSecondary());
            g.setFont(juce::Font(juce::FontOptions().withHeight(13.0f)));
            g.drawText(secondaryButtonText, buttonBounds, juce::Justification::centred);
        }
    }
}

void ErrorState::resized()
{
    // Layout is handled in paint()
}

void ErrorState::mouseUp(const juce::MouseEvent& event)
{
    auto pos = event.getPosition();

    if (onPrimaryAction && getPrimaryButtonBounds().contains(pos))
    {
        onPrimaryAction();
        return;
    }

    if (onSecondaryAction && getSecondaryButtonBounds().contains(pos))
    {
        onSecondaryAction();
        return;
    }
}

//==============================================================================
void ErrorState::setErrorType(ErrorType type)
{
    errorType = type;
    repaint();
}

void ErrorState::setTitle(const juce::String& newTitle)
{
    title = newTitle;
    repaint();
}

void ErrorState::setMessage(const juce::String& newMessage)
{
    message = newMessage;
    repaint();
}

void ErrorState::setPrimaryAction(const juce::String& buttonText, std::function<void()> callback)
{
    primaryButtonText = buttonText;
    onPrimaryAction = std::move(callback);
    repaint();
}

void ErrorState::setSecondaryAction(const juce::String& buttonText, std::function<void()> callback)
{
    secondaryButtonText = buttonText;
    onSecondaryAction = std::move(callback);
    repaint();
}

void ErrorState::clearSecondaryAction()
{
    secondaryButtonText = "";
    onSecondaryAction = nullptr;
    repaint();
}

//==============================================================================
ErrorState::ErrorType ErrorState::detectErrorType(const juce::String& errorMessage)
{
    // Check specific types first, then more general ones

    // Timeout - check before general network
    if (errorMessage.containsIgnoreCase("timeout") ||
        errorMessage.containsIgnoreCase("timed out"))
    {
        return ErrorType::Timeout;
    }

    // Offline - check before general network
    if (errorMessage.containsIgnoreCase("offline") ||
        errorMessage.containsIgnoreCase("no internet"))
    {
        return ErrorType::Offline;
    }

    // Rate limiting
    if (errorMessage.containsIgnoreCase("429") ||
        errorMessage.containsIgnoreCase("rate limit") ||
        errorMessage.containsIgnoreCase("too many requests") ||
        errorMessage.containsIgnoreCase("slow down"))
    {
        return ErrorType::RateLimit;
    }

    // Network/connection errors
    if (errorMessage.containsIgnoreCase("network") ||
        errorMessage.containsIgnoreCase("connection") ||
        errorMessage.containsIgnoreCase("connect") ||
        errorMessage.containsIgnoreCase("unreachable"))
    {
        return ErrorType::Network;
    }

    // Authentication errors
    if (errorMessage.containsIgnoreCase("auth") ||
        errorMessage.containsIgnoreCase("unauthorized") ||
        errorMessage.containsIgnoreCase("401") ||
        errorMessage.containsIgnoreCase("forbidden") ||
        errorMessage.containsIgnoreCase("403") ||
        errorMessage.containsIgnoreCase("not authenticated") ||
        errorMessage.containsIgnoreCase("sign in") ||
        errorMessage.containsIgnoreCase("login"))
    {
        return ErrorType::Auth;
    }

    // Permission errors
    if (errorMessage.containsIgnoreCase("permission") ||
        errorMessage.containsIgnoreCase("denied") ||
        errorMessage.containsIgnoreCase("not allowed") ||
        errorMessage.containsIgnoreCase("microphone") ||
        errorMessage.containsIgnoreCase("access"))
    {
        return ErrorType::Permission;
    }

    // Not found
    if (errorMessage.containsIgnoreCase("not found") ||
        errorMessage.containsIgnoreCase("404") ||
        errorMessage.containsIgnoreCase("doesn't exist") ||
        errorMessage.containsIgnoreCase("does not exist"))
    {
        return ErrorType::NotFound;
    }

    // Parsing/data errors
    if (errorMessage.containsIgnoreCase("parse") ||
        errorMessage.containsIgnoreCase("json") ||
        errorMessage.containsIgnoreCase("invalid format") ||
        errorMessage.containsIgnoreCase("unexpected") ||
        errorMessage.containsIgnoreCase("malformed"))
    {
        return ErrorType::Parsing;
    }

    // Validation errors
    if (errorMessage.containsIgnoreCase("validation") ||
        errorMessage.containsIgnoreCase("invalid") ||
        errorMessage.containsIgnoreCase("required") ||
        errorMessage.containsIgnoreCase("must be") ||
        errorMessage.containsIgnoreCase("cannot be empty"))
    {
        return ErrorType::Validation;
    }

    // Upload errors
    if (errorMessage.containsIgnoreCase("upload") ||
        errorMessage.containsIgnoreCase("file size") ||
        errorMessage.containsIgnoreCase("too large"))
    {
        return ErrorType::Upload;
    }

    // Audio errors
    if (errorMessage.containsIgnoreCase("audio") ||
        errorMessage.containsIgnoreCase("playback") ||
        errorMessage.containsIgnoreCase("codec") ||
        errorMessage.containsIgnoreCase("format not supported"))
    {
        return ErrorType::Audio;
    }

    // Server errors (check after more specific types)
    if (errorMessage.containsIgnoreCase("500") ||
        errorMessage.containsIgnoreCase("502") ||
        errorMessage.containsIgnoreCase("503") ||
        errorMessage.containsIgnoreCase("504") ||
        errorMessage.containsIgnoreCase("server error") ||
        errorMessage.containsIgnoreCase("internal error") ||
        errorMessage.containsIgnoreCase("service unavailable"))
    {
        return ErrorType::ServerError;
    }

    // Default to generic
    return ErrorType::Generic;
}

void ErrorState::configureFromError(const juce::String& errorMessage)
{
    ErrorType detectedType = detectErrorType(errorMessage);
    setErrorType(detectedType);

    // Set the message unless it's a parsing error (which has a better default)
    if (errorMessage.isNotEmpty() && detectedType != ErrorType::Parsing)
    {
        setMessage(errorMessage);
    }
}

//==============================================================================
void ErrorState::drawIcon(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    g.setColour(getIconColorForType());
    g.setFont(juce::Font(juce::FontOptions().withHeight(static_cast<float>(bounds.getHeight()))));
    g.drawText(getIconForType(), bounds, juce::Justification::centred);
}

juce::String ErrorState::getIconForType() const
{
    switch (errorType)
    {
        // Network & Server errors
        case ErrorType::Network:
            return juce::String(juce::CharPointer_UTF8("\xF0\x9F\x8C\x90")); // Globe
        case ErrorType::Timeout:
            return juce::String(juce::CharPointer_UTF8("\xE2\x8F\xB1")); // Stopwatch
        case ErrorType::Offline:
            return juce::String(juce::CharPointer_UTF8("\xF0\x9F\x93\xB5")); // No signal
        case ErrorType::ServerError:
            return juce::String(juce::CharPointer_UTF8("\xF0\x9F\x96\xA5")); // Server/computer
        case ErrorType::RateLimit:
            return juce::String(juce::CharPointer_UTF8("\xE2\x8F\xB3")); // Hourglass

        // Auth & Permission errors
        case ErrorType::Auth:
            return juce::String(juce::CharPointer_UTF8("\xF0\x9F\x94\x92")); // Lock
        case ErrorType::Permission:
            return juce::String(juce::CharPointer_UTF8("\xF0\x9F\x9A\xAB")); // No entry

        // Resource errors
        case ErrorType::NotFound:
            return juce::String(juce::CharPointer_UTF8("\xF0\x9F\x94\x8D")); // Magnifying glass
        case ErrorType::Empty:
            return juce::String(juce::CharPointer_UTF8("\xF0\x9F\x93\xAD")); // Empty inbox

        // Data errors
        case ErrorType::Validation:
            return juce::String(juce::CharPointer_UTF8("\xE2\x9C\x8F")); // Pencil
        case ErrorType::Parsing:
            return juce::String(juce::CharPointer_UTF8("\xF0\x9F\x93\x84")); // Document

        // Media errors
        case ErrorType::Upload:
            return juce::String(juce::CharPointer_UTF8("\xE2\xAC\x86")); // Upload arrow
        case ErrorType::Audio:
            return juce::String(juce::CharPointer_UTF8("\xF0\x9F\x94\x87")); // Muted speaker

        case ErrorType::Generic:
        default:
            return juce::String(juce::CharPointer_UTF8("\xE2\x9A\xA0\xEF\xB8\x8F")); // Warning
    }
}

juce::String ErrorState::getDefaultTitleForType() const
{
    switch (errorType)
    {
        // Network & Server errors
        case ErrorType::Network:
            return "Connection Error";
        case ErrorType::Timeout:
            return "Request Timed Out";
        case ErrorType::Offline:
            return "You're Offline";
        case ErrorType::ServerError:
            return "Server Error";
        case ErrorType::RateLimit:
            return "Slow Down";

        // Auth & Permission errors
        case ErrorType::Auth:
            return "Not Signed In";
        case ErrorType::Permission:
            return "Permission Required";

        // Resource errors
        case ErrorType::NotFound:
            return "Not Found";
        case ErrorType::Empty:
            return "Nothing Here";

        // Data errors
        case ErrorType::Validation:
            return "Invalid Input";
        case ErrorType::Parsing:
            return "Data Error";

        // Media errors
        case ErrorType::Upload:
            return "Upload Failed";
        case ErrorType::Audio:
            return "Audio Error";

        case ErrorType::Generic:
        default:
            return "Something Went Wrong";
    }
}

juce::String ErrorState::getDefaultMessageForType() const
{
    switch (errorType)
    {
        // Network & Server errors
        case ErrorType::Network:
            return "Check your internet connection and try again.";
        case ErrorType::Timeout:
            return "The request took too long. Please try again.";
        case ErrorType::Offline:
            return "Connect to the internet to continue.";
        case ErrorType::ServerError:
            return "We're having trouble with our servers. Please try again later.";
        case ErrorType::RateLimit:
            return "You're making too many requests. Please wait a moment and try again.";

        // Auth & Permission errors
        case ErrorType::Auth:
            return "Please sign in to continue.";
        case ErrorType::Permission:
            return "This action requires additional permissions.";

        // Resource errors
        case ErrorType::NotFound:
            return "The content you're looking for couldn't be found.";
        case ErrorType::Empty:
            return "There's nothing to show here yet.";

        // Data errors
        case ErrorType::Validation:
            return "Please check your input and try again.";
        case ErrorType::Parsing:
            return "We received unexpected data. This might be a temporary issue.";

        // Media errors
        case ErrorType::Upload:
            return "Failed to upload your file. Please check your connection and try again.";
        case ErrorType::Audio:
            return "There was a problem with audio playback.";

        case ErrorType::Generic:
        default:
            return "An unexpected error occurred. Please try again.";
    }
}

juce::Colour ErrorState::getIconColorForType() const
{
    switch (errorType)
    {
        // Network & Server errors
        case ErrorType::Network:
            return SidechainColors::warning();
        case ErrorType::Timeout:
            return SidechainColors::warning();
        case ErrorType::Offline:
            return SidechainColors::textMuted();
        case ErrorType::ServerError:
            return SidechainColors::error();
        case ErrorType::RateLimit:
            return SidechainColors::warning();

        // Auth & Permission errors
        case ErrorType::Auth:
            return SidechainColors::primary();
        case ErrorType::Permission:
            return SidechainColors::warning();

        // Resource errors
        case ErrorType::NotFound:
            return SidechainColors::textMuted();
        case ErrorType::Empty:
            return SidechainColors::textMuted();

        // Data errors
        case ErrorType::Validation:
            return SidechainColors::warning();
        case ErrorType::Parsing:
            return SidechainColors::error();

        // Media errors
        case ErrorType::Upload:
            return SidechainColors::error();
        case ErrorType::Audio:
            return SidechainColors::warning();

        case ErrorType::Generic:
        default:
            return SidechainColors::error();
    }
}

juce::Rectangle<int> ErrorState::getPrimaryButtonBounds() const
{
    auto bounds = getLocalBounds();

    if (compactMode)
    {
        return juce::Rectangle<int>(bounds.getRight() - 80, bounds.getCentreY() - 14, 70, 28);
    }
    else
    {
        int buttonWidth = 140;
        int buttonHeight = 40;

        // Calculate Y position based on content layout
        int iconSize = showIcon ? 48 : 0;
        int spacing = 16;
        int contentHeight = (showIcon ? iconSize + spacing : 0) + 24 + spacing / 2 + 40 + spacing;
        int startY = (bounds.getHeight() - contentHeight - buttonHeight) / 2 + contentHeight;

        return juce::Rectangle<int>(bounds.getCentreX() - buttonWidth / 2, startY, buttonWidth, buttonHeight);
    }
}

juce::Rectangle<int> ErrorState::getSecondaryButtonBounds() const
{
    if (compactMode || !onSecondaryAction)
        return juce::Rectangle<int>();

    auto primaryBounds = getPrimaryButtonBounds();
    return primaryBounds.translated(0, 48);
}

//==============================================================================
// Factory methods

std::unique_ptr<ErrorState> ErrorState::networkError(std::function<void()> onRetry)
{
    auto error = std::make_unique<ErrorState>();
    error->setErrorType(ErrorType::Network);
    error->setPrimaryAction("Retry", std::move(onRetry));
    return error;
}

std::unique_ptr<ErrorState> ErrorState::authError(std::function<void()> onSignIn)
{
    auto error = std::make_unique<ErrorState>();
    error->setErrorType(ErrorType::Auth);
    error->setPrimaryAction("Sign In", std::move(onSignIn));
    return error;
}

std::unique_ptr<ErrorState> ErrorState::notFoundError(const juce::String& resourceName)
{
    auto error = std::make_unique<ErrorState>();
    error->setErrorType(ErrorType::NotFound);
    error->setMessage("The " + resourceName + " you're looking for couldn't be found.");
    return error;
}

std::unique_ptr<ErrorState> ErrorState::genericError(const juce::String& errorMessage, std::function<void()> onRetry)
{
    auto error = std::make_unique<ErrorState>();
    error->setErrorType(ErrorType::Generic);
    if (errorMessage.isNotEmpty())
        error->setMessage(errorMessage);
    error->setPrimaryAction("Try Again", std::move(onRetry));
    return error;
}

std::unique_ptr<ErrorState> ErrorState::emptyState(const juce::String& emptyTitle, const juce::String& emptyMessage,
                                                   const juce::String& actionText, std::function<void()> onAction)
{
    auto error = std::make_unique<ErrorState>();
    error->setErrorType(ErrorType::Empty);
    error->setTitle(emptyTitle);
    error->setMessage(emptyMessage);
    if (actionText.isNotEmpty() && onAction)
        error->setPrimaryAction(actionText, std::move(onAction));
    return error;
}

std::unique_ptr<ErrorState> ErrorState::timeoutError(std::function<void()> onRetry)
{
    auto error = std::make_unique<ErrorState>();
    error->setErrorType(ErrorType::Timeout);
    error->setPrimaryAction("Retry", std::move(onRetry));
    return error;
}

std::unique_ptr<ErrorState> ErrorState::offlineError(std::function<void()> onRetry)
{
    auto error = std::make_unique<ErrorState>();
    error->setErrorType(ErrorType::Offline);
    error->setPrimaryAction("Retry", std::move(onRetry));
    return error;
}

std::unique_ptr<ErrorState> ErrorState::rateLimitError(std::function<void()> onRetry)
{
    auto error = std::make_unique<ErrorState>();
    error->setErrorType(ErrorType::RateLimit);
    error->setPrimaryAction("Try Again", std::move(onRetry));
    return error;
}

std::unique_ptr<ErrorState> ErrorState::permissionError(const juce::String& permissionName, std::function<void()> onSettings)
{
    auto error = std::make_unique<ErrorState>();
    error->setErrorType(ErrorType::Permission);
    error->setMessage(permissionName + " permission is required for this feature.");
    if (onSettings)
        error->setPrimaryAction("Open Settings", std::move(onSettings));
    return error;
}

std::unique_ptr<ErrorState> ErrorState::validationError(const juce::String& validationMessage)
{
    auto error = std::make_unique<ErrorState>();
    error->setErrorType(ErrorType::Validation);
    if (validationMessage.isNotEmpty())
        error->setMessage(validationMessage);
    return error;
}

std::unique_ptr<ErrorState> ErrorState::parsingError(const juce::String& context, std::function<void()> onRetry)
{
    auto error = std::make_unique<ErrorState>();
    error->setErrorType(ErrorType::Parsing);
    if (context.isNotEmpty())
        error->setMessage("Failed to parse " + context + ". This might be a temporary issue.");
    if (onRetry)
        error->setPrimaryAction("Retry", std::move(onRetry));
    return error;
}

std::unique_ptr<ErrorState> ErrorState::uploadError(const juce::String& uploadMessage, std::function<void()> onRetry)
{
    auto error = std::make_unique<ErrorState>();
    error->setErrorType(ErrorType::Upload);
    if (uploadMessage.isNotEmpty())
        error->setMessage(uploadMessage);
    error->setPrimaryAction("Try Again", std::move(onRetry));
    return error;
}

std::unique_ptr<ErrorState> ErrorState::audioError(const juce::String& audioMessage, std::function<void()> onRetry)
{
    auto error = std::make_unique<ErrorState>();
    error->setErrorType(ErrorType::Audio);
    if (audioMessage.isNotEmpty())
        error->setMessage(audioMessage);
    if (onRetry)
        error->setPrimaryAction("Retry", std::move(onRetry));
    return error;
}
