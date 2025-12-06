
.. _program_listing_file_plugin_source_ui_common_ConnectionIndicator.h:

Program Listing for File ConnectionIndicator.h
==============================================

|exhale_lsh| :ref:`Return to documentation for file <file_plugin_source_ui_common_ConnectionIndicator.h>` (``plugin/source/ui/common/ConnectionIndicator.h``)

.. |exhale_lsh| unicode:: U+021B0 .. UPWARDS ARROW WITH TIP LEFTWARDS

.. code-block:: cpp

   #pragma once
   
   #include <JuceHeader.h>
   #include "../../network/NetworkClient.h"
   
   //==============================================================================
   class ConnectionIndicator : public juce::Component,
                               public juce::TooltipClient
   {
   public:
       ConnectionIndicator()
       {
           setSize(16, 16);
       }
   
       void setStatus(NetworkClient::ConnectionStatus newStatus)
       {
           if (status != newStatus)
           {
               status = newStatus;
               repaint();
           }
       }
   
       NetworkClient::ConnectionStatus getStatus() const { return status; }
   
       void paint(juce::Graphics& g) override
       {
           auto bounds = getLocalBounds().toFloat().reduced(2.0f);
   
           // Draw shadow/glow effect
           juce::Colour glowColor;
           juce::Colour mainColor;
   
           switch (status)
           {
               case NetworkClient::ConnectionStatus::Connected:
                   mainColor = juce::Colour(0xff00c853);  // Material green
                   glowColor = mainColor.withAlpha(0.4f);
                   break;
               case NetworkClient::ConnectionStatus::Connecting:
                   mainColor = juce::Colour(0xffffd600);  // Material amber
                   glowColor = mainColor.withAlpha(0.4f);
                   break;
               case NetworkClient::ConnectionStatus::Disconnected:
               default:
                   mainColor = juce::Colour(0xffff1744);  // Material red
                   glowColor = mainColor.withAlpha(0.4f);
                   break;
           }
   
           // Draw glow
           g.setColour(glowColor);
           g.fillEllipse(bounds.expanded(1.0f));
   
           // Draw main dot
           g.setColour(mainColor);
           g.fillEllipse(bounds);
   
           // Draw highlight
           g.setColour(juce::Colours::white.withAlpha(0.3f));
           auto highlightBounds = bounds.reduced(bounds.getWidth() * 0.2f);
           highlightBounds.setY(highlightBounds.getY() - bounds.getHeight() * 0.1f);
           g.fillEllipse(highlightBounds.removeFromTop(highlightBounds.getHeight() * 0.4f));
       }
   
       juce::String getTooltip() override
       {
           switch (status)
           {
               case NetworkClient::ConnectionStatus::Connected:
                   return "Connected to Sidechain servers";
               case NetworkClient::ConnectionStatus::Connecting:
                   return "Connecting to servers...";
               case NetworkClient::ConnectionStatus::Disconnected:
               default:
                   return "Disconnected - Check your internet connection";
           }
       }
   
       void mouseDown(const juce::MouseEvent&) override
       {
           // Trigger reconnection attempt when clicked while disconnected
           if (onReconnectClicked && status == NetworkClient::ConnectionStatus::Disconnected)
           {
               onReconnectClicked();
           }
       }
   
       std::function<void()> onReconnectClicked;
   
   private:
       NetworkClient::ConnectionStatus status = NetworkClient::ConnectionStatus::Disconnected;
   
       JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ConnectionIndicator)
   };
