#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
 * UtilColors - Centralized color scheme for the Sidechain plugin
 *
 * Color Palette:
 *   Coral Pink:    #FD7792 - Primary accent, buttons, active states
 *   Rose Pink:     #EDA1C1 - Secondary accent, hover states
 *   Lavender:      #DCAEE8 - Tertiary accent, highlights
 *   Sky Blue:      #B4E4FF - Info, links, interactive elements
 *   Soft Blue:     #95BDFF - Secondary blue, icons
 *   Cream Yellow:  #F8F7BA - Warnings, highlights
 *   Mint Green:    #B5FFC7 - Success states, positive feedback
 *
 * Usage Guidelines:
 *   - Use coral/rose for primary actions and CTAs
 *   - Use blues for links and interactive elements
 *   - Use mint green for success/confirmation
 *   - Use cream yellow sparingly for attention
 *   - Background colors are derived with lower alpha or darker variants
 */
namespace SidechainColors
{
    //==========================================================================
    // Primary Palette
    //==========================================================================

    // Coral Pink - Primary accent color
    inline const juce::Colour coralPink()     { return juce::Colour(0xFFFD7792); }

    // Rose Pink - Secondary accent
    inline const juce::Colour rosePink()      { return juce::Colour(0xFFEDA1C1); }

    // Lavender - Tertiary accent
    inline const juce::Colour lavender()      { return juce::Colour(0xFFDCAEE8); }

    // Sky Blue - Primary blue
    inline const juce::Colour skyBlue()       { return juce::Colour(0xFFB4E4FF); }

    // Soft Blue - Secondary blue
    inline const juce::Colour softBlue()      { return juce::Colour(0xFF95BDFF); }

    // Cream Yellow - Highlight/warning
    inline const juce::Colour creamYellow()   { return juce::Colour(0xFFF8F7BA); }

    // Mint Green - Success states
    inline const juce::Colour mintGreen()     { return juce::Colour(0xFFB5FFC7); }

    //==========================================================================
    // Semantic Colors (mapped to palette)
    //==========================================================================

    // Primary action color (buttons, CTAs)
    inline const juce::Colour primary()       { return coralPink(); }

    // Secondary action color
    inline const juce::Colour secondary()     { return rosePink(); }

    // Accent/highlight color
    inline const juce::Colour accent()        { return lavender(); }

    // Link/interactive element color
    inline const juce::Colour link()          { return skyBlue(); }

    // Success state (uploads complete, etc.)
    inline const juce::Colour success()       { return mintGreen(); }

    // Warning state
    inline const juce::Colour warning()       { return creamYellow(); }

    // Error state (derived - darker red)
    inline const juce::Colour error()         { return coralPink().darker(0.2f); }

    //==========================================================================
    // Background Colors (dark theme)
    //==========================================================================

    // Main background - very dark
    inline const juce::Colour background()          { return juce::Colour(0xFF1C1C20); }

    // Slightly lighter background for cards/panels
    inline const juce::Colour backgroundLight()     { return juce::Colour(0xFF26262C); }

    // Even lighter for hover states on dark elements
    inline const juce::Colour backgroundLighter()   { return juce::Colour(0xFF30303A); }

    // Surface color for input fields, dropdowns
    inline const juce::Colour surface()             { return juce::Colour(0xFF38384C); }

    // Surface hover state
    inline const juce::Colour surfaceHover()        { return juce::Colour(0xFF484858); }

    //==========================================================================
    // Text Colors
    //==========================================================================

    // Primary text (white)
    inline const juce::Colour textPrimary()         { return juce::Colours::white; }

    // Secondary text (slightly dimmed)
    inline const juce::Colour textSecondary()       { return juce::Colour(0xFFB0B0B8); }

    // Tertiary/muted text
    inline const juce::Colour textMuted()           { return juce::Colour(0xFF808088); }

    // Disabled text
    inline const juce::Colour textDisabled()        { return juce::Colour(0xFF606068); }

    //==========================================================================
    // Border Colors
    //==========================================================================

    // Default border
    inline const juce::Colour border()              { return juce::Colour(0xFF404050); }

    // Subtle border
    inline const juce::Colour borderSubtle()        { return juce::Colour(0xFF303040); }

    // Active/focused border
    inline const juce::Colour borderActive()        { return coralPink(); }

    //==========================================================================
    // Waveform Colors
    //==========================================================================

    // Waveform primary color
    inline const juce::Colour waveform()            { return skyBlue(); }

    // Waveform played/progress color
    inline const juce::Colour waveformPlayed()      { return coralPink(); }

    // Waveform background
    inline const juce::Colour waveformBackground()  { return backgroundLight(); }

    //==========================================================================
    // Button Colors
    //==========================================================================

    // Primary button background
    inline const juce::Colour buttonPrimary()       { return coralPink(); }

    // Primary button hover
    inline const juce::Colour buttonPrimaryHover()  { return coralPink().brighter(0.15f); }

    // Secondary button background
    inline const juce::Colour buttonSecondary()     { return surface(); }

    // Secondary button hover
    inline const juce::Colour buttonSecondaryHover() { return surfaceHover(); }

    // Danger/destructive button
    inline const juce::Colour buttonDanger()        { return juce::Colour(0xFFE05555); }

    //==========================================================================
    // Status Colors
    //==========================================================================

    // Online/active indicator
    inline const juce::Colour online()              { return mintGreen(); }

    // Online indicator (green dot)
    inline const juce::Colour onlineIndicator()     { return juce::Colour(0xFF00D464); }

    // In studio indicator (cyan dot)
    inline const juce::Colour inStudioIndicator()   { return juce::Colour(0xFF00D4FF); }

    // Offline indicator
    inline const juce::Colour offline()             { return textMuted(); }

    // Recording indicator
    inline const juce::Colour recording()           { return coralPink(); }

    // Playing indicator
    inline const juce::Colour playing()             { return skyBlue(); }

    //==========================================================================
    // Interaction Colors
    //==========================================================================

    // Like/heart color
    inline const juce::Colour like()                { return coralPink(); }

    // Comment color
    inline const juce::Colour comment()             { return softBlue(); }

    // Share color
    inline const juce::Colour share()               { return lavender(); }

    // Follow button color
    inline const juce::Colour follow()              { return juce::Colour(0xFFC9D9FB); }  // Light blue

    //==========================================================================
    // Gradients
    //==========================================================================

    // Primary gradient (for headers, hero sections)
    inline juce::ColourGradient primaryGradient(juce::Point<float> start, juce::Point<float> end)
    {
        return juce::ColourGradient(coralPink(), start, lavender(), end, false);
    }

    // Background gradient
    inline juce::ColourGradient backgroundGradient(juce::Point<float> start, juce::Point<float> end)
    {
        return juce::ColourGradient(background(), start, backgroundLight(), end, false);
    }

    //==========================================================================
    // Utility Functions
    //==========================================================================

    // Get color with custom alpha
    inline juce::Colour withAlpha(const juce::Colour& c, float alpha)
    {
        return c.withAlpha(alpha);
    }

    // Darken a color by amount (0.0 - 1.0)
    inline juce::Colour darken(const juce::Colour& c, float amount)
    {
        return c.darker(amount);
    }

    // Lighten a color by amount (0.0 - 1.0)
    inline juce::Colour lighten(const juce::Colour& c, float amount)
    {
        return c.brighter(amount);
    }
}
