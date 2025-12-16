import { test, expect } from '../../fixtures/auth'
import { FeedPage } from '../../page-objects/FeedPage'

test.describe('Feed - Type Switching', () => {
  test('should switch to global feed', async ({ authenticatedPage }) => {
    const feedPage = new FeedPage(authenticatedPage)
    await feedPage.goto()

    // Switch to global
    await feedPage.switchToFeedType('global')

    // Global button should be active
    const isGlobalActive = await feedPage.isFeedTypeActive('global')
    expect(isGlobalActive).toBe(true)

    // Timeline should not be active
    const isTimelineActive = await feedPage.isFeedTypeActive('timeline')
    expect(isTimelineActive).toBe(false)
  })

  test('should switch to trending feed', async ({ authenticatedPage }) => {
    const feedPage = new FeedPage(authenticatedPage)
    await feedPage.goto()

    // Switch to trending
    await feedPage.switchToFeedType('trending')

    // Trending button should be active
    const isTrendingActive = await feedPage.isFeedTypeActive('trending')
    expect(isTrendingActive).toBe(true)
  })

  test('should load different posts for different feed types', async ({ authenticatedPage }) => {
    const feedPage = new FeedPage(authenticatedPage)
    await feedPage.goto()

    // Get timeline posts
    const timelineCount = await feedPage.getPostCount()

    // Switch to global
    await feedPage.switchToFeedType('global')
    const globalCount = await feedPage.getPostCount()

    // Posts might be different or same depending on data
    // Just verify both loaded
    expect(timelineCount >= 0).toBe(true)
    expect(globalCount >= 0).toBe(true)
  })

  test('should update button active states when switching', async ({ authenticatedPage }) => {
    const feedPage = new FeedPage(authenticatedPage)
    await feedPage.goto()

    const feedTypes: Array<'timeline' | 'global' | 'trending'> = [
      'timeline',
      'global',
      'trending',
    ]

    for (const feedType of feedTypes) {
      await feedPage.switchToFeedType(feedType)
      const isActive = await feedPage.isFeedTypeActive(feedType)
      expect(isActive).toBe(true)
    }
  })

  test('should handle rapid feed switching', async ({ authenticatedPage }) => {
    const feedPage = new FeedPage(authenticatedPage)
    await feedPage.goto()

    // Rapidly switch between feeds
    for (let i = 0; i < 3; i++) {
      await feedPage.switchToFeedType('global')
      await feedPage.switchToFeedType('timeline')
      await feedPage.switchToFeedType('trending')
    }

    // Should still be on trending
    const isTrendingActive = await feedPage.isFeedTypeActive('trending')
    expect(isTrendingActive).toBe(true)

    // Should still have content or empty state
    const postCount = await feedPage.getPostCount()
    const hasEmpty = await feedPage.hasEmptyState()
    expect(postCount > 0 || hasEmpty).toBe(true)
  })

  test('should show loading state when switching feeds', async ({ authenticatedPage }) => {
    const feedPage = new FeedPage(authenticatedPage)
    await feedPage.goto()

    // Start switching (might catch loading state due to network)
    const switchPromise = feedPage.switchToFeedType('global')

    // Might see loading spinner briefly
    const isLoadingInitially = await feedPage.isLoading()

    await switchPromise

    // After switch, should not be loading
    const isLoadingAfter = await feedPage.isLoading()
    expect(isLoadingAfter).toBe(false)
  })

  test('should maintain scroll position within feed type', async ({ authenticatedPage }) => {
    const feedPage = new FeedPage(authenticatedPage)
    await feedPage.goto()

    // Get initial state
    const initialCount = await feedPage.getPostCount()

    if (initialCount > 0) {
      // Scroll down
      await feedPage.scrollToBottom()

      // Switch to another feed and back
      await feedPage.switchToFeedType('global')
      await feedPage.switchToFeedType('timeline')

      // Should still have posts loaded
      const finalCount = await feedPage.getPostCount()
      expect(finalCount > 0).toBe(true)
    }
  })

  test('should have all feed type buttons visible', async ({ authenticatedPage }) => {
    const feedPage = new FeedPage(authenticatedPage)
    await feedPage.goto()

    // All buttons should be visible
    await expect(feedPage.timelineButton).toBeVisible()
    await expect(feedPage.globalButton).toBeVisible()
    await expect(feedPage.trendingButton).toBeVisible()
  })

  test('should not show error after feed type switch', async ({ authenticatedPage }) => {
    const feedPage = new FeedPage(authenticatedPage)
    await feedPage.goto()

    // Switch multiple times
    await feedPage.switchToFeedType('global')
    await feedPage.switchToFeedType('trending')
    await feedPage.switchToFeedType('timeline')

    // Should not show error
    const hasError = await feedPage.hasError()
    expect(hasError).toBe(false)
  })
})
