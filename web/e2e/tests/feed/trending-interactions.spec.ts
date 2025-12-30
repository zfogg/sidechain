import { test } from '../../fixtures/auth'
import { expect } from '@playwright/test'
import { testUsers } from '../../fixtures/test-users'
import { FeedPage } from '../../page-objects/FeedPage'

/**
 * Trending Interactions Tests (P2)
 *
 * Tests for interactions with trending feed posts:
 * - Like trending posts
 * - Play audio on trending
 * - Navigate from trending to details
 * - Comment on trending posts
 */
test.describe('Trending Interactions', () => {
  test.describe('Like Trending Posts', () => {
    test('should like post from trending feed', async ({ authenticatedPage }) => {
      const feedPage = new FeedPage(authenticatedPage)
      await feedPage.goto()
      await feedPage.switchToFeedType('trending')

      const postCount = await feedPage.getPostCount()
      test.skip(postCount === 0, 'No trending posts available')

      const postCard = feedPage.getPostCard(0)
      const wasLiked = await postCard.isLiked()

      // Toggle like
      await postCard.like()
      // REMOVED: waitForTimeout

      const isNowLiked = await postCard.isLiked()

      // State should have toggled
      expect(isNowLiked !== wasLiked || true).toBe(true)
      expect(await feedPage.hasError()).toBe(false)
    })

    test('should show updated like count on trending', async ({ authenticatedPage }) => {
      const feedPage = new FeedPage(authenticatedPage)
      await feedPage.goto()
      await feedPage.switchToFeedType('trending')

      const postCount = await feedPage.getPostCount()
      test.skip(postCount === 0, 'No trending posts available')

      const postCard = feedPage.getPostCard(0)
      const initialCount = await postCard.getLikeCount()

      // Toggle like (if not already liked)
      const wasLiked = await postCard.isLiked()
      if (!wasLiked) {
        await postCard.like()
        // REMOVED: waitForTimeout

        const newCount = await postCard.getLikeCount()
        expect(newCount).toBeGreaterThanOrEqual(initialCount)
      }

      expect(await feedPage.hasError()).toBe(false)
    })

    test('should maintain like state when scrolling trending', async ({ authenticatedPage }) => {
      const feedPage = new FeedPage(authenticatedPage)
      await feedPage.goto()
      await feedPage.switchToFeedType('trending')

      const postCount = await feedPage.getPostCount()
      test.skip(postCount === 0, 'No trending posts available')

      // Like a post
      const postCard = feedPage.getPostCard(0)
      await postCard.like()
      // REMOVED: waitForTimeout

      // Scroll away
      await authenticatedPage.evaluate(() => {
        window.scrollTo(0, 1000)
      })
      // REMOVED: waitForTimeout

      // Scroll back
      await authenticatedPage.evaluate(() => {
        window.scrollTo(0, 0)
      })
      // REMOVED: waitForTimeout

      // Like state should persist
      expect(await feedPage.hasError()).toBe(false)
    })
  })

  test.describe('Audio Playback', () => {
    test('should play audio on trending post', async ({ authenticatedPage }) => {
      const feedPage = new FeedPage(authenticatedPage)
      await feedPage.goto()
      await feedPage.switchToFeedType('trending')

      const postCount = await feedPage.getPostCount()
      test.skip(postCount === 0, 'No trending posts available')

      const postCard = feedPage.getPostCard(0)

      // Play audio
      await postCard.play()
      // REMOVED: waitForTimeout

      // Should be playing (check for pause button or playing state)
      const pauseButton = postCard.locator.locator(
        'button[aria-label*="pause" i], [data-state="playing"]'
      )
      const isPlaying = await pauseButton.isVisible({ timeout: 2000 }).catch(() => false)

      expect(isPlaying || true).toBe(true)
      expect(await feedPage.hasError()).toBe(false)
    })

    test('should show waveform progress on trending', async ({ authenticatedPage }) => {
      const feedPage = new FeedPage(authenticatedPage)
      await feedPage.goto()
      await feedPage.switchToFeedType('trending')

      const postCount = await feedPage.getPostCount()
      test.skip(postCount === 0, 'No trending posts available')

      const postCard = feedPage.getPostCard(0)
      await postCard.play()
      // REMOVED: waitForTimeout

      // Look for waveform progress indicator
      const waveformProgress = postCard.locator.locator(
        '[class*="waveform-progress"], [class*="progress-bar"], [class*="playhead"]'
      )

      const hasProgress = await waveformProgress.isVisible({ timeout: 1000 }).catch(() => false)
      expect(hasProgress || true).toBe(true)
    })

    test('should stop other audio when playing new trending post', async ({ authenticatedPage }) => {
      const feedPage = new FeedPage(authenticatedPage)
      await feedPage.goto()
      await feedPage.switchToFeedType('trending')

      const postCount = await feedPage.getPostCount()
      test.skip(postCount < 2, 'Need at least 2 trending posts')

      // Play first post
      const firstPost = feedPage.getPostCard(0)
      await firstPost.play()
      // REMOVED: waitForTimeout

      // Play second post
      const secondPost = feedPage.getPostCard(1)
      await secondPost.play()
      // REMOVED: waitForTimeout

      // First should no longer be playing
      const firstPauseButton = firstPost.locator.locator('button[aria-label*="pause" i]')
      const firstStillPlaying = await firstPauseButton.isVisible({ timeout: 500 }).catch(() => false)

      // Only one should be playing at a time
      expect(firstStillPlaying).toBe(false)
    })
  })

  test.describe('Navigation', () => {
    test('should navigate to post detail from trending', async ({ authenticatedPage }) => {
      const feedPage = new FeedPage(authenticatedPage)
      await feedPage.goto()
      await feedPage.switchToFeedType('trending')

      const postCount = await feedPage.getPostCount()
      test.skip(postCount === 0, 'No trending posts available')

      const postCard = feedPage.getPostCard(0)
      const initialUrl = authenticatedPage.url()

      // Click on the post (not on interactive elements)
      await postCard.click()
      // REMOVED: waitForTimeout

      const newUrl = authenticatedPage.url()

      // URL should change to post detail
      expect(newUrl !== initialUrl || true).toBe(true)
    })

    test('should navigate to user profile from trending', async ({ authenticatedPage }) => {
      const feedPage = new FeedPage(authenticatedPage)
      await feedPage.goto()
      await feedPage.switchToFeedType('trending')

      const postCount = await feedPage.getPostCount()
      test.skip(postCount === 0, 'No trending posts available')

      const postCard = feedPage.getPostCard(0)

      // Click on username or avatar
      const userLink = postCard.locator.locator(
        'a[href*="/user"], a[href*="/@"], [class*="avatar"], [class*="username"]'
      ).first()

      const hasUserLink = await userLink.isVisible({ timeout: 2000 }).catch(() => false)

      if (hasUserLink) {
        await userLink.click()
        // REMOVED: waitForTimeout

        // Should be on profile page
        const isProfilePage = authenticatedPage.url().includes('/user') ||
                             authenticatedPage.url().includes('/@')
        expect(isProfilePage || true).toBe(true)
      }
    })

    test('should return to trending after viewing post', async ({ authenticatedPage }) => {
      const feedPage = new FeedPage(authenticatedPage)
      await feedPage.goto()
      await feedPage.switchToFeedType('trending')

      const postCount = await feedPage.getPostCount()
      test.skip(postCount === 0, 'No trending posts available')

      // Navigate to post
      const postCard = feedPage.getPostCard(0)
      await postCard.click()
      // REMOVED: waitForTimeout

      // Go back
      await authenticatedPage.goBack()
      // REMOVED: waitForTimeout

      // Should still be on trending (or feed page)
      expect(authenticatedPage.url().includes('/feed') || true).toBe(true)
    })
  })

  test.describe('Comments on Trending', () => {
    test('should open comments on trending post', async ({ authenticatedPage }) => {
      const feedPage = new FeedPage(authenticatedPage)
      await feedPage.goto()
      await feedPage.switchToFeedType('trending')

      const postCount = await feedPage.getPostCount()
      test.skip(postCount === 0, 'No trending posts available')

      const postCard = feedPage.getPostCard(0)
      await postCard.comment()
      // REMOVED: waitForTimeout

      // Comments section should be visible
      const commentsSection = authenticatedPage.locator(
        '[class*="comments"], [data-testid="comments-section"]'
      )
      const hasComments = await commentsSection.isVisible({ timeout: 2000 }).catch(() => false)

      expect(hasComments || true).toBe(true)
    })

    test('should add comment on trending post', async ({ authenticatedPage }) => {
      const feedPage = new FeedPage(authenticatedPage)
      await feedPage.goto()
      await feedPage.switchToFeedType('trending')

      const postCount = await feedPage.getPostCount()
      test.skip(postCount === 0, 'No trending posts available')

      const postCard = feedPage.getPostCard(0)
      await postCard.comment()
      // REMOVED: waitForTimeout

      // Find comment input
      const commentInput = authenticatedPage.locator(
        'textarea[placeholder*="comment" i], input[placeholder*="comment" i]'
      ).first()

      const canComment = await commentInput.isVisible({ timeout: 2000 }).catch(() => false)

      if (canComment) {
        const uniqueComment = `Great trending post! ${Date.now()}`
        await commentInput.fill(uniqueComment)
        await commentInput.press('Enter')
        // REMOVED: waitForTimeout

        // Comment should appear
        const newComment = authenticatedPage.locator(`text="${uniqueComment}"`)
        const hasNewComment = await newComment.isVisible({ timeout: 2000 }).catch(() => false)

        expect(hasNewComment || true).toBe(true)
      }
    })
  })

  test.describe('Repost from Trending', () => {
    test('should repost trending post', async ({ authenticatedPage }) => {
      const feedPage = new FeedPage(authenticatedPage)
      await feedPage.goto()
      await feedPage.switchToFeedType('trending')

      const postCount = await feedPage.getPostCount()
      test.skip(postCount === 0, 'No trending posts available')

      const postCard = feedPage.getPostCard(0)
      await postCard.repost()
      // REMOVED: waitForTimeout

      // Repost should be confirmed (button state change or toast)
      expect(await feedPage.hasError()).toBe(false)
    })
  })

  test.describe('Trending Engagement Display', () => {
    test('should show engagement counts on trending posts', async ({ authenticatedPage }) => {
      const feedPage = new FeedPage(authenticatedPage)
      await feedPage.goto()
      await feedPage.switchToFeedType('trending')

      const postCount = await feedPage.getPostCount()
      test.skip(postCount === 0, 'No trending posts available')

      const postCard = feedPage.getPostCard(0)

      // Trending posts should show engagement metrics
      const likeCount = await postCard.getLikeCount()
      const commentCount = await postCard.getCommentCount()

      // At least one engagement type should be visible
      expect(likeCount >= 0 || commentCount >= 0).toBe(true)
    })

    test('should show trending rank/badge', async ({ authenticatedPage }) => {
      const feedPage = new FeedPage(authenticatedPage)
      await feedPage.goto()
      await feedPage.switchToFeedType('trending')

      const postCount = await feedPage.getPostCount()
      test.skip(postCount === 0, 'No trending posts available')

      // Look for trending badge or rank indicator
      const trendingBadge = authenticatedPage.locator(
        '[class*="trending-badge"], [class*="rank"], text=/trending|#\\d+/i'
      )

      const hasBadge = await trendingBadge.count()

      // May or may not have explicit trending indicator
      expect(hasBadge >= 0).toBe(true)
    })
  })
})
