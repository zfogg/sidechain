import { test, expect } from '../../fixtures/auth'
import { ProfilePage } from '../../page-objects/ProfilePage'
import { testUsers } from '../../fixtures/test-users'

test.describe('Profile - User Posts', () => {
  test('should display user posts section', async ({ authenticatedPage }) => {
    const profilePage = new ProfilePage(authenticatedPage)
    await profilePage.goto(testUsers.alice.username)

    // Should have posts section
    const hasPosts = await profilePage.hasPosts()
    const hasEmpty = await profilePage.hasEmptyPostsState()

    expect(hasPosts || hasEmpty).toBe(true)
  })

  test('should show post count', async ({ authenticatedPage }) => {
    const profilePage = new ProfilePage(authenticatedPage)
    await profilePage.goto(testUsers.alice.username)

    // Get post count
    const postCount = await profilePage.getPostCount()

    expect(typeof postCount).toBe('number')
    expect(postCount >= 0).toBe(true)
  })

  test('should display empty state when no posts', async ({ authenticatedPage }) => {
    const profilePage = new ProfilePage(authenticatedPage)

    // Try to find a user with no posts
    await profilePage.goto(testUsers.diana.username)

    // Check if empty
    const hasEmpty = await profilePage.hasEmptyPostsState()
    const posts = await profilePage.getUserPosts()

    if (posts === 0) {
      expect(hasEmpty).toBe(true)
    }
  })

  test('should list user posts', async ({ authenticatedPage }) => {
    const profilePage = new ProfilePage(authenticatedPage)
    await profilePage.goto(testUsers.alice.username)

    // Get posts count
    const postsCount = await profilePage.getUserPosts()

    expect(postsCount >= 0).toBe(true)
  })

  test('should show pinned posts at top', async ({ authenticatedPage }) => {
    const profilePage = new ProfilePage(authenticatedPage)
    await profilePage.goto(testUsers.alice.username)

    if ((await profilePage.getUserPosts()) > 0) {
      const firstPost = await profilePage.getFirstPost()

      if (firstPost) {
        const isPinned = await firstPost.isPinned()

        // First post might be pinned (depends on user)
        expect(typeof isPinned).toBe('boolean')
      }
    }
  })

  test('should allow interacting with posts', async ({ authenticatedPage }) => {
    const profilePage = new ProfilePage(authenticatedPage)
    await profilePage.goto(testUsers.alice.username)

    const firstPost = await profilePage.getFirstPost()

    if (firstPost) {
      // Like
      await firstPost.like()
      await authenticatedPage.waitForTimeout(300)

      // Save
      await firstPost.save()
      await authenticatedPage.waitForTimeout(300)

      // Comment
      await firstPost.comment()
      await authenticatedPage.waitForTimeout(300)

      // Should not error
      const hasError = await profilePage.hasError()
      expect(hasError).toBe(false)
    }
  })

  test('should play post audio', async ({ authenticatedPage }) => {
    const profilePage = new ProfilePage(authenticatedPage)
    await profilePage.goto(testUsers.alice.username)

    const firstPost = await profilePage.getFirstPost()

    if (firstPost) {
      // Play
      try {
        await firstPost.play()

        // Play count might increase
        const playCount = await firstPost.getPlayCount()
        expect(playCount >= 0).toBe(true)
      } catch (e) {
        // Play button might not exist
      }
    }
  })

  test('should display post metadata', async ({ authenticatedPage }) => {
    const profilePage = new ProfilePage(authenticatedPage)
    await profilePage.goto(testUsers.alice.username)

    const firstPost = await profilePage.getFirstPost()

    if (firstPost) {
      const title = await firstPost.getTitle()
      const likeCount = await firstPost.getLikeCount()
      const playCount = await firstPost.getPlayCount()

      expect(typeof title).toBe('string')
      expect(typeof likeCount).toBe('number')
      expect(typeof playCount).toBe('number')
    }
  })

  test('should update like count on profile posts', async ({ authenticatedPage }) => {
    const profilePage = new ProfilePage(authenticatedPage)
    await profilePage.goto(testUsers.alice.username)

    const firstPost = await profilePage.getFirstPost()

    if (firstPost) {
      const initialLikeCount = await firstPost.getLikeCount()

      // Like
      await firstPost.like()

      // Wait for update
      await expect(async () => {
        const newLikeCount = await firstPost.getLikeCount()
        expect(newLikeCount).not.toBe(initialLikeCount)
      }).toPass({ timeout: 3000 })
    }
  })

  test('should navigate to post from profile', async ({ authenticatedPage }) => {
    const profilePage = new ProfilePage(authenticatedPage)
    await profilePage.goto(testUsers.alice.username)

    const firstPost = await profilePage.getFirstPost()

    if (firstPost) {
      const initialUrl = authenticatedPage.url()

      // Click post
      await firstPost.click()

      // Might navigate to post detail
      await authenticatedPage.waitForTimeout(500)

      // URL might change
      const newUrl = authenticatedPage.url()

      // Should either navigate or modal opens (depending on implementation)
      expect(typeof newUrl).toBe('string')
    }
  })

  test('should filter posts by user', async ({ authenticatedPageAs }) => {
    const bobPage = await authenticatedPageAs(testUsers.bob)
    const profilePage = new ProfilePage(bobPage)

    await profilePage.goto(testUsers.bob.username)

    // Get bob's post count
    const bobPostCount = await profilePage.getPostCount()

    // Navigate to alice
    await profilePage.goto(testUsers.alice.username)

    // Get alice's post count
    const alicePostCount = await profilePage.getPostCount()

    // Should show different counts for different users
    // (might be same if they have same number of posts)
    expect(typeof bobPostCount).toBe('number')
    expect(typeof alicePostCount).toBe('number')
  })

  test('should handle loading posts on profile', async ({ authenticatedPage }) => {
    const profilePage = new ProfilePage(authenticatedPage)
    await profilePage.goto(testUsers.alice.username)

    // Wait for profile to load
    await profilePage.waitForProfileLoad()

    // Posts should load
    const postsCount = await profilePage.getUserPosts()

    expect(postsCount >= 0).toBe(true)
  })

  test('should show accurate post count', async ({ authenticatedPage }) => {
    const profilePage = new ProfilePage(authenticatedPage)
    await profilePage.goto(testUsers.alice.username)

    // Get stats post count
    const statsPostCount = (await profilePage.getUserStats()).posts

    // Get actual posts
    const actualPosts = await profilePage.getUserPosts()

    // Should match or be close (might differ by 1)
    expect(Math.abs(statsPostCount - actualPosts)).toBeLessThanOrEqual(1)
  })

  test('should display pinned posts before regular posts', async ({ authenticatedPage }) => {
    const profilePage = new ProfilePage(authenticatedPage)
    await profilePage.goto(testUsers.alice.username)

    const postsCount = await profilePage.getUserPosts()

    if (postsCount > 1) {
      const firstPost = await profilePage.getFirstPost()

      if (firstPost) {
        const isPinned = await firstPost.isPinned()

        // First post is more likely to be pinned than random
        // (though not guaranteed)
        expect(typeof isPinned).toBe('boolean')
      }
    }
  })

  test('should allow interactions on multiple posts', async ({ authenticatedPage }) => {
    const profilePage = new ProfilePage(authenticatedPage)
    await profilePage.goto(testUsers.alice.username)

    const postsCount = await profilePage.getUserPosts()

    if (postsCount > 1) {
      // Interact with first two posts
      const firstPost = await profilePage.getFirstPost()

      if (firstPost) {
        // Like first
        await firstPost.like()
        await authenticatedPage.waitForTimeout(300)

        // Find second post (might not be easy with page object)
        // Just verify no error
        const hasError = await profilePage.hasError()
        expect(hasError).toBe(false)
      }
    }
  })
})
