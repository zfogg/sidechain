import { Page, Locator } from '@playwright/test'

/**
 * NotificationsPage - Notification center
 * Encapsulates interactions with the Notifications page
 */
export class NotificationsPage {
  readonly page: Page
  readonly heading: Locator
  readonly allTab: Locator
  readonly unreadTab: Locator
  readonly unreadBadge: Locator
  readonly markAllReadButton: Locator
  readonly clearAllButton: Locator
  readonly notificationItems: Locator
  readonly loadingSpinner: Locator
  readonly emptyState: Locator
  readonly errorState: Locator
  readonly loadMoreButton: Locator
  readonly retryButton: Locator

  constructor(page: Page) {
    this.page = page
    this.heading = page.locator('h1:has-text("Notifications")')
    this.allTab = page.locator('button:has-text("All")').first()
    this.unreadTab = page.locator('button:has-text("Unread")').first()
    this.unreadBadge = page.locator('.bg-coral-pink.text-white')
    this.markAllReadButton = page.locator('button:has-text("Mark all as read")')
    this.clearAllButton = page.locator('button:has-text("Clear all")')
    this.notificationItems = page.locator('[class*="notification"], [data-testid*="notification"]')
    this.loadingSpinner = page.locator('[data-testid="spinner"], .animate-spin')
    this.emptyState = page.locator('text=/You\'re all caught up|No notifications yet/i')
    this.errorState = page.locator('text=/Failed to load notifications/i')
    this.loadMoreButton = page.locator('button:has-text("Load More")')
    this.retryButton = page.locator('button:has-text("Retry")')
  }

  /**
   * Navigate to notifications page
   */
  async goto(): Promise<void> {
    await this.page.goto('/notifications')
    await this.page.waitForLoadState('networkidle')
  }

  /**
   * Check if page has loaded
   */
  async isLoaded(): Promise<boolean> {
    return await this.heading.isVisible().catch(() => false)
  }

  /**
   * Switch to all notifications filter
   */
  async switchToAll(): Promise<void> {
    await this.allTab.click()
    await this.page.waitForTimeout(500)
  }

  /**
   * Switch to unread notifications filter
   */
  async switchToUnread(): Promise<void> {
    await this.unreadTab.click()
    await this.page.waitForTimeout(500)
  }

  /**
   * Get unread count from badge
   */
  async getUnreadCount(): Promise<number> {
    const text = await this.unreadBadge.textContent().catch(() => '0')
    const match = text.match(/(\d+)/)
    return match ? parseInt(match[1], 10) : 0
  }

  /**
   * Check if unread badge is visible
   */
  async hasUnreadBadge(): Promise<boolean> {
    return await this.unreadBadge.isVisible().catch(() => false)
  }

  /**
   * Get number of visible notification items
   */
  async getNotificationCount(): Promise<number> {
    return await this.notificationItems.count()
  }

  /**
   * Check if loading spinner is visible
   */
  async isLoading(): Promise<boolean> {
    return await this.loadingSpinner.isVisible().catch(() => false)
  }

  /**
   * Check if empty state is shown
   */
  async hasEmptyState(): Promise<boolean> {
    return await this.emptyState.isVisible().catch(() => false)
  }

  /**
   * Check if error state is shown
   */
  async hasError(): Promise<boolean> {
    return await this.errorState.isVisible().catch(() => false)
  }

  /**
   * Check if mark all as read button is visible
   */
  async hasMarkAllReadButton(): Promise<boolean> {
    return await this.markAllReadButton.isVisible().catch(() => false)
  }

  /**
   * Click mark all as read button
   */
  async clickMarkAllAsRead(): Promise<void> {
    await this.markAllReadButton.click()
  }

  /**
   * Click clear all button
   */
  async clickClearAll(): Promise<void> {
    await this.clearAllButton.click()
    // Handle confirmation dialog if it appears
    await this.page.on('dialog', async (dialog) => {
      if (dialog.type() === 'confirm') {
        await dialog.accept()
      }
    })
  }

  /**
   * Check if load more button is visible
   */
  async hasLoadMoreButton(): Promise<boolean> {
    return await this.loadMoreButton.isVisible().catch(() => false)
  }

  /**
   * Click load more button
   */
  async clickLoadMore(): Promise<void> {
    await this.loadMoreButton.click()
  }

  /**
   * Click retry button (on error)
   */
  async clickRetry(): Promise<void> {
    await this.retryButton.click()
  }

  /**
   * Get a specific notification item by index
   */
  async getNotificationItem(index: number): Promise<NotificationItemCard> {
    const item = this.notificationItems.nth(index)
    await item.isVisible()
    return new NotificationItemCard(this.page, item)
  }

  /**
   * Check if all tab is active
   */
  async isAllTabActive(): Promise<boolean> {
    const classes = await this.allTab
      .evaluate((el) => el.className)
      .catch(() => '')
    return classes.includes('default')
  }

  /**
   * Check if unread tab is active
   */
  async isUnreadTabActive(): Promise<boolean> {
    const classes = await this.unreadTab
      .evaluate((el) => el.className)
      .catch(() => '')
    return classes.includes('default')
  }

  /**
   * Wait for notifications to load
   */
  async waitForNotificationsLoad(timeout: number = 5000): Promise<void> {
    await Promise.race([
      this.notificationItems.first().waitFor({ timeout }),
      this.emptyState.waitFor({ timeout }),
      this.errorState.waitFor({ timeout }),
    ]).catch(() => null)
  }

  /**
   * Check if notifications are visible (not empty)
   */
  async hasNotifications(): Promise<boolean> {
    return (await this.getNotificationCount()) > 0
  }
}

/**
 * NotificationItemCard - Represents a single notification item
 */
export class NotificationItemCard {
  readonly page: Page
  readonly element: Locator
  readonly markAsReadButton: Locator
  readonly deleteButton: Locator
  readonly actionLink: Locator
  readonly content: Locator

  constructor(page: Page, element: Locator) {
    this.page = page
    this.element = element
    this.markAsReadButton = element.locator('button[aria-label*="read" i], button:has-text("✓")')
    this.deleteButton = element.locator('button[aria-label*="delete" i], button:has-text("🗑")')
    this.actionLink = element.locator('a').first()
    this.content = element.locator('p, div').first()
  }

  /**
   * Mark notification as read
   */
  async markAsRead(): Promise<void> {
    try {
      await this.markAsReadButton.click()
    } catch (e) {
      // Button might not exist
    }
  }

  /**
   * Delete notification
   */
  async delete(): Promise<void> {
    try {
      await this.deleteButton.click()
    } catch (e) {
      // Button might not exist
    }
  }

  /**
   * Click notification to navigate to related content
   */
  async clickNotification(): Promise<void> {
    try {
      await this.actionLink.click()
    } catch (e) {
      // Link might not exist
    }
  }

  /**
   * Get notification content
   */
  async getContent(): Promise<string> {
    return await this.content.textContent().then((t) => t?.trim() || '')
  }

  /**
   * Check if notification is visible
   */
  async isVisible(): Promise<boolean> {
    return await this.element.isVisible().catch(() => false)
  }

  /**
   * Check if notification is unread (might have visual indicator)
   */
  async isUnread(): Promise<boolean> {
    const classes = await this.element.evaluate((el) => el.className).catch(() => '')
    return classes.includes('unread') || classes.includes('font-bold')
  }
}
