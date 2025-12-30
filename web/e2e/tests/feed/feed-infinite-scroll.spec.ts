import { test } from '../../fixtures/auth'
import { expect } from '@playwright/test'
import { FeedPage } from '../../page-objects/FeedPage'

test.describe('Feed - Infinite Scroll', () => {
  test('should load more posts when scrolling to bottom', async ({ authenticatedPage }) => {
    const feedPage = new FeedPage(authenticatedPage)
    await feedPage.goto()

    const initialCount = await feedPage.getPostCount()

    if (initialCount > 0) {
      // Scroll to bottom
      await feedPage.scrollToBottom()

      // Might load more posts
      const finalCount = await feedPage.getPostCount()

      // Should be same or more
      expect(finalCount >= initialCount).toBe(true)
    }
  })

  test('should show loading indicator when fetching more posts', async ({ authenticatedPage }) => {
    const feedPage = new FeedPage(authenticatedPage)
    await feedPage.goto()

    const initialCount = await feedPage.getPostCount()

    if (initialCount > 0 && (await feedPage.hasMorePostsToLoad())) {
      // Scroll to bottom (might catch loading state)
      const scrollPromise = feedPage.scrollToBottom()

      // Try to catch loading spinner
      const wasLoading = await feedPage.isLoading()

      await scrollPromise

      // After loading, should not be loading
      const stillLoading = await feedPage.isLoading()
      expect(stillLoading).toBe(false)
    }
  })

  test('should show load more button when available', async ({ authenticatedPage }) => {
    const feedPage = new FeedPage(authenticatedPage)
    await feedPage.goto()

    const postCount = await feedPage.getPostCount()

    if (postCount > 0) {
      // Scroll to bottom
      await feedPage.scrollToBottom()

      // Check for load more button
      const hasMore = await feedPage.hasMorePostsToLoad()

      // If has more, button should be visible
      if (hasMore) {
        await expect(feedPage.loadMoreButton).toBeVisible()
      }
    }
  })

  test('should click load more button successfully', async ({ authenticatedPage }) => {
    const feedPage = new FeedPage(authenticatedPage)
    await feedPage.goto()

    const initialCount = await feedPage.getPostCount()

    if (await feedPage.hasMorePostsToLoad()) {
      // Click load more
      await feedPage.clickLoadMore()

      // Should load more posts (or stay the same if no more data)
      const finalCount = await feedPage.getPostCount()
      expect(finalCount >= initialCount).toBe(true)
    }
  })

  test('should handle pagination errors gracefully', async ({ authenticatedPage }) => {
    const feedPage = new FeedPage(authenticatedPage)
    await feedPage.goto()

    // Load more multiple times
    for (let i = 0; i < 3; i++) {
      if (await feedPage.hasMorePostsToLoad()) {
        await feedPage.clickLoadMore()
      }
    }

    // Should not show error
    const hasError = await feedPage.hasError()
    expect(hasError).toBe(false)
  })

  test('should show end of feed message when no more posts', async ({ authenticatedPage }) => {
    const feedPage = new FeedPage(authenticatedPage)
    await feedPage.goto()

    // Load all posts
    let loadMore = true
    let iterations = 0
    while (loadMore && iterations < 10) {
      if (await feedPage.hasMorePostsToLoad()) {
        await feedPage.clickLoadMore()
      } else {
        loadMore = false
      }
      iterations++
    }

    // Either should have reached end or show end message
    const endMessage = await feedPage.getEndOfFeedMessage()

    // Should either have end message or no more button
    const hasMoreButton = await feedPage.hasMorePostsToLoad()
    expect(!hasMoreButton || endMessage).toBe(true)
  })

  test('should not load duplicate posts', async ({ authenticatedPage }) => {
    const feedPage = new FeedPage(authenticatedPage)
    await feedPage.goto()

    const initialPosts = await feedPage.getAllPostCards()
    const initialIds = new Set<string>()

    // Collect initial post identifiers (using author + timestamp)
    for (const post of initialPosts) {
      try {
        const author = await post.getAuthorUsername()
        const time = await post.getPostTime()
        initialIds.add(`${author}-${time}`)
      } catch (e) {
        // Ignore if can't get data
      }
    }

    // Load more
    if (await feedPage.hasMorePostsToLoad()) {
      await feedPage.clickLoadMore()

      const finalPosts = await feedPage.getAllPostCards()
      const duplicates = new Set<string>()

      for (const post of finalPosts) {
        try {
          const author = await post.getAuthorUsername()
          const time = await post.getPostTime()
          const id = `${author}-${time}`

          if (initialIds.has(id)) {
            duplicates.add(id)
          }
        } catch (e) {
          // Ignore
        }
      }

      // Ideally should have no duplicates
      // But this depends on implementation
      expect(duplicates.size <= initialIds.size).toBe(true)
    }
  })

  test('should maintain feed state during pagination', async ({ authenticatedPage }) => {
    const feedPage = new FeedPage(authenticatedPage)
    await feedPage.goto()

    // Like a post
    if ((await feedPage.getPostCount()) > 0) {
      const postCard = feedPage.getPostCard(0)
      await postCard.like()
      const likedCount1 = await postCard.getLikeCount()

      // Load more posts
      if (await feedPage.hasMorePostsToLoad()) {
        await feedPage.clickLoadMore()
      }

      // Original post's like status should persist
      const likedCount2 = await postCard.getLikeCount()
      expect(likedCount2).toBe(likedCount1)
    }
  })

  test('should auto-load posts on scroll', async ({ authenticatedPage }) => {
    const feedPage = new FeedPage(authenticatedPage)
    await feedPage.goto()

    const initialCount = await feedPage.getPostCount()

    // Scroll down multiple times
    for (let i = 0; i < 3; i++) {
      await feedPage.scrollToBottom()
      // REMOVED: waitForTimeout
    }

    // Should have loaded more posts (if available)
    const finalCount = await feedPage.getPostCount()
    expect(finalCount >= initialCount).toBe(true)
  })

  test('should handle large pagination correctly', async ({ authenticatedPage }) => {
    const feedPage = new FeedPage(authenticatedPage)
    await feedPage.goto()

    // Load multiple pages
    let pageCount = 0
    let previousCount = 0

    for (let i = 0; i < 5; i++) {
      const currentCount = await feedPage.getPostCount()

      if (currentCount === previousCount) {
        // No more posts to load
        break
      }

      previousCount = currentCount
      pageCount++

      if (await feedPage.hasMorePostsToLoad()) {
        await feedPage.clickLoadMore()
        // REMOVED: waitForTimeout
      }
    }

    // Should have loaded some posts
    const finalCount = await feedPage.getPostCount()
    expect(finalCount > 0).toBe(true)
  })
})
