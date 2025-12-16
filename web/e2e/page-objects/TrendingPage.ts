import { Page, Locator, expect } from '@playwright/test'

/**
 * TrendingPage - Display trending posts with time period filtering
 * Encapsulates interactions with the Trending page
 */
export class TrendingPage {
  readonly page: Page
  readonly heading: Locator
  readonly weeklyButton: Locator
  readonly monthlyButton: Locator
  readonly alltimeButton: Locator
  readonly postCards: Locator
  readonly rankBadges: Locator
  readonly loadingSpinner: Locator
  readonly emptyState: Locator
  readonly errorMessage: Locator
  readonly postCount: Locator
  readonly loadMoreButton: Locator

  constructor(page: Page) {
    this.page = page
    this.heading = page.locator('h1:has-text("Trending")')
    this.weeklyButton = page.locator('button:has-text("This Week")')
    this.monthlyButton = page.locator('button:has-text("This Month")')
    this.alltimeButton = page.locator('button:has-text("All Time")')
    this.postCards = page.locator('[data-testid*="post-card"], .group:has(> :nth-child(1))')
    this.rankBadges = page.locator('.rounded-full.bg-gradient-to-r')
    this.loadingSpinner = page.locator('[data-testid="spinner"], .animate-spin')
    this.emptyState = page.locator('text=/No trending posts|Check back later/i')
    this.errorMessage = page.locator('text=/Failed to load trending/i')
    this.postCount = page.locator('text=/\\d+ posts/i')
    this.loadMoreButton = page.locator('button:has-text("Load More")')
  }

  /**
   * Navigate to trending page
   */
  async goto(): Promise<void> {
    await this.page.goto('/trending')
    await this.waitForPageLoad()
  }

  /**
   * Wait for trending page to load
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
   * Set time period filter
   */
  async setTimePeriod(period: 'weekly' | 'monthly' | 'alltime'): Promise<void> {
    const buttonMap = {
      'weekly': this.weeklyButton,
      'monthly': this.monthlyButton,
      'alltime': this.alltimeButton,
    }
    await buttonMap[period].click()
    await this.waitForPostsLoad()
  }

  /**
   * Get active time period
   */
  async getActiveTimePeriod(): Promise<string | null> {
    const periods = [
      { name: 'weekly', locator: this.weeklyButton },
      { name: 'monthly', locator: this.monthlyButton },
      { name: 'alltime', locator: this.alltimeButton },
    ]

    for (const period of periods) {
      const isActive = await period.locator
        .evaluate((el: Element) => el.className.includes('default'))
        .catch(() => false)
      if (isActive) return period.name
    }
    return null
  }

  /**
   * Wait for posts to load after filtering
   */
  async waitForPostsLoad(timeout: number = 10000): Promise<void> {
    // Wait for spinner to disappear or posts to appear
    await Promise.race([
      this.loadingSpinner.waitFor({ state: 'hidden', timeout }),
      this.postCards.first().waitFor({ timeout }),
      this.emptyState.waitFor({ timeout }),
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
   * Check if empty state is shown
   */
  async hasEmptyState(): Promise<boolean> {
    return await this.emptyState.isVisible().catch(() => false)
  }

  /**
   * Check if error is shown
   */
  async hasError(): Promise<boolean> {
    return await this.errorMessage.isVisible().catch(() => false)
  }

  /**
   * Get error message
   */
  async getErrorMessage(): Promise<string | null> {
    const error = await this.errorMessage.textContent().catch(() => null)
    return error?.trim() || null
  }

  /**
   * Get post count from header
   */
  async getDisplayedPostCount(): Promise<number> {
    const text = await this.postCount.textContent().catch(() => '0 posts')
    const match = text.match(/\d+/)
    return match ? parseInt(match[0], 10) : 0
  }

  /**
   * Get a specific post card by index
   */
  async getPostCard(index: number): Promise<TrendingPostCard> {
    const card = this.postCards.nth(index)
    await card.isVisible()
    return new TrendingPostCard(this.page, card)
  }

  /**
   * Get rank badge for post
   */
  async getRankBadge(index: number): Promise<string | null> {
    const badge = this.rankBadges.nth(index)
    const text = await badge.textContent().catch(() => null)
    return text?.trim() || null
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
   * Check if load more button is disabled
   */
  async isLoadMoreDisabled(): Promise<boolean> {
    return await this.loadMoreButton.isDisabled().catch(() => false)
  }

  /**
   * Like a post by index
   */
  async likePost(index: number): Promise<void> {
    const card = await this.getPostCard(index)
    await card.like()
  }

  /**
   * Check if page has any posts
   */
  async hasPosts(): Promise<boolean> {
    const postCount = await this.getPostCount()
    return postCount > 0
  }

  /**
   * Scroll to bottom of trending posts
   */
  async scrollToBottom(): Promise<void> {
    await this.page.locator('main').last().scrollIntoViewIfNeeded()
  }

  /**
   * Wait for post count to match expected value
   */
  async waitForPostCountToMatch(expectedCount: number, timeout: number = 5000): Promise<boolean> {
    try {
      await expect(async () => {
        const count = await this.getPostCount()
        expect(count).toBe(expectedCount)
      }).toPass({ timeout })
      return true
    } catch {
      return false
    }
  }
}

/**
 * TrendingPostCard - Represents a post card in trending page
 */
export class TrendingPostCard {
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
