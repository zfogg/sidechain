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
    this.timelineButton = page.locator('button:has-text("Timeline"), button:has-text("📰")').first()
    this.globalButton = page.locator('button:has-text("Global"), button:has-text("🌍")').first()
    this.trendingButton = page.locator('button:has-text("Trending"), button:has-text("🔥")').first()
    this.forYouButton = page.locator('button:has-text("For You"), button:has-text("✨")').first()

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
  async waitForFeedLoad(timeout: number = 5000): Promise<void> {
    await this.page.waitForLoadState('domcontentloaded').catch(() => {})

    // Wait for either buttons or posts to be visible
    try {
      await expect(
        this.timelineButton.or(this.postCards.first())
      ).toBeVisible({ timeout })
    } catch (e) {
      // If nothing visible, that's ok - page might still be loading
    }
  }

  /**
   * Check if the page is loaded
   */
  async isLoaded(): Promise<boolean> {
    return await this.timelineButton.or(this.postCards.first()).isVisible().catch(() => false)
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

    const button = buttons[feedType]
    // Wait for button to be visible before clicking
    await button.waitFor({ state: 'visible', timeout: 5000 }).catch(() => {})
    await button.click()
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
    try {
      const className = await button.getAttribute('class') || ''
      // The Button component uses 'default' variant for active, 'outline' for inactive
      return !className.includes('outline') || className.includes('active') || className.includes('selected')
    } catch {
      return false
    }
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
    // REMOVED: waitForTimeout
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
  readonly followButton: Locator

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

    // Action buttons row - find the container with multiple ghost buttons
    const actionButtonRow = cardLocator.locator('.flex.gap-2 button[class*="ghost"]')
    // Buttons are in order: Like, Comment, Save, Repost/Share
    this.likeButton = actionButtonRow.nth(0)
    this.likeCount = cardLocator.locator('text=/\\d+/').first()
    this.commentButton = actionButtonRow.nth(1)
    this.commentCount = cardLocator.locator('text=/\\d+ Comments/i').first()
    this.saveButton = actionButtonRow.nth(2)
    this.shareButton = actionButtonRow.nth(3)
    // Play button in audio player section
    this.playButton = cardLocator.locator('button:has(svg), [data-testid="play-button"]').first()
    this.followButton = cardLocator.locator('button:has-text("Follow"), button:has-text("Following")').first()

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
   * Click follow button
   */
  async follow(): Promise<void> {
    const initialText = await this.followButton.textContent()
    await this.followButton.click()
    
    // Wait for button text to change
    await expect(async () => {
      const newText = await this.followButton.textContent()
      expect(newText).not.toBe(initialText)
    }).toPass({ timeout: 3000 })
  }

  /**
   * Check if following this user
   */
  async isFollowing(): Promise<boolean> {
    const text = await this.followButton.textContent()
    return (text || '').includes('Following')
  }

  /**
   * Check if follow button is visible
   */
  async hasFollowButton(): Promise<boolean> {
    return await this.followButton.isVisible({ timeout: 1000 }).catch(() => false)
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
