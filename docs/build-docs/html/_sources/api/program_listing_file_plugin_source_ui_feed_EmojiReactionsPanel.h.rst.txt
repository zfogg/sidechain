
.. _program_listing_file_plugin_source_ui_feed_EmojiReactionsPanel.h:

Program Listing for File EmojiReactionsPanel.h
==============================================

|exhale_lsh| :ref:`Return to documentation for file <file_plugin_source_ui_feed_EmojiReactionsPanel.h>` (``plugin/source/ui/feed/EmojiReactionsPanel.h``)

.. |exhale_lsh| unicode:: U+021B0 .. UPWARDS ARROW WITH TIP LEFTWARDS

.. code-block:: cpp

   #pragma once
   
   #include <JuceHeader.h>
   #include "../../models/FeedPost.h"
   
   //==============================================================================
   class EmojiReactionsPanel : public juce::Component
   {
   public:
       EmojiReactionsPanel();
       ~EmojiReactionsPanel() override;
   
       //==============================================================================
       // Callback when an emoji is selected
       std::function<void(const juce::String& emoji)> onEmojiSelected;
   
       // Callback when panel should be dismissed
       std::function<void()> onDismiss;
   
       //==============================================================================
       void paint(juce::Graphics& g) override;
       void resized() override;
       void mouseUp(const juce::MouseEvent& event) override;
       void mouseMove(const juce::MouseEvent& event) override;
       void mouseExit(const juce::MouseEvent& event) override;
   
       //==============================================================================
       // Set the currently selected emoji (if user already reacted)
       void setSelectedEmoji(const juce::String& emoji);
   
       // Get preferred size for the panel
       static juce::Rectangle<int> getPreferredSize();
   
       //==============================================================================
       // Layout constants
       static constexpr int PANEL_HEIGHT = 50;
       static constexpr int EMOJI_SIZE = 32;
       static constexpr int EMOJI_SPACING = 8;
       static constexpr int PANEL_PADDING = 10;
   
   private:
       //==============================================================================
       juce::String selectedEmoji;  // Currently selected emoji
       int hoveredIndex = -1;       // Index of emoji being hovered (-1 = none)
   
       //==============================================================================
       // Get bounds for each emoji button
       juce::Rectangle<int> getEmojiBounds(int index) const;
   
       // Get emoji index from position (-1 if none)
       int getEmojiIndexAtPosition(juce::Point<int> pos) const;
   
       //==============================================================================
       JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EmojiReactionsPanel)
   };
   
   //==============================================================================
   class EmojiReactionsBubble : public juce::Component
   {
   public:
       EmojiReactionsBubble(juce::Component* targetComponent);
       ~EmojiReactionsBubble() override;
   
       //==============================================================================
       // Show the bubble positioned relative to the target
       void show();
   
       // Hide and destroy the bubble
       void dismiss();
   
       //==============================================================================
       // Callback when an emoji is selected
       std::function<void(const juce::String& emoji)> onEmojiSelected;
   
       //==============================================================================
       void paint(juce::Graphics& g) override;
       void resized() override;
       void mouseDown(const juce::MouseEvent& event) override;
       void inputAttemptWhenModal() override;
   
       //==============================================================================
       // Set the currently selected emoji
       void setSelectedEmoji(const juce::String& emoji);
   
   private:
       //==============================================================================
       std::unique_ptr<EmojiReactionsPanel> panel;
       juce::Component* target = nullptr;
       juce::Rectangle<int> targetBounds;
   
       // Arrow/pointer dimensions
       static constexpr int ARROW_SIZE = 8;
       static constexpr int CORNER_RADIUS = 12;
   
       //==============================================================================
       JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EmojiReactionsBubble)
   };
