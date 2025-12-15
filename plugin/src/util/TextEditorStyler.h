#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
 * TextEditorStyler - Consistent TextEditor configuration and styling
 *
 * Centralizes the repeated styling operations applied to TextEditors
 * throughout the UI (colors, fonts, placeholders, input restrictions).
 *
 * Usage:
 *   TextEditorStyler::style(emailEditor, "Email address");
 *   TextEditorStyler::stylePassword(passwordEditor, "Password");
 *   TextEditorStyler::styleMultiline(bioEditor, "Tell us about yourself...");
 */
namespace TextEditorStyler {
//==========================================================================
// Standard Styling

/**
 * Apply standard single-line text editor styling.
 * Sets colors, font, placeholder, and common properties.
 */
void style(juce::TextEditor &editor, const juce::String &placeholder = "");

/**
 * Apply password field styling (masked input).
 */
void stylePassword(juce::TextEditor &editor, const juce::String &placeholder = "Password");

/**
 * Apply multiline text editor styling.
 * Enables word wrap, scrollbars, and newline on return.
 */
void styleMultiline(juce::TextEditor &editor, const juce::String &placeholder = "");

//==========================================================================
// Specialized Styling

/**
 * Apply email field styling with appropriate keyboard hints.
 */
void styleEmail(juce::TextEditor &editor, const juce::String &placeholder = "Email");

/**
 * Apply username field styling (lowercase, restricted characters).
 */
void styleUsername(juce::TextEditor &editor, const juce::String &placeholder = "Username");

/**
 * Apply numeric-only field styling.
 */
void styleNumeric(juce::TextEditor &editor, const juce::String &placeholder = "");

/**
 * Apply URL field styling.
 */
void styleUrl(juce::TextEditor &editor, const juce::String &placeholder = "https://");

//==========================================================================
// Configuration

/**
 * Set input character restrictions.
 * @param maxLength Maximum number of characters (0 = unlimited)
 * @param allowedChars String of allowed characters (empty = all allowed)
 */
void setInputRestrictions(juce::TextEditor &editor, int maxLength, const juce::String &allowedChars = "");

/**
 * Set placeholder text with appropriate styling.
 */
void setPlaceholder(juce::TextEditor &editor, const juce::String &placeholder);

/**
 * Apply read-only styling (non-editable display).
 */
void styleReadOnly(juce::TextEditor &editor);

/**
 * Apply error state styling (red border).
 */
void setErrorState(juce::TextEditor &editor, bool hasError);

/**
 * Reset to normal state (remove error styling).
 */
void clearErrorState(juce::TextEditor &editor);

} // namespace TextEditorStyler
