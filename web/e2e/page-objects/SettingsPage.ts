import { Page, Locator } from '@playwright/test'

/**
 * SettingsPage - User settings page
 * Encapsulates interactions with the Settings page
 */
export class SettingsPage {
  readonly page: Page
  readonly profileTab: Locator
  readonly privacyTab: Locator
  readonly audioTab: Locator
  readonly notificationsTab: Locator
  readonly appearanceTab: Locator
  readonly accountTab: Locator
  readonly saveButtons: Locator
  readonly logoutButton: Locator
  readonly inputs: Locator
  readonly toggles: Locator

  constructor(page: Page) {
    this.page = page
    this.profileTab = page.locator('button[value="profile"], button:has-text("Profile")').first()
    this.privacyTab = page.locator('button[value="privacy"], button:has-text("Privacy")').first()
    this.audioTab = page.locator('button[value="audio"], button:has-text("Audio")').first()
    this.notificationsTab = page.locator('button[value="notifications"], button:has-text("Notifications")').first()
    this.appearanceTab = page.locator('button[value="appearance"], button:has-text("Appearance")').first()
    this.accountTab = page.locator('button[value="account"], button:has-text("Account")').first()
    this.saveButtons = page.locator('button:has-text("Save")')
    this.logoutButton = page.locator('button:has-text("Logout"), button:has-text("Sign Out")')
    this.inputs = page.locator('input[type="text"], textarea')
    this.toggles = page.locator('input[type="checkbox"], [role="switch"]')
  }

  /**
   * Navigate to settings page
   */
  async goto(): Promise<void> {
    await this.page.goto('/settings')
    await this.page.waitForLoadState('domcontentloaded')
  }

  /**
   * Check if settings page loaded
   */
  async isLoaded(): Promise<boolean> {
    return await this.profileTab.isVisible().catch(() => false)
  }

  /**
   * Switch to a settings tab
   */
  async switchTab(tab: 'profile' | 'privacy' | 'audio' | 'notifications' | 'appearance' | 'account'): Promise<void> {
    const tabMap = {
      'profile': this.profileTab,
      'privacy': this.privacyTab,
      'audio': this.audioTab,
      'notifications': this.notificationsTab,
      'appearance': this.appearanceTab,
      'account': this.accountTab,
    }
    await tabMap[tab].click()
    // REMOVED: waitForTimeout
  }

  /**
   * Check if a tab is active
   */
  async isTabActive(tab: 'profile' | 'privacy' | 'audio' | 'notifications' | 'appearance' | 'account'): Promise<boolean> {
    const tabMap = {
      'profile': this.profileTab,
      'privacy': this.privacyTab,
      'audio': this.audioTab,
      'notifications': this.notificationsTab,
      'appearance': this.appearanceTab,
      'account': this.accountTab,
    }
    return await tabMap[tab]
      .evaluate((el) => el.className.includes('active') || el.getAttribute('data-state') === 'active')
      .catch(() => false)
  }

  /**
   * Get number of settings inputs
   */
  async getInputCount(): Promise<number> {
    return await this.inputs.count()
  }

  /**
   * Fill a setting input by index
   */
  async fillInput(index: number, value: string): Promise<void> {
    await this.inputs.nth(index).fill(value)
  }

  /**
   * Get input value by index
   */
  async getInputValue(index: number): Promise<string> {
    return await this.inputs.nth(index).inputValue()
  }

  /**
   * Toggle a switch/checkbox by index
   */
  async toggleSwitch(index: number): Promise<void> {
    await this.toggles.nth(index).click()
  }

  /**
   * Check number of toggles
   */
  async getToggleCount(): Promise<number> {
    return await this.toggles.count()
  }

  /**
   * Click save button for current tab
   */
  async clickSave(): Promise<void> {
    const saveButton = this.page.locator('button:has-text("Save")').last()
    await saveButton.click()
  }

  /**
   * Check if save button is visible
   */
  async hasSaveButton(): Promise<boolean> {
    return await this.saveButtons.first().isVisible().catch(() => false)
  }

  /**
   * Click logout button
   */
  async clickLogout(): Promise<void> {
    await this.logoutButton.click()
  }

  /**
   * Check if logout button is visible
   */
  async hasLogoutButton(): Promise<boolean> {
    return await this.logoutButton.isVisible().catch(() => false)
  }

  /**
   * Get active tab name
   */
  async getActiveTab(): Promise<string | null> {
    const tabs = ['profile', 'privacy', 'audio', 'notifications', 'appearance', 'account'] as const

    for (const tab of tabs) {
      if (await this.isTabActive(tab)) {
        return tab
      }
    }
    return null
  }

  /**
   * Check all tabs are present
   */
  async allTabsPresent(): Promise<boolean> {
    const profileVis = await this.profileTab.isVisible().catch(() => false)
    const privacyVis = await this.privacyTab.isVisible().catch(() => false)
    const audioVis = await this.audioTab.isVisible().catch(() => false)
    const notifVis = await this.notificationsTab.isVisible().catch(() => false)
    const appearanceVis = await this.appearanceTab.isVisible().catch(() => false)
    const accountVis = await this.accountTab.isVisible().catch(() => false)

    return profileVis && privacyVis && audioVis && notifVis && appearanceVis && accountVis
  }
}
