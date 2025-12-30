import { Page, Locator, expect } from '@playwright/test'

/**
 * DiscoveryPage - Discovery features on the Feed page
 * Maps to /feed with discovery tabs (For You, Trending, Producers, Genres)
 */
export class DiscoveryPage {
  readonly page: Page
  readonly heading: Locator
  readonly forYouTab: Locator
  readonly trendingTab: Locator
  readonly producersTab: Locator
  readonly genresTab: Locator
  readonly timelineTab: Locator
  readonly globalTab: Locator
  readonly postCards: Locator
  readonly producerCards: Locator
  readonly timeWindowButtons: Locator
  readonly loadingIndicator: Locator
  readonly emptyState: Locator

  constructor(page: Page) {
    this.page = page
    this.heading = page.locator('h1:has-text("Sidechain Feed"), h1:has-text("Feed")')
    this.forYouTab = page.locator('button:has-text("For You"), button:has-text("✨")').first()
    this.trendingTab = page.locator('button:has-text("Trending"), button:has-text("🔥")').first()
    this.producersTab = page.locator('button:has-text("Producers"), button:has-text("👤")').first()
    this.genresTab = page.locator('button:has-text("Genres"), button:has-text("🎵")').first()
    this.timelineTab = page.locator('button:has-text("Timeline"), button:has-text("📰")').first()
    this.globalTab = page.locator('button:has-text("Global"), button:has-text("🌍")').first()
    this.postCards = page.locator('[data-testid="post-card"], .group')
    this.producerCards = page.locator('[data-testid="producer-card"]')
    this.timeWindowButtons = page.locator('button:has-text("Today"), button:has-text("This Week"), button:has-text("This Month")')
    this.loadingIndicator = page.locator('[data-testid="loading"], .loading, .spinner, [role="progressbar"]')
    this.emptyState = page.locator('[data-testid="empty-state"], text=/no posts|nothing here|empty/i')
  }

  async goto(): Promise<void> {
    await this.page.goto('/feed')
    await this.page.waitForLoadState('domcontentloaded')
    await this.waitForPageLoad()
  }

  async waitForPageLoad(timeout: number = 5000): Promise<void> {
    // Wait for either heading or tabs to be visible
    await expect(this.heading.or(this.timelineTab)).toBeVisible({ timeout })
  }

  async hasError(): Promise<boolean> {
    const errorIndicator = this.page.locator('text=/error|failed|unable/i, [data-testid="error"]')
    return await errorIndicator.isVisible().catch(() => false)
  }

  async isLoaded(): Promise<boolean> {
    return await this.heading.isVisible().catch(() => false)
  }

  async switchTab(tab: 'for-you' | 'trending' | 'producers' | 'genres' | 'timeline' | 'global'): Promise<void> {
    const tabMap = {
      'for-you': this.forYouTab,
      'trending': this.trendingTab,
      'producers': this.producersTab,
      'genres': this.genresTab,
      'timeline': this.timelineTab,
      'global': this.globalTab,
    }
    await tabMap[tab].click()
  }

  async getPostCount(): Promise<number> {
    return await this.postCards.count()
  }

  async getProducerCount(): Promise<number> {
    return await this.producerCards.count()
  }

  async hasContent(): Promise<boolean> {
    const postCount = await this.getPostCount()
    return postCount > 0
  }

  async setTimeWindow(window: 'day' | 'week' | 'month'): Promise<void> {
    const labels = { day: 'Today', week: 'This Week', month: 'This Month' }
    await this.page.locator(`button:has-text("${labels[window]}")`).click()
  }

  async getPostCard(index: number): Promise<DiscoveryPostCard> {
    const card = this.postCards.nth(index)
    return new DiscoveryPostCard(this.page, card)
  }

  async likePost(index: number): Promise<void> {
    const card = await this.getPostCard(index)
    await card.like()
  }

  async waitForTabLoad(): Promise<void> {
    // Wait for content to load after tab switch
    await this.page.waitForLoadState('domcontentloaded')
  }

  async isTabActive(tab: 'for-you' | 'trending' | 'producers' | 'genres' | 'timeline' | 'global'): Promise<boolean> {
    const tabMap = {
      'for-you': this.forYouTab,
      'trending': this.trendingTab,
      'producers': this.producersTab,
      'genres': this.genresTab,
      'timeline': this.timelineTab,
      'global': this.globalTab,
    }
    const tabEl = tabMap[tab]
    const classes = await tabEl.getAttribute('class') || ''
    const ariaSelected = await tabEl.getAttribute('aria-selected')
    return classes.includes('active') || classes.includes('selected') || ariaSelected === 'true'
  }

  async isLoading(): Promise<boolean> {
    return await this.loadingIndicator.isVisible().catch(() => false)
  }

  async hasEmptyState(): Promise<boolean> {
    return await this.emptyState.isVisible().catch(() => false)
  }

  async getActiveTimeWindow(): Promise<'day' | 'week' | 'month' | null> {
    const buttons = [
      { locator: this.page.locator('button:has-text("Today")'), value: 'day' as const },
      { locator: this.page.locator('button:has-text("This Week")'), value: 'week' as const },
      { locator: this.page.locator('button:has-text("This Month")'), value: 'month' as const },
    ]
    for (const btn of buttons) {
      const classes = await btn.locator.getAttribute('class') || ''
      const ariaPressed = await btn.locator.getAttribute('aria-pressed')
      if (classes.includes('active') || classes.includes('selected') || ariaPressed === 'true') {
        return btn.value
      }
    }
    return null
  }
}

export class DiscoveryPostCard {
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
