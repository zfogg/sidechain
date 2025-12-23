#pragma once

#include <JuceHeader.h>
#include <functional>

// Forward declarations
namespace Sidechain::Stores {
class AppStore;
}

// ==============================================================================
/**
 * UIHelpers - Common UI drawing utilities
 *
 * Provides consistent drawing patterns for cards, badges, buttons, and text
 * to reduce duplication across components.
 *
 * Usage:
 *   // Draw a card with background and border
 *   UIHelpers::drawCard(g, bounds, Colors::backgroundLight, Colors::border);
 *
 *   // Draw a badge with text
 *   UIHelpers::drawBadge(g, bounds, "120 BPM", Colors::badge,
 * Colors::textPrimary);
 *
 *   // Truncate text with ellipsis
 *   auto truncated = UIHelpers::truncateWithEllipsis("Very long text...", font,
 * 100);
 *
 *   // Load image asynchronously with SafePointer pattern
 *   UIHelpers::loadImageAsync(this, appStore, imageUrl,
 *       [](MyComponent* comp, const juce::Image& img) {
 *           comp->cachedImage = img;
 *           comp->repaint();
 *       });
 */
namespace UIHelpers {
// ==========================================================================
// Card/Panel Drawing

/**
 * Draw a rounded rectangle card with fill and optional border.
 *
 * @param g            Graphics context
 * @param bounds       Rectangle to draw
 * @param fillColor    Background fill color
 * @param borderColor  Border color (use transparent for no border)
 * @param cornerRadius Corner radius (default 8.0f)
 * @param borderWidth  Border thickness (default 1.0f)
 */
void drawCard(juce::Graphics &g, juce::Rectangle<float> bounds, juce::Colour fillColor,
              juce::Colour borderColor = juce::Colours::transparentBlack, float cornerRadius = 8.0f,
              float borderWidth = 1.0f);

/** Overload for int bounds */
void drawCard(juce::Graphics &g, juce::Rectangle<int> bounds, juce::Colour fillColor,
              juce::Colour borderColor = juce::Colours::transparentBlack, float cornerRadius = 8.0f,
              float borderWidth = 1.0f);

/**
 * Draw a card with hover state support.
 */
void drawCardWithHover(juce::Graphics &g, juce::Rectangle<int> bounds, juce::Colour normalColor,
                       juce::Colour hoverColor, juce::Colour borderColor, bool isHovered, float cornerRadius = 8.0f);

// ==========================================================================
// Badge/Tag Drawing

/**
 * Draw a small badge/tag with text (e.g., "120 BPM", "NEW").
 *
 * @param g         Graphics context
 * @param bounds    Rectangle to draw
 * @param text      Badge text
 * @param bgColor   Background color
 * @param textColor Text color
 * @param fontSize  Font size (default 11.0f)
 * @param cornerRadius Corner radius (default 4.0f)
 */
void drawBadge(juce::Graphics &g, juce::Rectangle<int> bounds, const juce::String &text, juce::Colour bgColor,
               juce::Colour textColor, float fontSize = 11.0f, float cornerRadius = 4.0f);

/**
 * Draw a pill-shaped badge that auto-sizes to text.
 * Returns the actual bounds used.
 */
juce::Rectangle<int> drawPillBadge(juce::Graphics &g, int x, int y, const juce::String &text, juce::Colour bgColor,
                                   juce::Colour textColor, float fontSize = 11.0f, int hPadding = 8, int vPadding = 4);

// ==========================================================================
// Button Drawing

/**
 * Draw a rounded button with text.
 *
 * @param g           Graphics context
 * @param bounds      Button bounds
 * @param text        Button text
 * @param bgColor     Background color
 * @param textColor   Text color
 * @param isHovered   Whether button is being hovered
 * @param cornerRadius Corner radius (default 6.0f)
 */
void drawButton(juce::Graphics &g, juce::Rectangle<int> bounds, const juce::String &text, juce::Colour bgColor,
                juce::Colour textColor, bool isHovered = false, float cornerRadius = 6.0f);

/**
 * Draw an outline-style button.
 */
void drawOutlineButton(juce::Graphics &g, juce::Rectangle<int> bounds, const juce::String &text,
                       juce::Colour borderColor, juce::Colour textColor, bool isHovered = false,
                       float cornerRadius = 6.0f);

// ==========================================================================
// Avatar Drawing

/**
 * Draw a circular avatar image with optional border.
 * If the image is invalid, draws a placeholder circle with the given color.
 *
 * @param g              Graphics context
 * @param bounds         Rectangle to draw the avatar in
 * @param image          The avatar image to draw (can be invalid for placeholder)
 * @param placeholderColor Color for the placeholder circle if image is invalid
 * @param borderColor    Border color (use transparent for no border)
 * @param borderWidth    Border thickness (default 1.0f)
 */
void drawCircularAvatar(juce::Graphics &g, juce::Rectangle<int> bounds, const juce::Image &image,
                        juce::Colour placeholderColor, juce::Colour borderColor = juce::Colours::transparentBlack,
                        float borderWidth = 1.0f);

/**
 * Draw an online/in-studio status indicator dot.
 * Typically positioned at the bottom-right of an avatar.
 *
 * @param g              Graphics context
 * @param avatarBounds   The avatar bounds (indicator will be positioned relative to this)
 * @param isOnline       Whether the user is online
 * @param isInStudio     Whether the user is in studio (takes precedence for color)
 * @param backgroundColor Background color for the indicator border (usually card background)
 * @param indicatorSize  Size of the indicator (default 14)
 */
void drawOnlineIndicator(juce::Graphics &g, juce::Rectangle<int> avatarBounds, bool isOnline, bool isInStudio,
                         juce::Colour backgroundColor, int indicatorSize = 14);

/**
 * Draw a circular avatar with initials fallback.
 * If the image is invalid, draws initials on a colored placeholder.
 * Combines drawCircularAvatar + initials drawing into a single call.
 *
 * @param g              Graphics context
 * @param bounds         Rectangle to draw the avatar in
 * @param image          The avatar image to draw (can be invalid for initials fallback)
 * @param name           Name to generate initials from
 * @param placeholderColor Color for the placeholder circle if image is invalid
 * @param initialsColor  Color for the initials text
 * @param borderColor    Border color (use transparent for no border)
 * @param borderWidth    Border thickness (default 1.0f)
 */
void drawAvatarWithInitials(juce::Graphics &g, juce::Rectangle<int> bounds, const juce::Image &image,
                            const juce::String &name, juce::Colour placeholderColor, juce::Colour initialsColor,
                            juce::Colour borderColor = juce::Colours::transparentBlack, float borderWidth = 1.0f);

// ==========================================================================
// Follow Button Drawing

/**
 * Draw a follow/following button with consistent styling.
 *
 * @param g              Graphics context
 * @param bounds         Button bounds
 * @param isFollowing    Current follow state
 * @param followColor    Color for the "Follow" state button
 * @param followTextColor Text color for "Follow" state
 * @param followingTextColor Text color for "Following" state
 * @param borderColor    Border color for "Following" state
 * @param cornerRadius   Corner radius (default 4.0f)
 */
void drawFollowButton(juce::Graphics &g, juce::Rectangle<int> bounds, bool isFollowing, juce::Colour followColor,
                      juce::Colour followTextColor, juce::Colour followingTextColor, juce::Colour borderColor,
                      float cornerRadius = 4.0f);

// ==========================================================================
// Icon Drawing

/**
 * Draw a circular icon button background.
 */
void drawIconButton(juce::Graphics &g, juce::Rectangle<int> bounds, juce::Colour bgColor, bool isHovered = false);

/**
 * Draw a simple icon character (emoji or symbol).
 */
void drawIcon(juce::Graphics &g, juce::Rectangle<int> bounds, const juce::String &icon, juce::Colour color,
              float fontSize = 16.0f);

// ==========================================================================
// Progress/Status Drawing

/**
 * Draw a horizontal progress bar.
 */
void drawProgressBar(juce::Graphics &g, juce::Rectangle<int> bounds,
                     float progress, // 0.0 to 1.0
                     juce::Colour bgColor, juce::Colour fillColor, float cornerRadius = 4.0f);

/**
 * Draw a loading spinner indicator.
 */
void drawLoadingSpinner(juce::Graphics &g, juce::Rectangle<int> bounds, juce::Colour color,
                        float rotation); // In radians

// ==========================================================================
// Separator Drawing

/**
 * Draw a horizontal divider line.
 */
void drawDivider(juce::Graphics &g, int x, int y, int width, juce::Colour color, float thickness = 1.0f);

/**
 * Draw a vertical divider line.
 */
void drawVerticalDivider(juce::Graphics &g, int x, int y, int height, juce::Colour color, float thickness = 1.0f);

// ==========================================================================
// Text Utilities

/**
 * Truncate text to fit within maxWidth, adding ellipsis if needed.
 *
 * @param text     The text to truncate
 * @param font     Font to measure with
 * @param maxWidth Maximum width in pixels
 * @return         Truncated text with "..." if needed
 */
juce::String truncateWithEllipsis(const juce::String &text, const juce::Font &font, int maxWidth);

/**
 * Draw text with automatic ellipsis truncation.
 */
void drawTruncatedText(juce::Graphics &g, const juce::String &text, juce::Rectangle<int> bounds, juce::Colour color,
                       juce::Justification justification = juce::Justification::centredLeft);

/**
 * Calculate text width for the current font.
 */
int getTextWidth(const juce::Graphics &g, const juce::String &text);
int getTextWidth(const juce::Font &font, const juce::String &text);

// ==========================================================================
// Toggle Button Styling

/**
 * Style a toggle button with consistent colors and appearance.
 * Sets text color, tick color, and disabled tick color.
 *
 * @param toggle       The toggle button to style
 * @param label        The button text label
 * @param textColor    Text color for the label
 * @param tickColor    Color when toggle is checked
 * @param disabledColor Color when toggle is disabled/unchecked
 */
void styleToggleButton(juce::ToggleButton &toggle, const juce::String &label, juce::Colour textColor,
                       juce::Colour tickColor, juce::Colour disabledColor);

/**
 * Style and configure a toggle button with click handler.
 * Combines styling with toggle state and callback setup.
 *
 * @param toggle        The toggle button to style
 * @param label         The button text label
 * @param textColor     Text color for the label
 * @param tickColor     Color when toggle is checked
 * @param disabledColor Color when toggle is disabled/unchecked
 * @param initialState  Initial toggle state
 * @param onClick       Callback when toggle is clicked
 */
void setupToggleButton(juce::ToggleButton &toggle, const juce::String &label, juce::Colour textColor,
                       juce::Colour tickColor, juce::Colour disabledColor, bool initialState,
                       std::function<void()> onClick);

// ==========================================================================
// Shadow/Effects

/**
 * Draw a drop shadow beneath a rectangle.
 * Call before drawing the actual element.
 */
void drawDropShadow(juce::Graphics &g, juce::Rectangle<int> bounds, juce::Colour shadowColor, int radius = 4,
                    juce::Point<int> offset = {0, 2});

// ==========================================================================
// Tooltip

/**
 * Draw a tooltip box with text.
 */
void drawTooltip(juce::Graphics &g, juce::Rectangle<int> bounds, const juce::String &text, juce::Colour bgColor,
                 juce::Colour textColor);

// ==========================================================================
// Async Image Loading

/**
 * Load an image asynchronously using the SafePointer pattern.
 * This eliminates the common boilerplate of creating SafePointer, subscribing
 * to the observable, and checking for null in callbacks.
 *
 * Usage:
 *   UIHelpers::loadImageAsync(this, appStore, avatarUrl,
 *       [](CommentRow* comp, const juce::Image& img) {
 *           comp->cachedAvatarImage = img;
 *           comp->repaint();
 *       },
 *       [](CommentRow* comp) {
 *           Log::warn("Failed to load avatar");
 *       });
 *
 * @tparam ComponentType The type of component (must be a juce::Component subclass)
 * @param component      Pointer to the component requesting the image
 * @param appStore       AppStore instance for image loading
 * @param imageUrl       URL of the image to load
 * @param onSuccess      Callback when image loads successfully (receives component and image)
 * @param onError        Optional callback when loading fails (receives component)
 * @param logContext     Optional context string for error logging
 */
template <typename ComponentType>
void loadImageAsync(ComponentType *component, Sidechain::Stores::AppStore *appStore, const juce::String &imageUrl,
                    std::function<void(ComponentType *, const juce::Image &)> onSuccess,
                    std::function<void(ComponentType *)> onError = nullptr,
                    const juce::String &logContext = "UIHelpers");

// Non-template declaration for implementation in .cpp file
void loadImageAsyncImpl(juce::Component *component, Sidechain::Stores::AppStore *appStore, const juce::String &imageUrl,
                        std::function<void(const juce::Image &)> onSuccess, std::function<void()> onError,
                        const juce::String &logContext);

} // namespace UIHelpers

// Template implementation (must be in header)
template <typename ComponentType>
void UIHelpers::loadImageAsync(ComponentType *component, Sidechain::Stores::AppStore *appStore,
                               const juce::String &imageUrl,
                               std::function<void(ComponentType *, const juce::Image &)> onSuccess,
                               std::function<void(ComponentType *)> onError, const juce::String &logContext) {
  if (component == nullptr || appStore == nullptr || imageUrl.isEmpty())
    return;

  juce::Component::SafePointer<ComponentType> safeThis(component);

  auto successWrapper = [safeThis, onSuccess](const juce::Image &image) {
    if (safeThis == nullptr)
      return;
    if (image.isValid() && onSuccess) {
      onSuccess(safeThis.getComponent(), image);
    }
  };

  auto errorWrapper = [safeThis, onError]() {
    if (safeThis == nullptr)
      return;
    if (onError) {
      onError(safeThis.getComponent());
    }
  };

  loadImageAsyncImpl(component, appStore, imageUrl, successWrapper, errorWrapper, logContext);
}
