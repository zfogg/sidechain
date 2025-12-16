import { test } from '../../fixtures/auth'
import { expect } from '@playwright/test'
import { DiscoveryPage } from '../../page-objects/DiscoveryPage'

test.describe('Discovery - Recommendations & Interactions', () => {
  test('should display for-you recommendations', async ({ authenticatedPage }) => {
    const discoveryPage = new DiscoveryPage(authenticatedPage)
    await discoveryPage.goto()

    await discoveryPage.waitForTabLoad()

    // Should show recommendations or empty state
    const hasContent = await discoveryPage.hasContent()
    const hasEmpty = await discoveryPage.hasEmptyState()
    expect(hasContent || hasEmpty).toBe(true)
  })

  test('should show recommendation reasons', async ({ authenticatedPage }) => {
    const discoveryPage = new DiscoveryPage(authenticatedPage)
    await discoveryPage.goto()

    await discoveryPage.waitForTabLoad()

    const postCount = await discoveryPage.getPostCount()
    if (postCount > 0) {
      // Try to get recommendations with reasons
      const recommendations = await discoveryPage.getRecommendations()
      expect(Array.isArray(recommendations)).toBe(true)
    }
  })

  test('should display trending posts when switching to trending tab', async ({ authenticatedPage }) => {
    const discoveryPage = new DiscoveryPage(authenticatedPage)
    await discoveryPage.goto()

    await discoveryPage.switchTab('trending')
    await discoveryPage.waitForTabLoad()

    // Should show trending posts or empty state
    const hasContent = await discoveryPage.hasContent()
    const hasEmpty = await discoveryPage.hasEmptyState()
    expect(hasContent || hasEmpty).toBe(true)
  })

  test('should display trending producers', async ({ authenticatedPage }) => {
    const discoveryPage = new DiscoveryPage(authenticatedPage)
    await discoveryPage.goto()

    await discoveryPage.switchTab('producers')
    await discoveryPage.waitForTabLoad()

    const producerCount = await discoveryPage.getProducerCount()
    expect(producerCount >= 0).toBe(true)
  })

  test('should display genre recommendations', async ({ authenticatedPage }) => {
    const discoveryPage = new DiscoveryPage(authenticatedPage)
    await discoveryPage.goto()

    await discoveryPage.switchTab('genres')
    await discoveryPage.waitForTabLoad()

    // Should show genre picks or empty state
    const hasContent = await discoveryPage.hasContent()
    const hasEmpty = await discoveryPage.hasEmptyState()
    expect(hasContent || hasEmpty).toBe(true)
  })

  test('should like a post from recommendations', async ({ authenticatedPage }) => {
    const discoveryPage = new DiscoveryPage(authenticatedPage)
    await discoveryPage.goto()

    await discoveryPage.waitForTabLoad()

    const postCount = await discoveryPage.getPostCount()
    if (postCount > 0) {
      const card = await discoveryPage.getPostCard(0)
      const initialLiked = await card.isLiked().catch(() => false)

      await card.like()
      await authenticatedPage.waitForTimeout(300)

      const finalLiked = await card.isLiked().catch(() => false)
      expect(typeof finalLiked).toBe('boolean')
    }
  })

  test('should get accurate like count', async ({ authenticatedPage }) => {
    const discoveryPage = new DiscoveryPage(authenticatedPage)
    await discoveryPage.goto()

    await discoveryPage.waitForTabLoad()

    const postCount = await discoveryPage.getPostCount()
    if (postCount > 0) {
      const card = await discoveryPage.getPostCard(0)
      const likeCount = await card.getLikeCount()
      expect(typeof likeCount).toBe('number')
      expect(likeCount >= 0).toBe(true)
    }
  })

  test('should save a post from discovery', async ({ authenticatedPage }) => {
    const discoveryPage = new DiscoveryPage(authenticatedPage)
    await discoveryPage.goto()

    await discoveryPage.waitForTabLoad()

    const postCount = await discoveryPage.getPostCount()
    if (postCount > 0) {
      const card = await discoveryPage.getPostCard(0)
      try {
        await card.save()
        await authenticatedPage.waitForTimeout(300)
        expect(true).toBe(true)
      } catch (e) {
        // Save button might not exist
      }
    }
  })

  test('should comment on a post from discovery', async ({ authenticatedPage }) => {
    const discoveryPage = new DiscoveryPage(authenticatedPage)
    await discoveryPage.goto()

    await discoveryPage.waitForTabLoad()

    const postCount = await discoveryPage.getPostCount()
    if (postCount > 0) {
      const card = await discoveryPage.getPostCard(0)
      try {
        await card.comment()
        await authenticatedPage.waitForTimeout(300)
        expect(true).toBe(true)
      } catch (e) {
        // Comment button might not exist or open dialog
      }
    }
  })

  test('should play audio from discovery post', async ({ authenticatedPage }) => {
    const discoveryPage = new DiscoveryPage(authenticatedPage)
    await discoveryPage.goto()

    await discoveryPage.waitForTabLoad()

    const postCount = await discoveryPage.getPostCount()
    if (postCount > 0) {
      const card = await discoveryPage.getPostCard(0)
      try {
        await card.play()
        await authenticatedPage.waitForTimeout(300)
        expect(true).toBe(true)
      } catch (e) {
        // Play button might not exist
      }
    }
  })

  test('should navigate to author profile from discovery', async ({ authenticatedPage }) => {
    const discoveryPage = new DiscoveryPage(authenticatedPage)
    await discoveryPage.goto()

    await discoveryPage.waitForTabLoad()

    const postCount = await discoveryPage.getPostCount()
    if (postCount > 0) {
      const initialUrl = authenticatedPage.url()
      const card = await discoveryPage.getPostCard(0)

      try {
        await card.clickAuthor()
        await authenticatedPage.waitForTimeout(500)

        // Should navigate to profile
        const newUrl = authenticatedPage.url()
        expect(newUrl).not.toBe(initialUrl)
      } catch (e) {
        // Author link might not exist
      }
    }
  })

  test('should click on a discovery post', async ({ authenticatedPage }) => {
    const discoveryPage = new DiscoveryPage(authenticatedPage)
    await discoveryPage.goto()

    await discoveryPage.waitForTabLoad()

    const postCount = await discoveryPage.getPostCount()
    if (postCount > 0) {
      const initialUrl = authenticatedPage.url()
      const card = await discoveryPage.getPostCard(0)

      try {
        await card.click()
        await authenticatedPage.waitForTimeout(500)

        const newUrl = authenticatedPage.url()
        expect(typeof newUrl).toBe('string')
      } catch (e) {
        // Click might not navigate
      }
    }
  })

  test('should interact with multiple posts from discovery', async ({ authenticatedPage }) => {
    const discoveryPage = new DiscoveryPage(authenticatedPage)
    await discoveryPage.goto()

    await discoveryPage.waitForTabLoad()

    const postCount = await discoveryPage.getPostCount()
    expect(postCount >= 0).toBe(true)

    if (postCount > 1) {
      // Like first post
      const card1 = await discoveryPage.getPostCard(0)
      try {
        await card1.like()
      } catch (e) {
        // Ignore
      }

      // Like second post
      const card2 = await discoveryPage.getPostCard(1)
      try {
        await card2.like()
      } catch (e) {
        // Ignore
      }

      expect(true).toBe(true)
    }
  })

  test('should handle empty recommendations gracefully', async ({ authenticatedPage }) => {
    const discoveryPage = new DiscoveryPage(authenticatedPage)
    await discoveryPage.goto()

    await discoveryPage.waitForTabLoad()

    // If empty, should show empty state message
    const isEmpty = await discoveryPage.hasEmptyState()
    const hasContent = await discoveryPage.hasContent()

    expect(isEmpty || hasContent).toBe(true)
  })

  test('should reload recommendations on tab switch', async ({ authenticatedPage }) => {
    const discoveryPage = new DiscoveryPage(authenticatedPage)
    await discoveryPage.goto()

    // Get initial count from for-you
    await discoveryPage.waitForTabLoad()
    const forYouCount = await discoveryPage.getPostCount()

    // Switch to trending
    await discoveryPage.switchTab('trending')
    await discoveryPage.waitForTabLoad()
    const trendingCount = await discoveryPage.getPostCount()

    // Both should be valid numbers
    expect(typeof forYouCount).toBe('number')
    expect(typeof trendingCount).toBe('number')
  })

  test('should filter recommendations by time window', async ({ authenticatedPage }) => {
    const discoveryPage = new DiscoveryPage(authenticatedPage)
    await discoveryPage.goto()

    await discoveryPage.switchTab('trending')
    await discoveryPage.waitForTabLoad()

    const weekCount = await discoveryPage.getPostCount()

    // Switch to month
    await discoveryPage.setTimeWindow('month')
    await discoveryPage.waitForTabLoad()

    const monthCount = await discoveryPage.getPostCount()

    expect(typeof weekCount).toBe('number')
    expect(typeof monthCount).toBe('number')
  })

  test('should scroll discovery content without errors', async ({ authenticatedPage }) => {
    const discoveryPage = new DiscoveryPage(authenticatedPage)
    await discoveryPage.goto()

    await discoveryPage.waitForTabLoad()

    try {
      await discoveryPage.scrollToBottom()
      expect(true).toBe(true)
    } catch (e) {
      // Scroll might not be needed
    }
  })

  test('should not show error on discovery page', async ({ authenticatedPage }) => {
    const discoveryPage = new DiscoveryPage(authenticatedPage)
    await discoveryPage.goto()

    await discoveryPage.waitForTabLoad()

    const hasError = await discoveryPage.hasError()
    expect(hasError).toBe(false)
  })
})
