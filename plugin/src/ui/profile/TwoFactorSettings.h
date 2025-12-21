#pragma once

#include "../common/AppStoreComponent.h"
#include <JuceHeader.h>

class NetworkClient;

namespace Sidechain {
namespace Stores {
class AppStore;
struct AuthState;
} // namespace Stores
} // namespace Sidechain

using namespace Sidechain::UI;
using namespace Sidechain::Stores;

// ==============================================================================
/**
 * TwoFactorSettings provides a UI for managing two-factor authentication
 *
 * Features:
 * - View current 2FA status
 * - Enable 2FA (TOTP for apps, HOTP for YubiKey)
 * - Display setup QR code URL and manual secret
 * - Show and copy backup codes
 * - Disable 2FA
 * - Regenerate backup codes
 */
class TwoFactorSettings : public AppStoreComponent<AuthState>, public juce::Button::Listener {
public:
  TwoFactorSettings(AppStore *store = nullptr);
  ~TwoFactorSettings() override;

  // ==============================================================================
  // Data binding
  void setNetworkClient(NetworkClient *client) {
    networkClient = client;
  }
  void loadStatus();

  // ==============================================================================
  // Callbacks
  std::function<void()> onClose;

  // ==============================================================================
  // Modal display
  void showModal(juce::Component *parentComponent);
  void closeDialog();

  // ==============================================================================
  // Component overrides
  void paint(juce::Graphics &g) override;
  void resized() override;
  void buttonClicked(juce::Button *button) override;

protected:
  // ==============================================================================
  // AppStoreComponent overrides
  void onAppStateChanged(const AuthState &state) override;
  void subscribeToAppStore();

private:
  // ==============================================================================
  // State enum
  enum class State {
    Loading,
    Disabled,      // 2FA not enabled, show enable options
    SetupType,     // User is choosing TOTP vs HOTP
    SetupPassword, // User entering password to enable
    SetupQR,       // Showing QR/secret for setup
    SetupVerify,   // User entering verification code
    Enabled,       // 2FA enabled, show status and options
    Disabling,     // User entering code/password to disable
    Error
  };

  State currentState = State::Loading;
  NetworkClient *networkClient = nullptr;

  // ==============================================================================
  // Status data
  bool twoFactorEnabled = false;
  juce::String twoFactorType;
  int backupCodesRemaining = 0;

  // Setup data
  juce::String setupType; // "totp" or "hotp"
  juce::String setupSecret;
  juce::String setupQRUrl;
  juce::StringArray backupCodes;

  // Error/status messages
  juce::String errorMessage;
  juce::String statusMessage;
  bool isProcessing = false;

  // ==============================================================================
  // UI Components
  std::unique_ptr<juce::TextButton> closeButton;
  std::unique_ptr<juce::TextButton> enableButton;
  std::unique_ptr<juce::TextButton> totpButton;
  std::unique_ptr<juce::TextButton> hotpButton;
  std::unique_ptr<juce::TextButton> backButton;
  std::unique_ptr<juce::TextButton> disableButton;
  std::unique_ptr<juce::TextButton> regenerateButton;
  std::unique_ptr<juce::TextButton> copySecretButton;
  std::unique_ptr<juce::TextButton> copyCodesButton;
  std::unique_ptr<juce::TextButton> verifyButton;
  std::unique_ptr<juce::TextButton> confirmDisableButton;

  std::unique_ptr<juce::TextEditor> passwordInput;
  std::unique_ptr<juce::TextEditor> codeInput;

  // ==============================================================================
  // State handlers
  void showDisabledState();
  void showTypeSelection();
  void showPasswordEntry();
  void showSetupQR();
  void showVerifyEntry();
  void showEnabledState();
  void showDisablingEntry();
  void showError(const juce::String &message);

  // ==============================================================================
  // Actions
  void startEnable(const juce::String &type);
  void submitPassword();
  void submitVerificationCode();
  void startDisable();
  void submitDisableCode();
  void doRegenerateBackupCodes();

  // ==============================================================================
  // Drawing methods
  void drawHeader(juce::Graphics &g, juce::Rectangle<int> bounds);
  void drawStatus(juce::Graphics &g, juce::Rectangle<int> bounds);
  void drawSetupInfo(juce::Graphics &g, juce::Rectangle<int> bounds);
  void drawBackupCodes(juce::Graphics &g, juce::Rectangle<int> bounds);

  // ==============================================================================
  // Helpers
  void copyToClipboard(const juce::String &text);
  void hideAllInputs();

  // ==============================================================================
  // Layout constants
  static constexpr int HEADER_HEIGHT = 60;
  static constexpr int BUTTON_HEIGHT = 45;
  static constexpr int INPUT_HEIGHT = 45;
  static constexpr int PADDING = 25;
  static constexpr int SPACING = 15;

  // ==============================================================================
  // Colors
  struct Colors {
    static inline juce::Colour background{0xff1a1a1e};
    static inline juce::Colour headerBg{0xff252529};
    static inline juce::Colour textPrimary{0xffffffff};
    static inline juce::Colour textSecondary{0xffa0a0a0};
    static inline juce::Colour accent{0xff00d4ff};
    static inline juce::Colour accentDark{0xff0099bb};
    static inline juce::Colour successGreen{0xff2ed573};
    static inline juce::Colour warningOrange{0xffffa502};
    static inline juce::Colour errorRed{0xffff4757};
    static inline juce::Colour buttonBg{0xff2d2d32};
    static inline juce::Colour buttonHover{0xff3d3d42};
    static inline juce::Colour inputBg{0xff252529};
    static inline juce::Colour inputBorder{0xff4a4a4e};
    static inline juce::Colour closeButton{0xff3a3a3e};
    static inline juce::Colour codeBackground{0xff1e1e22};
  };

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TwoFactorSettings)
};
