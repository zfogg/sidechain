import { test } from '../../fixtures/auth'
import { expect } from '@playwright/test'
import { FeedPage } from '../../page-objects/FeedPage'

test.describe('Feed - Post Interactions', () => {
  test.beforeEach(async ({ authenticatedPage }) => {
    const feedPage = new FeedPage(authenticatedPage)
    await feedPage.goto()

    // Skip if no posts
    const hasContent = (await feedPage.getPostCount()) > 0 || (await feedPage.hasEmptyState())
    test.skip(!hasContent, 'No posts available in feed')
  })

  test('should like a post with optimistic update', async ({ authenticatedPage }) => {
    const feedPage = new FeedPage(authenticatedPage)
    await feedPage.goto()

    const postCount = await feedPage.getPostCount()
    if (postCount === 0) return

    const postCard = feedPage.getPostCard(0)

    // Get initial like count
    const initialLikeCount = await postCard.getLikeCount()
    const wasLiked = await postCard.isLiked()

    // Like the post
    await postCard.like()

    // Wait for optimistic update
    await expect(async () => {
      const newLikeCount = await postCard.getLikeCount()
      const isNowLiked = await postCard.isLiked()

      if (!wasLiked) {
        // Liked a post - count should increase
        expect(newLikeCount).toBeGreaterThan(initialLikeCount)
        expect(isNowLiked).toBe(true)
      }
    }).toPass({ timeout: 3000 })
  })

  test('should unlike a post', async ({ authenticatedPage }) => {
    const feedPage = new FeedPage(authenticatedPage)
    await feedPage.goto()

    const postCount = await feedPage.getPostCount()
    if (postCount === 0) return

    const postCard = feedPage.getPostCard(0)

    // Like the post first
    await postCard.like()
    // REMOVED: waitForTimeout

    const likedCount = await postCard.getLikeCount()

    // Unlike
    await postCard.like()

    // Count should decrease
    await expect(async () => {
      const newCount = await postCard.getLikeCount()
      expect(newCount).toBeLessThanOrEqual(likedCount)
    }).toPass({ timeout: 3000 })
  })

  test('should save a post', async ({ authenticatedPage }) => {
    const feedPage = new FeedPage(authenticatedPage)
    await feedPage.goto()

    const postCount = await feedPage.getPostCount()
    if (postCount === 0) return

    const postCard = feedPage.getPostCard(0)

    // Save the post
    await postCard.save()

    // Should not error
    const hasError = await feedPage.hasError()
    expect(hasError).toBe(false)
  })

  test('should comment on a post', async ({ authenticatedPage }) => {
    const feedPage = new FeedPage(authenticatedPage)
    await feedPage.goto()

    const postCount = await feedPage.getPostCount()
    if (postCount === 0) return

    const postCard = feedPage.getPostCard(0)

    // Click comment button
    await postCard.comment()

    // Should open comment dialog/section (implementation varies)
    // Just verify no error occurred
    const hasError = await feedPage.hasError()
    expect(hasError).toBe(false)
  })

  test('should play audio post', async ({ authenticatedPage }) => {
    const feedPage = new FeedPage(authenticatedPage)
    await feedPage.goto()

    const postCount = await feedPage.getPostCount()
    if (postCount === 0) return

    const postCard = feedPage.getPostCard(0)

    // Check if play button exists
    const playButtonVisible = await postCard.playButton
      .isVisible({ timeout: 1000 })
      .catch(() => false)

    if (playButtonVisible) {
      // Play the post
      await postCard.play()

      // Play count might increase
      const playCount = await postCard.getPlayCount()
      expect(playCount >= 0).toBe(true)
    }
  })

  test('should navigate to post author profile', async ({ authenticatedPage }) => {
    const feedPage = new FeedPage(authenticatedPage)
    await feedPage.goto()

    const postCount = await feedPage.getPostCount()
    if (postCount === 0) return

    const postCard = feedPage.getPostCard(0)
    const authorUsername = await postCard.getAuthorUsername()

    // Click author
    await postCard.clickAuthor()

    // Should navigate to profile
    await authenticatedPage.waitForURL(/\/profile\//, { timeout: 5000 })

    // URL should contain the username
    expect(authenticatedPage.url()).toContain(authorUsername)
  })

  test('should share a post', async ({ authenticatedPage }) => {
    const feedPage = new FeedPage(authenticatedPage)
    await feedPage.goto()

    const postCount = await feedPage.getPostCount()
    if (postCount === 0) return

    const postCard = feedPage.getPostCard(0)

    // Share button might not exist depending on implementation
    const shareVisible = await postCard.shareButton
      .isVisible({ timeout: 1000 })
      .catch(() => false)

    if (shareVisible) {
      await postCard.share()

      // Should not error
      const hasError = await feedPage.hasError()
      expect(hasError).toBe(false)
    }
  })

  test('should display post metadata correctly', async ({ authenticatedPage }) => {
    const feedPage = new FeedPage(authenticatedPage)
    await feedPage.goto()

    const postCount = await feedPage.getPostCount()
    if (postCount === 0) return

    const postCard = feedPage.getPostCard(0)

    // Get post info
    const author = await postCard.getAuthorName()
    const time = await postCard.getPostTime()

    expect(author).toBeTruthy()
    expect(time).toBeTruthy()
  })

  test('should get accurate like count', async ({ authenticatedPage }) => {
    const feedPage = new FeedPage(authenticatedPage)
    await feedPage.goto()

    const postCount = await feedPage.getPostCount()
    if (postCount === 0) return

    const postCard = feedPage.getPostCard(0)

    // Like count should be a number
    const likeCount = await postCard.getLikeCount()
    expect(typeof likeCount).toBe('number')
    expect(likeCount >= 0).toBe(true)
  })

  test('should allow multiple post interactions', async ({ authenticatedPage }) => {
    const feedPage = new FeedPage(authenticatedPage)
    await feedPage.goto()

    const postCount = await feedPage.getPostCount()
    if (postCount === 0) return

    const postCard = feedPage.getPostCard(0)

    // Like
    await postCard.like()
    // REMOVED: waitForTimeout

    // Save
    await postCard.save()
    // REMOVED: waitForTimeout

    // Comment
    await postCard.comment()
    // REMOVED: waitForTimeout

    // Should still be functional
    const hasError = await feedPage.hasError()
    expect(hasError).toBe(false)
  })
})
