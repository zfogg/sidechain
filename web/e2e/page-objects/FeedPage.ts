import { Page, Locator, expect } from '@playwright/test'
import { waitForLoadingToComplete, waitForContentLoad } from '../helpers/wait-strategies'

export type FeedType = 'timeline' | 'global' | 'trending' | 'forYou'

/**
 * FeedPage - Page object for the main feed/timeline page
 * Handles feed navigation, loading, interactions, and pagination
 */
export class FeedPage {
  readonly page: Page
  readonly url = '/feed'

  // Navigation elements
  readonly timelineButton: Locator
  readonly globalButton: Locator
  readonly trendingButton: Locator
  readonly forYouButton: Locator

  // Content elements
  readonly postCards: Locator
  readonly emptyState: Locator
  readonly loadingSpinner: Locator
  readonly errorMessage: Locator

  // Pagination
  readonly loadMoreButton: Locator

  // Counters
  readonly activityCount: Locator

  constructor(page: Page) {
    this.page = page
    this.timelineButton = page.locator('button', { hasText: /Timeline|👥/ })
    this.globalButton = page.locator('button', { hasText: /Global|🌍/ })
    this.trendingButton = page.locator('button', { hasText: /Trending|📊|This Week/ })
    this.forYouButton = page.locator('button', { hasText: /For You|Recommended/ })

    this.postCards = page.locator('[data-testid="post-card"], .bg-card.border, [class*="Post"]')
    this.emptyState = page.locator('text=/no activity|no posts|empty|📭|📈/i')
    this.loadingSpinner = page.locator('[data-testid="spinner"], [class*="Spinner"], [class*="Loading"]')
    this.errorMessage = page.locator('text=/error|failed|unable/i')

    this.loadMoreButton = page.locator('button', { hasText: /Load More|View More|Show More/i })
    this.activityCount = page.locator('text=/\\d+\\s+(activitie|post|item)s/i')
  }

  /**
   * Navigate to the feed page
   */
  async goto(): Promise<void> {
    await this.page.goto(this.url)
    await this.waitForFeedLoad()
  }

  /**
   * Wait for feed to load with posts or empty state
   */
  async waitForFeedLoad(timeout: number = 10000): Promise<void> {
    await this.page.waitForLoadState('networkidle', { timeout }).catch(() => {})

    // Wait for either posts, empty state, or error to be visible
    try {
      await expect(
        this.page.locator(
          '[data-testid="post-card"], .bg-card.border, text=/no activity|no posts|error|failed/i'
        ).first()
      ).toBeVisible({ timeout: 5000 })
    } catch (e) {
      // If nothing visible, that's ok - page might still be loading
    }
  }

  /**
   * Switch to a specific feed type
   */
  async switchToFeedType(feedType: FeedType): Promise<void> {
    const buttons = {
      timeline: this.timelineButton,
      global: this.globalButton,
      trending: this.trendingButton,
      forYou: this.forYouButton,
    }

    await buttons[feedType].click()
    await this.waitForFeedLoad()
  }

  /**
   * Get the count of visible post cards
   */
  async getPostCount(): Promise<number> {
    return await this.postCards.count()
  }

  /**
   * Check if a feed type button is active/selected
   */
  async isFeedTypeActive(feedType: FeedType): Promise<boolean> {
    const buttons = {
      timeline: this.timelineButton,
      global: this.globalButton,
      trending: this.trendingButton,
      forYou: this.forYouButton,
    }

    const button = buttons[feedType]
    return await button.evaluate((el: HTMLElement) =>
      el.className.includes('active') ||
      el.getAttribute('aria-current') === 'page' ||
      el.parentElement?.className.includes('active')
    )
  }

  /**
   * Check if empty state is visible
   */
  async hasEmptyState(): Promise<boolean> {
    return await this.emptyState.isVisible({ timeout: 1000 }).catch(() => false)
  }

  /**
   * Check if error state is visible
   */
  async hasError(): Promise<boolean> {
    return await this.errorMessage.isVisible({ timeout: 1000 }).catch(() => false)
  }

  /**
   * Check if loading spinner is visible
   */
  async isLoading(): Promise<boolean> {
    return await this.loadingSpinner.isVisible({ timeout: 1000 }).catch(() => false)
  }

  /**
   * Wait for loading to complete
   */
  async waitForLoading(): Promise<void> {
    await waitForLoadingToComplete(this.page, '[data-testid="spinner"]')
  }

  /**
   * Get a post card by index
   */
  getPostCard(index: number): PostCardElement {
    return new PostCardElement(this.postCards.nth(index))
  }

  /**
   * Get all post cards
   */
  async getAllPostCards(): Promise<PostCardElement[]> {
    const count = await this.getPostCount()
    return Array.from({ length: count }, (_, i) => this.getPostCard(i))
  }

  /**
   * Scroll down to load more posts
   */
  async scrollToBottom(): Promise<void> {
    await this.page.evaluate(() => window.scrollTo(0, document.body.scrollHeight))
    await this.page.waitForTimeout(500)
  }

  /**
   * Click load more button if visible
   */
  async clickLoadMore(): Promise<void> {
    const isVisible = await this.loadMoreButton.isVisible({ timeout: 1000 }).catch(() => false)
    if (isVisible) {
      await this.loadMoreButton.click()
      await this.waitForLoading()
    }
  }

  /**
   * Get activity count from header
   */
  async getActivityCount(): Promise<number> {
    const text = await this.activityCount.textContent()
    const match = text?.match(/(\d+)/)
    return match ? parseInt(match[1]) : 0
  }

  /**
   * Check if has more posts to load
   */
  async hasMorePostsToLoad(): Promise<boolean> {
    return await this.loadMoreButton.isVisible({ timeout: 1000 }).catch(() => false)
  }

  /**
   * Get end of feed message
   */
  async getEndOfFeedMessage(): Promise<string | null> {
    const text = await this.page.locator('text=/no more|end of feed|thats all/i').textContent()
    return text
  }
}

/**
 * PostCardElement - Represents a single post card in the feed
 */
export class PostCardElement {
  readonly locator: Locator

  // Post info
  readonly authorName: Locator
  readonly authorUsername: Locator
  readonly postTime: Locator

  // Post content
  readonly postTitle: Locator
  readonly postDescription: Locator

  // Post actions
  readonly likeButton: Locator
  readonly likeCount: Locator
  readonly commentButton: Locator
  readonly commentCount: Locator
  readonly saveButton: Locator
  readonly shareButton: Locator
  readonly playButton: Locator

  // Post metadata
  readonly audioPlayer: Locator
  readonly playCount: Locator

  constructor(cardLocator: Locator) {
    this.locator = cardLocator

    this.authorName = cardLocator.locator('[class*="name"], [class*="author"], p strong').first()
    this.authorUsername = cardLocator.locator('[class*="username"], p').filter({ hasText: /@/ }).first()
    this.postTime = cardLocator.locator('[class*="time"], [class*="date"], text=/ago/i').first()

    this.postTitle = cardLocator.locator('h2, h3, [class*="Title"]').first()
    this.postDescription = cardLocator.locator('p', { has: cardLocator }).nth(1)

    this.likeButton = cardLocator.locator('button', { hasText: /❤|Like|like/i }).first()
    this.likeCount = this.likeButton.locator('.. >> text=/\\d+/').first()
    this.commentButton = cardLocator.locator('button', { hasText: /💬|Comment|comment/i })
    this.commentCount = this.commentButton.locator('.. >> text=/\\d+/').first()
    this.saveButton = cardLocator.locator('button', { hasText: /💾|Save|save|bookmark/i })
    this.shareButton = cardLocator.locator('button', { hasText: /🔗|Share|share/i })
    this.playButton = cardLocator.locator('button', { hasText: /▶|Play|play/i }).first()

    this.audioPlayer = cardLocator.locator('audio, [data-testid="audio-player"]')
    this.playCount = cardLocator.locator('text=/\\d+\\s+play/i')
  }

  /**
   * Get post author display name
   */
  async getAuthorName(): Promise<string> {
    return (await this.authorName.textContent()) || ''
  }

  /**
   * Get post author username
   */
  async getAuthorUsername(): Promise<string> {
    const text = (await this.authorUsername.textContent()) || ''
    return text.replace('@', '').trim()
  }

  /**
   * Get post timestamp
   */
  async getPostTime(): Promise<string> {
    return (await this.postTime.textContent()) || ''
  }

  /**
   * Click like button
   */
  async like(): Promise<void> {
    await this.likeButton.click()
  }

  /**
   * Check if post is liked
   */
  async isLiked(): Promise<boolean> {
    const className = await this.likeButton.getAttribute('class')
    return (className || '').includes('active') || (className || '').includes('liked')
  }

  /**
   * Get like count
   */
  async getLikeCount(): Promise<number> {
    const text = await this.likeCount.textContent()
    const match = text?.match(/(\d+)/)
    return match ? parseInt(match[1]) : 0
  }

  /**
   * Click comment button
   */
  async comment(): Promise<void> {
    await this.commentButton.click()
  }

  /**
   * Get comment count
   */
  async getCommentCount(): Promise<number> {
    const text = await this.commentCount.textContent()
    const match = text?.match(/(\d+)/)
    return match ? parseInt(match[1]) : 0
  }

  /**
   * Click save button
   */
  async save(): Promise<void> {
    await this.saveButton.click()
  }

  /**
   * Click share button
   */
  async share(): Promise<void> {
    await this.shareButton.click()
  }

  /**
   * Play audio
   */
  async play(): Promise<void> {
    await this.playButton.click()
  }

  /**
   * Get play count
   */
  async getPlayCount(): Promise<number> {
    const text = await this.playCount.textContent()
    const match = text?.match(/(\d+)/)
    return match ? parseInt(match[1]) : 0
  }

  /**
   * Click author name to view profile
   */
  async clickAuthor(): Promise<void> {
    await this.authorName.click()
  }

  /**
   * Click the card itself
   */
  async click(): Promise<void> {
    await this.locator.click()
  }

  /**
   * Check if element is visible
   */
  async isVisible(): Promise<boolean> {
    return await this.locator.isVisible()
  }
}
