#include "TextEditorStyler.h"
#include "Colors.h"

namespace TextEditorStyler
{

//==============================================================================
// Internal helpers

namespace
{
    void applyBaseStyle(juce::TextEditor& editor)
    {
        // Colors
        editor.setColour(juce::TextEditor::backgroundColourId, SidechainColors::surface());
        editor.setColour(juce::TextEditor::outlineColourId, SidechainColors::border());
        editor.setColour(juce::TextEditor::focusedOutlineColourId, SidechainColors::primary());
        editor.setColour(juce::TextEditor::textColourId, SidechainColors::textPrimary());
        editor.setColour(juce::TextEditor::highlightColourId, SidechainColors::primary().withAlpha(0.3f));
        editor.setColour(juce::TextEditor::highlightedTextColourId, SidechainColors::textPrimary());
        editor.setColour(juce::CaretComponent::caretColourId, SidechainColors::primary());

        // Font
        editor.setFont(juce::Font(14.0f));

        // Common properties
        editor.setCaretVisible(true);
        editor.setPopupMenuEnabled(true);
        editor.setSelectAllWhenFocused(false);
        editor.setIndents(12, 0);
    }

    void applySingleLineStyle(juce::TextEditor& editor)
    {
        editor.setMultiLine(false, false);
        editor.setReturnKeyStartsNewLine(false);
        editor.setScrollbarsShown(false);
    }

    void applyMultiLineStyle(juce::TextEditor& editor)
    {
        editor.setMultiLine(true, true);
        editor.setReturnKeyStartsNewLine(true);
        editor.setScrollbarsShown(true);
        editor.setIndents(12, 8);
    }
}

//==============================================================================
// Standard Styling

void style(juce::TextEditor& editor, const juce::String& placeholder)
{
    applyBaseStyle(editor);
    applySingleLineStyle(editor);

    if (placeholder.isNotEmpty())
        setPlaceholder(editor, placeholder);
}

void stylePassword(juce::TextEditor& editor, const juce::String& placeholder)
{
    style(editor, placeholder);
    editor.setPasswordCharacter(static_cast<juce::juce_wchar>(0x2022));  // bullet character
}

void styleMultiline(juce::TextEditor& editor, const juce::String& placeholder)
{
    applyBaseStyle(editor);
    applyMultiLineStyle(editor);

    if (placeholder.isNotEmpty())
        setPlaceholder(editor, placeholder);
}

//==============================================================================
// Specialized Styling

void styleEmail(juce::TextEditor& editor, const juce::String& placeholder)
{
    style(editor, placeholder);
    // Email fields typically don't need character restrictions
    // but we set a reasonable max length
    editor.setInputRestrictions(254);  // RFC 5321 max email length
}

void styleUsername(juce::TextEditor& editor, const juce::String& placeholder)
{
    style(editor, placeholder);
    // Usernames: lowercase alphanumeric + underscore, max 30 chars
    editor.setInputRestrictions(30, "abcdefghijklmnopqrstuvwxyz0123456789_");
}

void styleNumeric(juce::TextEditor& editor, const juce::String& placeholder)
{
    style(editor, placeholder);
    editor.setInputRestrictions(0, "0123456789.-");
}

void styleUrl(juce::TextEditor& editor, const juce::String& placeholder)
{
    style(editor, placeholder);
    // URLs can be quite long
    editor.setInputRestrictions(2048);
}

//==============================================================================
// Configuration

void setInputRestrictions(juce::TextEditor& editor, int maxLength, const juce::String& allowedChars)
{
    editor.setInputRestrictions(maxLength, allowedChars);
}

void setPlaceholder(juce::TextEditor& editor, const juce::String& placeholder)
{
    editor.setTextToShowWhenEmpty(placeholder, SidechainColors::textMuted());
}

void styleReadOnly(juce::TextEditor& editor)
{
    applyBaseStyle(editor);
    applySingleLineStyle(editor);

    editor.setReadOnly(true);
    editor.setCaretVisible(false);
    editor.setColour(juce::TextEditor::backgroundColourId, SidechainColors::surface());
    editor.setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
}

void setErrorState(juce::TextEditor& editor, bool hasError)
{
    if (hasError)
    {
        editor.setColour(juce::TextEditor::outlineColourId, SidechainColors::error());
        editor.setColour(juce::TextEditor::focusedOutlineColourId, SidechainColors::error());
    }
    else
    {
        clearErrorState(editor);
    }
}

void clearErrorState(juce::TextEditor& editor)
{
    editor.setColour(juce::TextEditor::outlineColourId, SidechainColors::border());
    editor.setColour(juce::TextEditor::focusedOutlineColourId, SidechainColors::primary());
}

}  // namespace TextEditorStyler
