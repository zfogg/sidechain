#pragma once

#include "../common/AppStoreComponent.h"
#include <JuceHeader.h>

class NetworkClient;

namespace Sidechain {
namespace Stores {
class AppStore;
struct UserState;
} // namespace Stores
} // namespace Sidechain

using namespace Sidechain::UI;
using namespace Sidechain::Stores;

//==============================================================================
/**
 * ActivityStatusSettings provides a UI for managing activity status privacy
 * settings
 *
 * Features:
 * - Toggle to show/hide online status to other users
 * - Toggle to show/hide "last active" time to other users
 * - All changes are persisted to the backend
 * - Loads current preferences on open
 */
class ActivityStatusSettings : public AppStoreComponent<UserState>, public juce::Button::Listener {
public:
  ActivityStatusSettings(AppStore *store = nullptr);
  ~ActivityStatusSettings() override;

  //==============================================================================
  // Data binding
  void setNetworkClient(NetworkClient *client) {
    networkClient = client;
  }
  void loadSettings();

  //==============================================================================
  // Modal dialog methods
  void showModal(juce::Component *parentComponent);
  void closeDialog();

  //==============================================================================
  // Callbacks
  std::function<void()> onClose;

  //==============================================================================
  // Component overrides
  void paint(juce::Graphics &g) override;
  void resized() override;
  void buttonClicked(juce::Button *button) override;

protected:
  //==============================================================================
  // AppStoreComponent overrides
  void onAppStateChanged(const UserState &state) override;
  void subscribeToAppStore() override;

private:
  //==============================================================================
  NetworkClient *networkClient = nullptr;

  // State
  bool isLoading = false;
  bool isSaving = false;
  juce::String errorMessage;

  // Settings state
  bool showActivityStatus = true;
  bool showLastActive = true;

  //==============================================================================
  // UI Components
  std::unique_ptr<juce::TextButton> closeButton;

  // Toggle buttons for each setting
  std::unique_ptr<juce::ToggleButton> showActivityStatusToggle;
  std::unique_ptr<juce::ToggleButton> showLastActiveToggle;

  //==============================================================================
  // Drawing methods
  void drawHeader(juce::Graphics &g, juce::Rectangle<int> bounds);
  void drawDescription(juce::Graphics &g, juce::Rectangle<int> bounds, const juce::String &text);

  //==============================================================================
  // Helpers
  void setupToggles();
  void populateFromSettings();
  void handleToggleChange(juce::ToggleButton *toggle);
  void saveSettings();

  //==============================================================================
  // Layout constants
  static constexpr int HEADER_HEIGHT = 60;
  static constexpr int TOGGLE_HEIGHT = 50;
  static constexpr int DESCRIPTION_HEIGHT = 40;
  static constexpr int PADDING = 25;

  //==============================================================================
  // Colors (matching NotificationSettings style)
  struct Colors {
    static inline juce::Colour background{0xff1a1a1e};
    static inline juce::Colour headerBg{0xff252529};
    static inline juce::Colour textPrimary{0xffffffff};
    static inline juce::Colour textSecondary{0xffa0a0a0};
    static inline juce::Colour accent{0xff00d4ff};
    static inline juce::Colour toggleBg{0xff2d2d32};
    static inline juce::Colour toggleBorder{0xff4a4a4e};
    static inline juce::Colour errorRed{0xffff4757};
    static inline juce::Colour closeButton{0xff3a3a3e};
  };

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ActivityStatusSettings)
};
