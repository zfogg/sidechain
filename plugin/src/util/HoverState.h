#pragma once

#include <JuceHeader.h>
#include <functional>

// ==============================================================================
/**
 * HoverState - Mouse hover state management
 *
 * Tracks hover state for a component and provides callbacks for state changes.
 * Handles mouse cursor changes and hover-based styling.
 *
 * Usage:
 *   class MyComponent : public juce::Component {
 *       HoverState hoverState;
 *
 *       MyComponent() {
 *           hoverState.onHoverChanged = [this](bool h) { repaint(); };
 *       }
 *
 *       void mouseEnter(const juce::MouseEvent&) override {
 * hoverState.setHovered(true); } void mouseExit(const juce::MouseEvent&)
 * override { hoverState.setHovered(false); }
 *
 *       void paint(juce::Graphics& g) override {
 *           g.setColour(hoverState.isHovered() ? hoverColor : normalColor);
 *       }
 *   };
 */
class HoverState {
public:
  HoverState() = default;

  // ==========================================================================
  // State Management

  /**
   * Set hover state and trigger callback if changed.
   */
  void setHovered(bool hovered) {
    if (isHoveredState != hovered) {
      isHoveredState = hovered;
      if (onHoverChanged)
        onHoverChanged(isHoveredState);
    }
  }

  /**
   * Check if currently hovered.
   */
  bool isHovered() const {
    return isHoveredState;
  }

  /**
   * Reset hover state without triggering callback.
   */
  void reset() {
    isHoveredState = false;
  }

  // ==========================================================================
  // Callbacks

  /**
   * Called when hover state changes.
   */
  std::function<void(bool isHovered)> onHoverChanged;

private:
  bool isHoveredState = false;
};

// ==============================================================================
/**
 * HoverStateWithCursor - Hover state with automatic cursor management
 *
 * Extends HoverState to automatically change the mouse cursor on hover.
 *
 * Usage:
 *   HoverStateWithCursor hoverState(this,
 * juce::MouseCursor::PointingHandCursor);
 *   // mouseEnter/mouseExit automatically handled
 */
class HoverStateWithCursor : public juce::MouseListener {
public:
  /**
   * Create hover state with cursor management.
   * @param component The component to track hover for
   * @param hoverCursor Cursor to show on hover (default: pointing hand)
   */
  HoverStateWithCursor(juce::Component *component,
                       juce::MouseCursor::StandardCursorType hoverCursor = juce::MouseCursor::PointingHandCursor)
      : targetComponent(component), hoverCursorType(hoverCursor) {
    if (targetComponent)
      targetComponent->addMouseListener(this, false);
  }

  ~HoverStateWithCursor() override {
    if (targetComponent)
      targetComponent->removeMouseListener(this);
  }

  // ==========================================================================
  // State

  bool isHovered() const {
    return hoverState.isHovered();
  }
  void reset() {
    hoverState.reset();
  }

  // ==========================================================================
  // Configuration

  void setHoverCursor(juce::MouseCursor::StandardCursorType cursor) {
    hoverCursorType = cursor;
  }

  void setEnabled(bool enabled) {
    isEnabled = enabled;
    if (!enabled)
      hoverState.setHovered(false);
  }

  // ==========================================================================
  // Callbacks

  std::function<void(bool isHovered)> onHoverChanged;

private:
  void mouseEnter(const juce::MouseEvent &) override {
    if (!isEnabled)
      return;

    hoverState.setHovered(true);

    if (targetComponent)
      targetComponent->setMouseCursor(juce::MouseCursor(hoverCursorType));

    if (onHoverChanged)
      onHoverChanged(true);
  }

  void mouseExit(const juce::MouseEvent &) override {
    hoverState.setHovered(false);

    if (targetComponent)
      targetComponent->setMouseCursor(juce::MouseCursor::NormalCursor);

    if (onHoverChanged)
      onHoverChanged(false);
  }

  juce::Component *targetComponent = nullptr;
  HoverState hoverState;
  juce::MouseCursor::StandardCursorType hoverCursorType;
  bool isEnabled = true;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HoverStateWithCursor)
};

// ==============================================================================
/**
 * MultiHoverState - Track hover state for multiple regions
 *
 * Useful when a component has multiple interactive areas.
 *
 * Usage:
 *   MultiHoverState hovers;
 *   hovers.addRegion("like", likeBounds);
 *   hovers.addRegion("comment", commentBounds);
 *   hovers.onRegionHoverChanged = [this](const String& id, bool h) { repaint();
 * };
 *
 *   void mouseMove(const MouseEvent& e) {
 * hovers.updateFromPoint(e.getPosition()); }
 */
class MultiHoverState {
public:
  MultiHoverState() = default;

  // ==========================================================================
  // Region Management

  void addRegion(const juce::String &id, juce::Rectangle<int> bounds) {
    regions[id] = {bounds, false};
  }

  void updateRegion(const juce::String &id, juce::Rectangle<int> bounds) {
    if (regions.count(id) > 0)
      regions[id].bounds = bounds;
    else
      addRegion(id, bounds);
  }

  void removeRegion(const juce::String &id) {
    regions.erase(id);
  }

  void clearRegions() {
    regions.clear();
  }

  // ==========================================================================
  // State Queries

  bool isHovered(const juce::String &id) const {
    auto it = regions.find(id);
    return it != regions.end() && it->second.isHovered;
  }

  juce::String getHoveredRegion() const {
    for (const auto &[id, region] : regions)
      if (region.isHovered)
        return id;
    return {};
  }

  bool isAnyHovered() const {
    for (const auto &[id, region] : regions)
      if (region.isHovered)
        return true;
    return false;
  }

  // ==========================================================================
  // Updates

  void updateFromPoint(juce::Point<int> point) {
    for (auto &[id, region] : regions) {
      bool nowHovered = region.bounds.contains(point);

      if (region.isHovered != nowHovered) {
        region.isHovered = nowHovered;
        if (onRegionHoverChanged)
          onRegionHoverChanged(id, nowHovered);
      }
    }
  }

  void clearAllHover() {
    for (auto &[id, region] : regions) {
      if (region.isHovered) {
        region.isHovered = false;
        if (onRegionHoverChanged)
          onRegionHoverChanged(id, false);
      }
    }
  }

  // ==========================================================================
  // Callbacks

  std::function<void(const juce::String &regionId, bool isHovered)> onRegionHoverChanged;

private:
  struct Region {
    juce::Rectangle<int> bounds;
    bool isHovered = false;
  };

  std::map<juce::String, Region> regions;
};
