import { Page, Locator, expect } from '@playwright/test'

/**
 * DiscoveryPage - Gorse-powered recommendations and discovery page
 * Encapsulates interactions with the Discovery page (4 tabs: For You, Trending, Producers, Genres)
 */
export class DiscoveryPage {
  readonly page: Page
  readonly heading: Locator
  readonly forYouTab: Locator
  readonly trendingTab: Locator
  readonly producersTab: Locator
  readonly genresTab: Locator
  readonly loadingSpinner: Locator
  readonly postCards: Locator
  readonly producerCards: Locator
  readonly emptyState: Locator
  readonly timeWindowButtons: Locator
  readonly recommendationReason: Locator

  constructor(page: Page) {
    this.page = page
    this.heading = page.locator('h1:has-text("Discover")')
    this.forYouTab = page.locator('button:has-text("For You")')
    this.trendingTab = page.locator('button:has-text("Trending")')
    this.producersTab = page.locator('button:has-text("Producers")')
    this.genresTab = page.locator('button:has-text("Genres")')
    this.loadingSpinner = page.locator('[data-testid="spinner"], .animate-spin')
    this.postCards = page.locator('[data-testid*="post-card"], .group:has(> :nth-child(1))')
    this.producerCards = page.locator('[data-testid="producer-card"], .grid > [class*="card"]')
    this.emptyState = page.locator('text=/coming soon|no.*yet|recommendations|producers/i')
    this.timeWindowButtons = page.locator('button:has-text("Today"), button:has-text("This Week"), button:has-text("This Month")')
    this.recommendationReason = page.locator('text=/💡/').locator('..')
  }

  /**
   * Navigate to discovery page
   */
  async goto(): Promise<void> {
    await this.page.goto('/discover')
    await this.waitForPageLoad()
  }

  /**
   * Wait for discovery page to load
   */
  async waitForPageLoad(timeout: number = 10000): Promise<void> {
    await expect(this.heading).toBeVisible({ timeout })
  }

  /**
   * Check if page has loaded
   */
  async isLoaded(): Promise<boolean> {
    return await this.heading.isVisible().catch(() => false)
  }

  /**
   * Switch to a discovery tab
   */
  async switchTab(tab: 'for-you' | 'trending' | 'producers' | 'genres'): Promise<void> {
    const tabMap = {
      'for-you': this.forYouTab,
      'trending': this.trendingTab,
      'producers': this.producersTab,
      'genres': this.genresTab,
    }
    await tabMap[tab].click()
    await this.waitForTabLoad()
  }

  /**
   * Get active tab name
   */
  async getActiveTab(): Promise<string | null> {
    const tabs = [
      { name: 'for-you', locator: this.forYouTab },
      { name: 'trending', locator: this.trendingTab },
      { name: 'producers', locator: this.producersTab },
      { name: 'genres', locator: this.genresTab },
    ]

    for (const tab of tabs) {
      const isActive = await tab.locator
        .evaluate((el: Element) => el.className.includes('default') || el.getAttribute('data-state') === 'active')
        .catch(() => false)
      if (isActive) return tab.name
    }
    return null
  }

  /**
   * Check if tab is active
   */
  async isTabActive(tab: 'for-you' | 'trending' | 'producers' | 'genres'): Promise<boolean> {
    const tabMap = {
      'for-you': this.forYouTab,
      'trending': this.trendingTab,
      'producers': this.producersTab,
      'genres': this.genresTab,
    }
    return await tabMap[tab]
      .evaluate((el: Element) => el.className.includes('default'))
      .catch(() => false)
  }

  /**
   * Wait for tab content to load
   */
  async waitForTabLoad(timeout: number = 10000): Promise<void> {
    // Wait for spinner to disappear or content to appear
    await Promise.race([
      this.loadingSpinner.waitFor({ state: 'hidden', timeout }),
      this.page.locator('h2').filter({ hasText: /Recommended|Trending|Producers|Genre/ }).waitFor({ timeout }),
    ]).catch(() => null)
  }

  /**
   * Check if loading spinner is visible
   */
  async isLoading(): Promise<boolean> {
    return await this.loadingSpinner.isVisible().catch(() => false)
  }

  /**
   * Get number of posts displayed
   */
  async getPostCount(): Promise<number> {
    return await this.postCards.count()
  }

  /**
   * Get number of producers displayed
   */
  async getProducerCount(): Promise<number> {
    return await this.producerCards.count()
  }

  /**
   * Check if empty state is shown
   */
  async hasEmptyState(): Promise<boolean> {
    return await this.emptyState.isVisible().catch(() => false)
  }

  /**
   * Check if content is displayed (non-empty)
   */
  async hasContent(): Promise<boolean> {
    const postCount = await this.getPostCount()
    const producerCount = await this.getProducerCount()
    return postCount > 0 || producerCount > 0
  }

  /**
   * Set time window filter (for trending tab)
   */
  async setTimeWindow(window: 'day' | 'week' | 'month'): Promise<void> {
    const windowMap = {
      'day': this.page.locator('button:has-text("Today")'),
      'week': this.page.locator('button:has-text("This Week")'),
      'month': this.page.locator('button:has-text("This Month")'),
    }
    await windowMap[window].click()
    await this.waitForTabLoad()
  }

  /**
   * Get active time window
   */
  async getActiveTimeWindow(): Promise<string | null> {
    const windows = [
      { name: 'day', locator: this.page.locator('button:has-text("Today")') },
      { name: 'week', locator: this.page.locator('button:has-text("This Week")') },
      { name: 'month', locator: this.page.locator('button:has-text("This Month")') },
    ]

    for (const window of windows) {
      const isActive = await window.locator
        .evaluate((el: Element) => el.className.includes('default'))
        .catch(() => false)
      if (isActive) return window.name
    }
    return null
  }

  /**
   * Get a specific post card by index
   */
  async getPostCard(index: number): Promise<DiscoveryPostCard> {
    const card = this.postCards.nth(index)
    await card.isVisible()
    return new DiscoveryPostCard(this.page, card)
  }

  /**
   * Like a post by index (for-you/trending/genres tabs)
   */
  async likePost(index: number): Promise<void> {
    const card = await this.getPostCard(index)
    await card.like()
  }

  /**
   * Get all recommendations (for-you tab)
   */
  async getRecommendations(): Promise<Array<{ title: string; reason?: string }>> {
    const recommendations: Array<{ title: string; reason?: string }> = []
    const count = await this.getPostCount()

    for (let i = 0; i < count; i++) {
      const card = this.postCards.nth(i)
      // Try to find title
      const title = await card.locator('h3, .font-semibold').first().textContent().catch(() => '')
      // Try to find recommendation reason
      const reason = await this.recommendationReason
        .nth(i)
        .textContent()
        .catch(() => undefined)

      recommendations.push({
        title: title?.trim() || `Post ${i + 1}`,
        reason: reason?.replace('💡', '').trim(),
      })
    }

    return recommendations
  }

  /**
   * Check if specific producer is visible
   */
  async hasProducer(producerName: string): Promise<boolean> {
    return await this.page
      .locator(`text="${producerName}"`)
      .isVisible()
      .catch(() => false)
  }

  /**
   * Get error message if present
   */
  async getErrorMessage(): Promise<string | null> {
    const error = await this.page.locator('[role="alert"], .text-red-500').first().textContent().catch(() => null)
    return error?.trim() || null
  }

  /**
   * Check if error is present
   */
  async hasError(): Promise<boolean> {
    return (await this.getErrorMessage()) !== null
  }

  /**
   * Scroll to bottom of discovery content
   */
  async scrollToBottom(): Promise<void> {
    await this.page.locator('main').last().scrollIntoViewIfNeeded()
  }
}

/**
 * DiscoveryPostCard - Represents a post card in discovery
 */
export class DiscoveryPostCard {
  readonly page: Page
  readonly element: Locator
  readonly likeButton: Locator
  readonly saveButton: Locator
  readonly commentButton: Locator
  readonly playButton: Locator
  readonly authorLink: Locator
  readonly likeCount: Locator

  constructor(page: Page, element: Locator) {
    this.page = page
    this.element = element
    this.likeButton = element.locator('[aria-label*="like" i], button:has-text("♥"), button:has-text("❤")')
    this.saveButton = element.locator('[aria-label*="save" i], button:has-text("📌")')
    this.commentButton = element.locator('[aria-label*="comment" i], button:has-text("💬")')
    this.playButton = element.locator('[aria-label*="play" i], button:has-text("▶")')
    this.authorLink = element.locator('a:has-text(/^@/)')
    this.likeCount = element.locator('text=/\\d+\\s*(like|♥|❤)/i')
  }

  /**
   * Like the post
   */
  async like(): Promise<void> {
    await this.likeButton.click()
  }

  /**
   * Check if post is liked
   */
  async isLiked(): Promise<boolean> {
    const classes = await this.likeButton.evaluate((el) => el.className).catch(() => '')
    return classes.includes('text-red') || classes.includes('fill-red')
  }

  /**
   * Get like count
   */
  async getLikeCount(): Promise<number> {
    const text = await this.likeCount.textContent().catch(() => '0')
    const match = text.match(/\d+/)
    return match ? parseInt(match[0], 10) : 0
  }

  /**
   * Save the post
   */
  async save(): Promise<void> {
    await this.saveButton.click()
  }

  /**
   * Comment on the post
   */
  async comment(): Promise<void> {
    await this.commentButton.click()
  }

  /**
   * Play the audio
   */
  async play(): Promise<void> {
    await this.playButton.click()
  }

  /**
   * Navigate to author profile
   */
  async clickAuthor(): Promise<void> {
    await this.authorLink.click()
  }

  /**
   * Click the post card
   */
  async click(): Promise<void> {
    await this.element.click()
  }

  /**
   * Check if card is visible
   */
  async isVisible(): Promise<boolean> {
    return await this.element.isVisible().catch(() => false)
  }
}
