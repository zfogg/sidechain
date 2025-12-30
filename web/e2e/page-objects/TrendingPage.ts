import { Page, Locator, expect } from '@playwright/test'

/**
 * TrendingPage - Trending tab on the Feed page
 * Maps to /feed with Trending tab selected
 */
export class TrendingPage {
  readonly page: Page
  readonly heading: Locator
  readonly trendingTab: Locator
  readonly todayButton: Locator
  readonly weeklyButton: Locator
  readonly monthlyButton: Locator
  readonly postCards: Locator
  readonly loadMoreButton: Locator

  constructor(page: Page) {
    this.page = page
    this.heading = page.locator('h1:has-text("Sidechain Feed"), h1:has-text("Feed")')
    this.trendingTab = page.locator('button:has-text("Trending"), button:has-text("🔥")').first()
    this.todayButton = page.locator('button:has-text("Today")')
    this.weeklyButton = page.locator('button:has-text("This Week")')
    this.monthlyButton = page.locator('button:has-text("This Month")')
    this.postCards = page.locator('[data-testid="post-card"], .group')
    this.loadMoreButton = page.locator('button:has-text("Load More")')
  }

  async goto(): Promise<void> {
    await this.page.goto('/feed')
    await this.waitForPageLoad()
    await this.trendingTab.click()
  }

  async waitForPageLoad(timeout: number = 5000): Promise<void> {
    await expect(this.heading).toBeVisible({ timeout })
  }

  async isLoaded(): Promise<boolean> {
    return await this.heading.isVisible().catch(() => false)
  }

  async setTimePeriod(period: 'day' | 'weekly' | 'monthly'): Promise<void> {
    const buttonMap = {
      'day': this.todayButton,
      'weekly': this.weeklyButton,
      'monthly': this.monthlyButton,
    }
    await buttonMap[period].click()
  }

  async getPostCount(): Promise<number> {
    return await this.postCards.count()
  }

  async getPostCard(index: number): Promise<TrendingPostCard> {
    const card = this.postCards.nth(index)
    return new TrendingPostCard(this.page, card)
  }

  async hasPosts(): Promise<boolean> {
    return (await this.getPostCount()) > 0
  }

  async hasLoadMoreButton(): Promise<boolean> {
    return await this.loadMoreButton.isVisible().catch(() => false)
  }

  async clickLoadMore(): Promise<void> {
    await this.loadMoreButton.click()
  }
}

export class TrendingPostCard {
  readonly page: Page
  readonly element: Locator
  readonly likeButton: Locator
  readonly playButton: Locator
  readonly authorLink: Locator

  constructor(page: Page, element: Locator) {
    this.page = page
    this.element = element
    this.likeButton = element.locator('button').filter({ hasText: /like|heart|❤|♥/i }).first()
    this.playButton = element.locator('button[aria-label*="play" i]').first()
    this.authorLink = element.locator('a[href*="/@"]').first()
  }

  async like(): Promise<void> {
    await this.likeButton.click()
  }

  async play(): Promise<void> {
    await this.playButton.click()
  }

  async clickAuthor(): Promise<void> {
    await this.authorLink.click()
  }

  async click(): Promise<void> {
    await this.element.click()
  }

  async isVisible(): Promise<boolean> {
    return await this.element.isVisible().catch(() => false)
  }
}
