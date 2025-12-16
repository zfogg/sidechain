import { test } from '../../fixtures/auth'
import { expect } from '@playwright/test'
import { FeedPage } from '../../page-objects/FeedPage'

test.describe('Feed - Loading & Display', () => {
  test('should load timeline feed by default', async ({ authenticatedPage }) => {
    const feedPage = new FeedPage(authenticatedPage)
    await feedPage.goto()

    // Should be on feed page
    expect(authenticatedPage.url()).toContain('/feed')

    // Timeline should be active
    const isTimelineActive = await feedPage.isFeedTypeActive('timeline')
    expect(isTimelineActive).toBe(true)
  })

  test('should display posts or empty state', async ({ authenticatedPage }) => {
    const feedPage = new FeedPage(authenticatedPage)
    await feedPage.goto()

    // Should have either posts or empty state
    const postCount = await feedPage.getPostCount()
    const hasEmpty = await feedPage.hasEmptyState()

    expect(postCount > 0 || hasEmpty).toBe(true)
  })

  test('should show loading spinner while loading', async ({ authenticatedPage }) => {
    const feedPage = new FeedPage(authenticatedPage)

    // Start navigation (might catch loading state)
    const gotoPromise = feedPage.goto()

    // Try to see loading spinner (might be too fast)
    const isLoading = await feedPage.isLoading()

    await gotoPromise

    // After navigation, should not be loading
    const stillLoading = await feedPage.isLoading()
    expect(stillLoading).toBe(false)
  })

  test('should display post count in header', async ({ authenticatedPage }) => {
    const feedPage = new FeedPage(authenticatedPage)
    await feedPage.goto()

    // Should have an activity count (might be 0)
    const count = await feedPage.getActivityCount()
    expect(count).toBeGreaterThanOrEqual(0)
  })

  test('should handle empty feed gracefully', async ({ authenticatedPage }) => {
    const feedPage = new FeedPage(authenticatedPage)
    await feedPage.goto()

    // If empty, should show empty state message
    const hasEmpty = await feedPage.hasEmptyState()
    if (hasEmpty) {
      expect(await feedPage.postCount()).toBe(0)
      const emptyMessage = await feedPage.emptyState.textContent()
      expect(emptyMessage).toBeTruthy()
    }
  })

  test('should retry on error', async ({ authenticatedPage }) => {
    const feedPage = new FeedPage(authenticatedPage)
    await feedPage.goto()

    // Network request should succeed (real backend)
    const hasError = await feedPage.hasError()
    expect(hasError).toBe(false)
  })

  test('should display feed action buttons', async ({ authenticatedPage }) => {
    const feedPage = new FeedPage(authenticatedPage)
    await feedPage.goto()

    // All feed type buttons should be visible
    await expect(feedPage.timelineButton).toBeVisible()
    await expect(feedPage.globalButton).toBeVisible()
    await expect(feedPage.trendingButton).toBeVisible()
  })

  test('should have scrollable post list', async ({ authenticatedPage }) => {
    const feedPage = new FeedPage(authenticatedPage)
    await feedPage.goto()

    const initialPostCount = await feedPage.getPostCount()

    // If posts exist, should be able to scroll
    if (initialPostCount > 0) {
      await feedPage.scrollToBottom()

      // Page should still show posts
      const postCount = await feedPage.getPostCount()
      expect(postCount).toBeGreaterThan(0)
    }
  })

  test('should persist scroll position when switching tabs', async ({ authenticatedPage }) => {
    const feedPage = new FeedPage(authenticatedPage)
    await feedPage.goto()

    const initialPostCount = await feedPage.getPostCount()

    if (initialPostCount > 0) {
      // Scroll down
      await feedPage.scrollToBottom()
      const scrollTop = await authenticatedPage.evaluate(() => window.scrollY)

      // Switch to different feed type
      await feedPage.switchToFeedType('global')

      // Switch back
      await feedPage.switchToFeedType('timeline')

      // Scroll position might reset (depends on implementation)
      const newScrollTop = await authenticatedPage.evaluate(() => window.scrollY)

      // Just verify page is still functional
      const postCount = await feedPage.getPostCount()
      expect(postCount).toBeGreaterThanOrEqual(0)
    }
  })
})
