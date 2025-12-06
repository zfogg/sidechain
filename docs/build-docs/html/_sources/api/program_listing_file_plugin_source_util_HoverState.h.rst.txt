
.. _program_listing_file_plugin_source_util_HoverState.h:

Program Listing for File HoverState.h
=====================================

|exhale_lsh| :ref:`Return to documentation for file <file_plugin_source_util_HoverState.h>` (``plugin/source/util/HoverState.h``)

.. |exhale_lsh| unicode:: U+021B0 .. UPWARDS ARROW WITH TIP LEFTWARDS

.. code-block:: cpp

   #pragma once
   
   #include <JuceHeader.h>
   #include <functional>
   
   //==============================================================================
   class HoverState
   {
   public:
       HoverState() = default;
   
       //==========================================================================
       // State Management
   
       void setHovered(bool hovered)
       {
           if (isHoveredState != hovered)
           {
               isHoveredState = hovered;
               if (onHoverChanged)
                   onHoverChanged(isHoveredState);
           }
       }
   
       bool isHovered() const { return isHoveredState; }
   
       void reset() { isHoveredState = false; }
   
       //==========================================================================
       // Callbacks
   
       std::function<void(bool isHovered)> onHoverChanged;
   
   private:
       bool isHoveredState = false;
   };
   
   //==============================================================================
   class HoverStateWithCursor : public juce::MouseListener
   {
   public:
       HoverStateWithCursor(juce::Component* component,
                            juce::MouseCursor::StandardCursorType hoverCursor = juce::MouseCursor::PointingHandCursor)
           : targetComponent(component)
           , hoverCursorType(hoverCursor)
       {
           if (targetComponent)
               targetComponent->addMouseListener(this, false);
       }
   
       ~HoverStateWithCursor() override
       {
           if (targetComponent)
               targetComponent->removeMouseListener(this);
       }
   
       //==========================================================================
       // State
   
       bool isHovered() const { return hoverState.isHovered(); }
       void reset() { hoverState.reset(); }
   
       //==========================================================================
       // Configuration
   
       void setHoverCursor(juce::MouseCursor::StandardCursorType cursor)
       {
           hoverCursorType = cursor;
       }
   
       void setEnabled(bool enabled)
       {
           isEnabled = enabled;
           if (!enabled)
               hoverState.setHovered(false);
       }
   
       //==========================================================================
       // Callbacks
   
       std::function<void(bool isHovered)> onHoverChanged;
   
   private:
       void mouseEnter(const juce::MouseEvent&) override
       {
           if (!isEnabled)
               return;
   
           hoverState.setHovered(true);
   
           if (targetComponent)
               targetComponent->setMouseCursor(juce::MouseCursor(hoverCursorType));
   
           if (onHoverChanged)
               onHoverChanged(true);
       }
   
       void mouseExit(const juce::MouseEvent&) override
       {
           hoverState.setHovered(false);
   
           if (targetComponent)
               targetComponent->setMouseCursor(juce::MouseCursor::NormalCursor);
   
           if (onHoverChanged)
               onHoverChanged(false);
       }
   
       juce::Component* targetComponent = nullptr;
       HoverState hoverState;
       juce::MouseCursor::StandardCursorType hoverCursorType;
       bool isEnabled = true;
   
       JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HoverStateWithCursor)
   };
   
   //==============================================================================
   class MultiHoverState
   {
   public:
       MultiHoverState() = default;
   
       //==========================================================================
       // Region Management
   
       void addRegion(const juce::String& id, juce::Rectangle<int> bounds)
       {
           regions[id] = { bounds, false };
       }
   
       void updateRegion(const juce::String& id, juce::Rectangle<int> bounds)
       {
           if (regions.count(id) > 0)
               regions[id].bounds = bounds;
           else
               addRegion(id, bounds);
       }
   
       void removeRegion(const juce::String& id)
       {
           regions.erase(id);
       }
   
       void clearRegions()
       {
           regions.clear();
       }
   
       //==========================================================================
       // State Queries
   
       bool isHovered(const juce::String& id) const
       {
           auto it = regions.find(id);
           return it != regions.end() && it->second.isHovered;
       }
   
       juce::String getHoveredRegion() const
       {
           for (const auto& [id, region] : regions)
               if (region.isHovered)
                   return id;
           return {};
       }
   
       bool isAnyHovered() const
       {
           for (const auto& [id, region] : regions)
               if (region.isHovered)
                   return true;
           return false;
       }
   
       //==========================================================================
       // Updates
   
       void updateFromPoint(juce::Point<int> point)
       {
           for (auto& [id, region] : regions)
           {
               bool nowHovered = region.bounds.contains(point);
   
               if (region.isHovered != nowHovered)
               {
                   region.isHovered = nowHovered;
                   if (onRegionHoverChanged)
                       onRegionHoverChanged(id, nowHovered);
               }
           }
       }
   
       void clearAllHover()
       {
           for (auto& [id, region] : regions)
           {
               if (region.isHovered)
               {
                   region.isHovered = false;
                   if (onRegionHoverChanged)
                       onRegionHoverChanged(id, false);
               }
           }
       }
   
       //==========================================================================
       // Callbacks
   
       std::function<void(const juce::String& regionId, bool isHovered)> onRegionHoverChanged;
   
   private:
       struct Region
       {
           juce::Rectangle<int> bounds;
           bool isHovered = false;
       };
   
       std::map<juce::String, Region> regions;
   };
