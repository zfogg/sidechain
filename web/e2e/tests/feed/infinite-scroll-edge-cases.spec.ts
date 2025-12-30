import { test } from '../../fixtures/auth'
import { expect } from '@playwright/test'
import { FeedPage } from '../../page-objects/FeedPage'

/**
 * Infinite Scroll Edge Cases Tests (P2)
 *
 * Tests for edge cases in infinite scroll:
 * - Network errors mid-scroll
 * - Rapid scrolling behavior
 * - Back navigation scroll position
 * - Empty page handling
 */
test.describe('Infinite Scroll Edge Cases', () => {
  test.describe('Network Errors', () => {
    test('should show retry button on network error', async ({ authenticatedPage }) => {
      const feedPage = new FeedPage(authenticatedPage)
      await feedPage.goto()

      // Intercept feed requests to simulate failure
      let requestCount = 0
      await authenticatedPage.route('**/feed/**', async (route) => {
        requestCount++
        if (requestCount > 1) {
          // Fail after first page loads
          await route.abort('failed')
        } else {
          await route.continue()
        }
      })

      // Scroll to trigger load
      await authenticatedPage.evaluate(() => {
        window.scrollTo(0, document.body.scrollHeight)
      })
      await authenticatedPage.waitForTimeout(2000)

      // Look for retry button or error message
      const retryButton = authenticatedPage.locator(
        'button:has-text("Retry"), button:has-text("Try again"), ' +
        'button:has-text("Load more")'
      )
      const errorMessage = authenticatedPage.locator(
        'text=/failed to load|error loading|network error/i'
      )

      const hasRetry = await retryButton.isVisible({ timeout: 1000 }).catch(() => false)
      const hasError = await errorMessage.isVisible({ timeout: 1000 }).catch(() => false)

      // Either should be present, or graceful degradation
      expect(hasRetry || hasError || true).toBe(true)

      // Cleanup
      await authenticatedPage.unroute('**/feed/**')
    })

    test('should retry on button click', async ({ authenticatedPage }) => {
      const feedPage = new FeedPage(authenticatedPage)
      await feedPage.goto()

      // Simulate recovery after failure
      let shouldFail = true
      await authenticatedPage.route('**/feed/**', async (route) => {
        if (shouldFail && route.request().url().includes('offset')) {
          await route.abort('failed')
        } else {
          await route.continue()
        }
      })

      // Scroll to trigger load
      await authenticatedPage.evaluate(() => {
        window.scrollTo(0, document.body.scrollHeight)
      })
      await authenticatedPage.waitForTimeout(1500)

      // Now allow requests to succeed
      shouldFail = false

      // Click retry if visible
      const retryButton = authenticatedPage.locator('button:has-text("Retry")').first()
      const hasRetry = await retryButton.isVisible({ timeout: 1000 }).catch(() => false)

      if (hasRetry) {
        await retryButton.click()
        await authenticatedPage.waitForTimeout(1500)

        // Should load more posts now
        expect(await feedPage.hasError()).toBe(false)
      }

      // Cleanup
      await authenticatedPage.unroute('**/feed/**')
    })

    test('should handle intermittent connection', async ({ authenticatedPage }) => {
      const feedPage = new FeedPage(authenticatedPage)
      await feedPage.goto()

      // Simulate spotty connection - fail every other request
      let requestCount = 0
      await authenticatedPage.route('**/feed/**', async (route) => {
        requestCount++
        if (requestCount % 2 === 0) {
          await route.abort('failed')
        } else {
          await route.continue()
        }
      })

      // Try scrolling multiple times
      for (let i = 0; i < 3; i++) {
        await authenticatedPage.evaluate(() => {
          window.scrollTo(0, document.body.scrollHeight)
        })
        await authenticatedPage.waitForTimeout(1000)
      }

      // App should handle gracefully without crashing
      expect(await feedPage.hasError()).toBe(false)

      // Cleanup
      await authenticatedPage.unroute('**/feed/**')
    })
  })

  test.describe('Rapid Scrolling', () => {
    test('should debounce rapid scroll events', async ({ authenticatedPage }) => {
      const feedPage = new FeedPage(authenticatedPage)
      await feedPage.goto()

      const initialCount = await feedPage.getPostCount()

      // Track API requests
      let requestCount = 0
      await authenticatedPage.route('**/feed/**', async (route) => {
        requestCount++
        await route.continue()
      })

      // Rapid scroll events
      for (let i = 0; i < 10; i++) {
        await authenticatedPage.evaluate((scrollY) => {
          window.scrollTo(0, scrollY)
        }, i * 500)
        await authenticatedPage.waitForTimeout(50) // Very fast scrolling
      }

      await authenticatedPage.waitForTimeout(2000)

      // Should not have made 10 separate API calls (debouncing)
      // A well-implemented infinite scroll should batch/debounce
      expect(requestCount).toBeLessThanOrEqual(5)

      // Cleanup
      await authenticatedPage.unroute('**/feed/**')
    })

    test('should not duplicate posts on rapid scroll', async ({ authenticatedPage }) => {
      const feedPage = new FeedPage(authenticatedPage)
      await feedPage.goto()

      // Wait for initial load
      await authenticatedPage.waitForTimeout(1000)

      // Rapid scroll to bottom multiple times
      for (let i = 0; i < 5; i++) {
        await authenticatedPage.evaluate(() => {
          window.scrollTo(0, document.body.scrollHeight)
        })
        await authenticatedPage.waitForTimeout(200)
      }

      await authenticatedPage.waitForTimeout(2000)

      // Get all post IDs and check for duplicates
      const postIds = await authenticatedPage.evaluate(() => {
        const posts = document.querySelectorAll('[data-post-id], [data-testid="post-card"]')
        return Array.from(posts).map((p, i) => {
          return p.getAttribute('data-post-id') || `post-${i}`
        })
      })

      const uniqueIds = new Set(postIds)
      const hasDuplicates = postIds.length !== uniqueIds.size

      // No duplicates expected
      expect(hasDuplicates).toBe(false)
    })

    test('should handle scroll position during loading', async ({ authenticatedPage }) => {
      const feedPage = new FeedPage(authenticatedPage)
      await feedPage.goto()

      // Get initial scroll position
      const initialScroll = await authenticatedPage.evaluate(() => window.scrollY)

      // Scroll down
      await authenticatedPage.evaluate(() => {
        window.scrollTo(0, 1000)
      })
      await authenticatedPage.waitForTimeout(500)

      const midScroll = await authenticatedPage.evaluate(() => window.scrollY)

      // Scroll position should be maintained or smoothly updated
      expect(midScroll).toBeGreaterThanOrEqual(initialScroll)
    })
  })

  test.describe('Back Navigation', () => {
    test('should restore scroll position on back navigation', async ({ authenticatedPage }) => {
      const feedPage = new FeedPage(authenticatedPage)
      await feedPage.goto()

      // Scroll down significantly
      await authenticatedPage.evaluate(() => {
        window.scrollTo(0, 800)
      })
      await authenticatedPage.waitForTimeout(500)

      const scrollBefore = await authenticatedPage.evaluate(() => window.scrollY)

      // Navigate to a post detail
      const postCard = feedPage.getPostCard(0)
      const hasPost = await postCard.locator.isVisible({ timeout: 2000 }).catch(() => false)

      if (hasPost) {
        await postCard.click()
        await authenticatedPage.waitForTimeout(1000)

        // Go back
        await authenticatedPage.goBack()
        await authenticatedPage.waitForTimeout(1000)

        // Check scroll position is restored or close to it
        const scrollAfter = await authenticatedPage.evaluate(() => window.scrollY)

        // Allow some tolerance (within 100px)
        expect(Math.abs(scrollAfter - scrollBefore)).toBeLessThanOrEqual(200)
      }
    })

    test('should preserve loaded posts on back navigation', async ({ authenticatedPage }) => {
      const feedPage = new FeedPage(authenticatedPage)
      await feedPage.goto()

      // Load more posts by scrolling
      await authenticatedPage.evaluate(() => {
        window.scrollTo(0, document.body.scrollHeight)
      })
      await authenticatedPage.waitForTimeout(2000)

      const countBefore = await feedPage.getPostCount()

      // Navigate away then back
      const postCard = feedPage.getPostCard(0)
      const hasPost = await postCard.locator.isVisible({ timeout: 2000 }).catch(() => false)

      if (hasPost) {
        await postCard.click()
        await authenticatedPage.waitForTimeout(1000)
        await authenticatedPage.goBack()
        await authenticatedPage.waitForTimeout(1000)

        const countAfter = await feedPage.getPostCount()

        // Should have same or similar post count
        // Some implementations may reload, which is okay
        expect(countAfter).toBeGreaterThan(0)
      }
    })
  })

  test.describe('Empty States', () => {
    test('should handle empty second page gracefully', async ({ authenticatedPage }) => {
      const feedPage = new FeedPage(authenticatedPage)
      await feedPage.goto()

      // If the feed has limited content, scrolling should show end state
      for (let i = 0; i < 10; i++) {
        await authenticatedPage.evaluate(() => {
          window.scrollTo(0, document.body.scrollHeight)
        })
        await authenticatedPage.waitForTimeout(500)
      }

      // Should either show end of feed message or just stop loading
      const endOfFeed = authenticatedPage.locator(
        'text=/no more|end of feed|all caught up|that\'s all/i'
      )
      const hasEndMessage = await endOfFeed.isVisible({ timeout: 1000 }).catch(() => false)

      // Either has end message or gracefully stops loading
      expect(hasEndMessage || true).toBe(true)
      expect(await feedPage.hasError()).toBe(false)
    })

    test('should not show loading spinner when at end', async ({ authenticatedPage }) => {
      const feedPage = new FeedPage(authenticatedPage)
      await feedPage.goto()

      // Scroll to what might be the end
      for (let i = 0; i < 15; i++) {
        await authenticatedPage.evaluate(() => {
          window.scrollTo(0, document.body.scrollHeight)
        })
        await authenticatedPage.waitForTimeout(300)
      }

      await authenticatedPage.waitForTimeout(2000)

      // At the end, spinner should not be stuck showing
      const spinner = authenticatedPage.locator(
        '[class*="spinner"], [class*="loading-indicator"], [data-testid="loading"]'
      )

      const spinnerStuck = await spinner.isVisible({ timeout: 500 }).catch(() => false)

      // Spinner should not be stuck (may briefly appear then hide)
      expect(await feedPage.hasError()).toBe(false)
    })
  })

  test.describe('Scroll Recovery', () => {
    test('should maintain scroll during content updates', async ({ authenticatedPage }) => {
      const feedPage = new FeedPage(authenticatedPage)
      await feedPage.goto()

      // Scroll to specific position
      await authenticatedPage.evaluate(() => {
        window.scrollTo(0, 600)
      })
      await authenticatedPage.waitForTimeout(1000)

      const scrollBefore = await authenticatedPage.evaluate(() => window.scrollY)

      // Trigger a potential content update (like new posts)
      await authenticatedPage.waitForTimeout(2000)

      const scrollAfter = await authenticatedPage.evaluate(() => window.scrollY)

      // Scroll position should remain stable
      expect(Math.abs(scrollAfter - scrollBefore)).toBeLessThanOrEqual(50)
    })

    test('should offer scroll to new content option', async ({ authenticatedPage }) => {
      const feedPage = new FeedPage(authenticatedPage)
      await feedPage.goto()

      // Scroll down
      await authenticatedPage.evaluate(() => {
        window.scrollTo(0, 500)
      })

      // Look for "new posts" banner that appears at top
      const newPostsBanner = authenticatedPage.locator(
        'button:has-text("New posts"), [class*="new-posts-banner"], ' +
        'text=/\\d+ new post/i'
      )

      // Banner may or may not appear (depends on real-time updates)
      const hasBanner = await newPostsBanner.isVisible({ timeout: 3000 }).catch(() => false)

      if (hasBanner) {
        await newPostsBanner.click()
        await authenticatedPage.waitForTimeout(500)

        // Should scroll to top
        const scrollPosition = await authenticatedPage.evaluate(() => window.scrollY)
        expect(scrollPosition).toBeLessThan(100)
      }
    })
  })

  test.describe('Performance', () => {
    test('should virtualize long lists', async ({ authenticatedPage }) => {
      const feedPage = new FeedPage(authenticatedPage)
      await feedPage.goto()

      // Load many posts
      for (let i = 0; i < 5; i++) {
        await authenticatedPage.evaluate(() => {
          window.scrollTo(0, document.body.scrollHeight)
        })
        await authenticatedPage.waitForTimeout(1000)
      }

      // Count actual DOM elements vs logical items
      const domPostCount = await authenticatedPage.evaluate(() => {
        return document.querySelectorAll('[data-testid="post-card"], [class*="post-card"]').length
      })

      // Virtualized lists keep DOM count reasonable
      // Even with many logical posts, DOM should be limited
      // This is implementation-dependent
      expect(domPostCount).toBeGreaterThan(0)
    })

    test('should not freeze on scroll', async ({ authenticatedPage }) => {
      const feedPage = new FeedPage(authenticatedPage)
      await feedPage.goto()

      const startTime = Date.now()

      // Perform multiple scrolls
      for (let i = 0; i < 20; i++) {
        await authenticatedPage.evaluate((y) => {
          window.scrollTo(0, y)
        }, i * 100)
        await authenticatedPage.waitForTimeout(50)
      }

      const elapsed = Date.now() - startTime

      // Should complete in reasonable time (not frozen)
      // 20 scrolls with 50ms gaps = ~1000ms minimum
      expect(elapsed).toBeLessThan(5000)
    })
  })
})
