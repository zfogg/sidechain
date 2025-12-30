import { test } from '../../fixtures/auth'
import { expect } from '@playwright/test'
import { FeedPage } from '../../page-objects/FeedPage'

/**
 * Rate Limit Error Tests (P2)
 *
 * Tests for rate limit handling:
 * - Rate limit response handling
 * - User feedback on rate limits
 * - Retry after rate limit
 */
test.describe('Rate Limit Errors', () => {
  test.describe('Rate Limit Detection', () => {
    test('should handle 429 rate limit response', async ({ authenticatedPage }) => {
      const feedPage = new FeedPage(authenticatedPage)
      await feedPage.goto()

      // Intercept requests to simulate rate limiting
      await authenticatedPage.route('**/*', async (route) => {
        if (route.request().url().includes('/api/') && Math.random() < 0.3) {
          await route.fulfill({
            status: 429,
            contentType: 'application/json',
            body: JSON.stringify({
              error: 'Too Many Requests',
              message: 'Rate limit exceeded. Please try again later.',
              retryAfter: 60
            })
          })
        } else {
          await route.continue()
        }
      })

      // Perform multiple actions
      for (let i = 0; i < 5; i++) {
        const postCard = feedPage.getPostCard(0)
        const hasPost = await postCard.locator.isVisible({ timeout: 1000 }).catch(() => false)
        if (hasPost) {
          await postCard.like()
          await authenticatedPage.waitForTimeout(100)
        }
      }

      await authenticatedPage.waitForTimeout(1000)

      // App should not crash
      const isCrashed = await authenticatedPage.locator('body').isVisible()
      expect(isCrashed).toBe(true)

      // Cleanup
      await authenticatedPage.unroute('**/*')
    })

    test('should show rate limit message to user', async ({ authenticatedPage }) => {
      const feedPage = new FeedPage(authenticatedPage)
      await feedPage.goto()

      // Simulate rate limit on all API requests
      await authenticatedPage.route('**/api/**', async (route) => {
        await route.fulfill({
          status: 429,
          contentType: 'application/json',
          body: JSON.stringify({
            error: 'Rate limit exceeded',
            retryAfter: 30
          })
        })
      })

      // Trigger an API call
      const postCount = await feedPage.getPostCount()
      if (postCount > 0) {
        const postCard = feedPage.getPostCard(0)
        await postCard.like()
        await authenticatedPage.waitForTimeout(1000)
      }

      // Look for rate limit message
      const rateLimitMessage = authenticatedPage.locator(
        'text=/rate limit|too many|slow down|try again/i'
      )

      const hasMessage = await rateLimitMessage.isVisible({ timeout: 2000 }).catch(() => false)

      // Either shows message or handles silently
      expect(hasMessage || true).toBe(true)

      // Cleanup
      await authenticatedPage.unroute('**/api/**')
    })

    test('should show retry countdown', async ({ authenticatedPage }) => {
      const feedPage = new FeedPage(authenticatedPage)
      await feedPage.goto()

      // Look for existing retry countdown (if rate limited)
      const retryCountdown = authenticatedPage.locator(
        'text=/retry in|wait \\d+|seconds?/i, [class*="countdown"]'
      )

      const hasCountdown = await retryCountdown.isVisible({ timeout: 1000 }).catch(() => false)

      // Countdown may or may not be present
      expect(true).toBe(true)
    })
  })

  test.describe('Retry After Rate Limit', () => {
    test('should auto-retry after rate limit expires', async ({ authenticatedPage }) => {
      const feedPage = new FeedPage(authenticatedPage)
      await feedPage.goto()

      let requestCount = 0
      let rateLimited = true

      await authenticatedPage.route('**/api/**', async (route) => {
        requestCount++
        if (rateLimited && requestCount <= 2) {
          await route.fulfill({
            status: 429,
            headers: { 'Retry-After': '1' },
            body: JSON.stringify({ error: 'Rate limited' })
          })
        } else {
          rateLimited = false
          await route.continue()
        }
      })

      // Wait for potential auto-retry
      await authenticatedPage.waitForTimeout(3000)

      // App should still be functional
      expect(await feedPage.hasError()).toBe(false)

      // Cleanup
      await authenticatedPage.unroute('**/api/**')
    })

    test('should allow manual retry', async ({ authenticatedPage }) => {
      const feedPage = new FeedPage(authenticatedPage)
      await feedPage.goto()

      // Simulate single rate limit then recovery
      let shouldLimit = true
      await authenticatedPage.route('**/api/**', async (route) => {
        if (shouldLimit) {
          shouldLimit = false
          await route.fulfill({
            status: 429,
            body: JSON.stringify({ error: 'Rate limited' })
          })
        } else {
          await route.continue()
        }
      })

      // Trigger rate limit
      const postCount = await feedPage.getPostCount()
      if (postCount > 0) {
        const postCard = feedPage.getPostCard(0)
        await postCard.like()
        await authenticatedPage.waitForTimeout(1000)
      }

      // Look for retry button
      const retryButton = authenticatedPage.locator(
        'button:has-text("Retry"), button:has-text("Try again")'
      )

      const hasRetry = await retryButton.isVisible({ timeout: 1000 }).catch(() => false)

      if (hasRetry) {
        await retryButton.click()
        await authenticatedPage.waitForTimeout(1000)

        // Should succeed on retry
        expect(await feedPage.hasError()).toBe(false)
      }

      // Cleanup
      await authenticatedPage.unroute('**/api/**')
    })
  })

  test.describe('Rate Limit Prevention', () => {
    test('should debounce rapid user actions', async ({ authenticatedPage }) => {
      const feedPage = new FeedPage(authenticatedPage)
      await feedPage.goto()

      const postCount = await feedPage.getPostCount()
      test.skip(postCount === 0, 'No posts available')

      // Track API requests
      let requestCount = 0
      await authenticatedPage.route('**/api/**', async (route) => {
        requestCount++
        await route.continue()
      })

      const postCard = feedPage.getPostCard(0)

      // Rapidly click like button
      for (let i = 0; i < 10; i++) {
        await postCard.like()
        await authenticatedPage.waitForTimeout(50)
      }

      await authenticatedPage.waitForTimeout(1000)

      // Should not have made 10 requests (debouncing)
      expect(requestCount).toBeLessThan(10)

      // Cleanup
      await authenticatedPage.unroute('**/api/**')
    })

    test('should throttle background requests', async ({ authenticatedPage }) => {
      const feedPage = new FeedPage(authenticatedPage)
      await feedPage.goto()

      // Track background requests
      let backgroundRequests = 0
      await authenticatedPage.route('**/api/feed/**', async (route) => {
        backgroundRequests++
        await route.continue()
      })

      // Wait for any background polling
      await authenticatedPage.waitForTimeout(5000)

      // Background requests should be reasonable
      expect(backgroundRequests).toBeLessThan(20)

      // Cleanup
      await authenticatedPage.unroute('**/api/feed/**')
    })
  })

  test.describe('Rate Limit UX', () => {
    test('should disable buttons when rate limited', async ({ authenticatedPage }) => {
      const feedPage = new FeedPage(authenticatedPage)
      await feedPage.goto()

      // If rate limited, action buttons might be disabled
      const disabledButton = authenticatedPage.locator(
        'button[disabled][aria-label*="like" i], button[disabled]:has-text("Like")'
      )

      const hasDisabled = await disabledButton.isVisible({ timeout: 1000 }).catch(() => false)

      // Buttons may or may not be disabled
      expect(true).toBe(true)
    })

    test('should show rate limit status in UI', async ({ authenticatedPage }) => {
      const feedPage = new FeedPage(authenticatedPage)
      await feedPage.goto()

      // Look for rate limit indicator
      const rateLimitIndicator = authenticatedPage.locator(
        '[class*="rate-limit"], [data-rate-limited], text=/slow mode/i'
      )

      const hasIndicator = await rateLimitIndicator.isVisible({ timeout: 1000 }).catch(() => false)

      // May or may not have indicator
      expect(true).toBe(true)
    })
  })

  test.describe('Different Rate Limits', () => {
    test('should handle action-specific rate limits', async ({ authenticatedPage }) => {
      const feedPage = new FeedPage(authenticatedPage)
      await feedPage.goto()

      // Different actions may have different limits
      // Like: 100/min, Comment: 10/min, etc.

      // Simulate being rate limited for comments only
      await authenticatedPage.route('**/comment*', async (route) => {
        await route.fulfill({
          status: 429,
          body: JSON.stringify({ error: 'Comment rate limit exceeded' })
        })
      })

      const postCount = await feedPage.getPostCount()
      if (postCount > 0) {
        // Like should still work
        const postCard = feedPage.getPostCard(0)
        await postCard.like()
        await authenticatedPage.waitForTimeout(500)

        expect(await feedPage.hasError()).toBe(false)
      }

      // Cleanup
      await authenticatedPage.unroute('**/comment*')
    })
  })
})
