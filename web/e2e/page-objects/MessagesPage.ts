import { Page, Locator } from '@playwright/test'

/**
 * MessagesPage - Direct messaging page with Stream.io integration
 * Simplified page object focusing on navigational elements
 */
export class MessagesPage {
  readonly page: Page
  readonly channelListContainer: Locator
  readonly messageThreadContainer: Locator
  readonly emptyStateHeading: Locator
  readonly newConversationButton: Locator
  readonly newMessageDialog: Locator
  readonly loadingSpinner: Locator
  readonly errorMessage: Locator

  constructor(page: Page) {
    this.page = page
    this.channelListContainer = page.locator('[class*="channel"], [class*="conversation"]').first()
    this.messageThreadContainer = page.locator('[class*="message"], [class*="thread"]').nth(1)
    this.emptyStateHeading = page.locator('text=/No conversation selected|Failed to load/i')
    this.newConversationButton = page.locator('button:has-text("Start New Conversation")')
    this.newMessageDialog = page.locator('[role="dialog"], .fixed.inset-0').last()
    this.loadingSpinner = page.locator('[data-testid="spinner"], .animate-spin')
    this.errorMessage = page.locator('text=/Failed to load conversation/i')
  }

  /**
   * Navigate to messages page
   */
  async goto(): Promise<void> {
    await this.page.goto('/messages')
    await this.page.waitForLoadState('domcontentloaded')
  }

  /**
   * Navigate to specific conversation
   */
  async gotoConversation(channelId: string): Promise<void> {
    await this.page.goto(`/messages/${channelId}`)
    await this.page.waitForLoadState('domcontentloaded')
  }

  /**
   * Check if page has loaded
   */
  async isLoaded(): Promise<boolean> {
    return await this.page.locator('body').isVisible().catch(() => false)
  }

  /**
   * Check if channel list is visible
   */
  async isChannelListVisible(): Promise<boolean> {
    return await this.channelListContainer.isVisible().catch(() => false)
  }

  /**
   * Check if empty state is shown
   */
  async hasEmptyState(): Promise<boolean> {
    return await this.emptyStateHeading.isVisible().catch(() => false)
  }

  /**
   * Check if message thread is visible
   */
  async isMessageThreadVisible(): Promise<boolean> {
    return await this.messageThreadContainer.isVisible().catch(() => false)
  }

  /**
   * Check if loading spinner is visible
   */
  async isLoading(): Promise<boolean> {
    return await this.loadingSpinner.isVisible().catch(() => false)
  }

  /**
   * Check if error is shown
   */
  async hasError(): Promise<boolean> {
    return await this.errorMessage.isVisible().catch(() => false)
  }

  /**
   * Click new conversation button
   */
  async clickNewConversation(): Promise<void> {
    await this.newConversationButton.click()
  }

  /**
   * Check if new message dialog is open
   */
  async isNewMessageDialogOpen(): Promise<boolean> {
    return await this.newMessageDialog.isVisible().catch(() => false)
  }

  /**
   * Close new message dialog
   */
  async closeNewMessageDialog(): Promise<void> {
    // Try to find close button or press Escape
    const closeButton = this.page.locator('button[aria-label*="close" i], button:has-text("✕")').last()
    const isVisible = await closeButton.isVisible().catch(() => false)

    if (isVisible) {
      await closeButton.click()
    } else {
      await this.page.keyboard.press('Escape')
    }
  }

  /**
   * Get number of channels in list
   */
  async getChannelCount(): Promise<number> {
    return await this.page.locator('[class*="channel-item"], [class*="conversation"]').count()
  }

  /**
   * Click on a specific channel by index
   */
  async clickChannel(index: number): Promise<void> {
    const channels = this.page.locator('[class*="channel-item"], [class*="conversation-item"]')
    await channels.nth(index).click()
  }

  /**
   * Wait for message thread to load
   */
  async waitForThreadLoad(timeout: number = 5000): Promise<void> {
    await Promise.race([
      this.messageThreadContainer.waitFor({ timeout }),
      this.errorMessage.waitFor({ timeout }),
    ]).catch(() => null)
  }

  /**
   * Check if any conversations are available
   */
  async hasConversations(): Promise<boolean> {
    const count = await this.getChannelCount()
    return count > 0
  }
}
