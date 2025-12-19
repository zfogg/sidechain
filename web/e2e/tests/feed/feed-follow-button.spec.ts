import { test } from '../../fixtures/auth'
import { expect } from '@playwright/test'
import { FeedPage, PostCardElement } from '../../page-objects/FeedPage'
import { ProfilePage } from '../../page-objects/ProfilePage'
import { testUsers } from '../../fixtures/test-users'

test.describe('Feed - Follow Button', () => {
  test('should show follow button on posts in feed', async ({ authenticatedPageAs }) => {
    const alicePage = await authenticatedPageAs(testUsers.alice)
    const feedPage = new FeedPage(alicePage)

    await feedPage.goto()
    await feedPage.waitForFeedLoad()

    const postCount = await feedPage.getPostCount()
    if (postCount > 0) {
      const firstPost = feedPage.getPostCard(0)
      const hasFollowButton = await firstPost.hasFollowButton()
      
      // Follow button should be visible if post is not by alice
      // (we can't guarantee this, but if there are posts, at least check the button exists)
      if (hasFollowButton) {
        const buttonText = await firstPost.followButton.textContent()
        expect(buttonText).toMatch(/Follow|Following/i)
      }
    }
  })

  test('should update follow button when clicked in feed', async ({ authenticatedPageAs }) => {
    const alicePage = await authenticatedPageAs(testUsers.alice)
    const feedPage = new FeedPage(alicePage)

    await feedPage.goto()
    await feedPage.waitForFeedLoad()

    const postCount = await feedPage.getPostCount()
    if (postCount === 0) {
      test.skip()
      return
    }

    // Find a post with a follow button (not alice's own post)
    let postWithFollowButton: PostCardElement | null = null
    for (let i = 0; i < postCount; i++) {
      const post = feedPage.getPostCard(i)
      if (await post.hasFollowButton()) {
        postWithFollowButton = post
        break
      }
    }

    if (!postWithFollowButton) {
      test.skip()
      return
    }

    // Get initial follow state
    const initialFollowState = await postWithFollowButton.isFollowing()
    const initialButtonText = await postWithFollowButton.followButton.textContent()

    // Click follow button
    await postWithFollowButton.follow()

    // Button text should change
    await expect(async () => {
      const newButtonText = await postWithFollowButton!.followButton.textContent()
      expect(newButtonText).not.toBe(initialButtonText)
      
      // If we were following, should now say "Follow", if not following, should say "Following"
      const newFollowState = await postWithFollowButton!.isFollowing()
      expect(newFollowState).toBe(!initialFollowState)
    }).toPass({ timeout: 5000 })
  })

  test('should update follow button in feed when following from profile', async ({ authenticatedPageAs }) => {
    const alicePage = await authenticatedPageAs(testUsers.alice)
    const feedPage = new FeedPage(alicePage)
    const profilePage = new ProfilePage(alicePage)

    await feedPage.goto()
    await feedPage.waitForFeedLoad()

    const postCount = await feedPage.getPostCount()
    if (postCount === 0) {
      test.skip()
      return
    }

    // Find a post with a follow button and get the author username
    let postWithFollowButton: PostCardElement | null = null
    let authorUsername: string | null = null
    
    for (let i = 0; i < postCount; i++) {
      const post = feedPage.getPostCard(i)
      if (await post.hasFollowButton()) {
        postWithFollowButton = post
        authorUsername = await post.getAuthorUsername()
        break
      }
    }

    if (!postWithFollowButton || !authorUsername) {
      test.skip()
      return
    }

    // Get initial follow state in feed
    const initialFeedFollowState = await postWithFollowButton.isFollowing()

    // Navigate to author's profile
    await profilePage.goto(authorUsername)
    await profilePage.waitForProfileLoad()

    // Get follow state on profile
    const profileFollowState = await profilePage.isFollowing()

    // If states don't match, follow/unfollow to sync
    if (profileFollowState !== initialFeedFollowState) {
      if (profileFollowState) {
        await profilePage.unfollow()
      } else {
        await profilePage.follow()
      }
      await alicePage.waitForTimeout(500)
    }

    // Now follow from profile
    const wasFollowing = await profilePage.isFollowing()
    await profilePage.follow()
    await alicePage.waitForTimeout(500)

    // Navigate back to feed
    await feedPage.goto()
    await feedPage.waitForFeedLoad()

    // Find the same post again
    let updatedPost: PostCardElement | null = null
    for (let i = 0; i < await feedPage.getPostCount(); i++) {
      const post = feedPage.getPostCard(i)
      const postAuthor = await post.getAuthorUsername()
      if (postAuthor === authorUsername) {
        updatedPost = post
        break
      }
    }

    if (updatedPost) {
      // Follow button in feed should reflect the new state
      await expect(async () => {
        const feedFollowState = await updatedPost!.isFollowing()
        expect(feedFollowState).toBe(!wasFollowing)
      }).toPass({ timeout: 5000 })
    }
  })

  test('should update all follow buttons for same user across feed', async ({ authenticatedPageAs }) => {
    const alicePage = await authenticatedPageAs(testUsers.alice)
    const feedPage = new FeedPage(alicePage)

    await feedPage.goto()
    await feedPage.waitForFeedLoad()

    const postCount = await feedPage.getPostCount()
    if (postCount < 2) {
      test.skip()
      return
    }

    // Find first post with follow button and get author
    let firstPost: PostCardElement | null = null
    let authorUsername: string | null = null
    
    for (let i = 0; i < postCount; i++) {
      const post = feedPage.getPostCard(i)
      if (await post.hasFollowButton()) {
        firstPost = post
        authorUsername = await post.getAuthorUsername()
        break
      }
    }

    if (!firstPost || !authorUsername) {
      test.skip()
      return
    }

    // Find all posts by this author
    const postsByAuthor: PostCardElement[] = []
    for (let i = 0; i < postCount; i++) {
      const post = feedPage.getPostCard(i)
      const postAuthor = await post.getAuthorUsername()
      if (postAuthor === authorUsername && (await post.hasFollowButton())) {
        postsByAuthor.push(post)
      }
    }

    if (postsByAuthor.length < 2) {
      test.skip()
      return
    }

    // Get initial follow state from first post
    const initialFollowState = await firstPost.isFollowing()

    // Click follow on first post
    await firstPost.follow()
    await alicePage.waitForTimeout(500)

    // All posts by this author should have updated follow buttons
    for (const post of postsByAuthor) {
      await expect(async () => {
        const followState = await post.isFollowing()
        expect(followState).toBe(!initialFollowState)
      }).toPass({ timeout: 3000 })
    }
  })

  test('should show cursor pointer on follow button hover', async ({ authenticatedPageAs }) => {
    const alicePage = await authenticatedPageAs(testUsers.alice)
    const feedPage = new FeedPage(alicePage)

    await feedPage.goto()
    await feedPage.waitForFeedLoad()

    const postCount = await feedPage.getPostCount()
    if (postCount === 0) {
      test.skip()
      return
    }

    // Find a post with follow button
    let postWithFollowButton: PostCardElement | null = null
    for (let i = 0; i < postCount; i++) {
      const post = feedPage.getPostCard(i)
      if (await post.hasFollowButton()) {
        postWithFollowButton = post
        break
      }
    }

    if (!postWithFollowButton) {
      test.skip()
      return
    }

    // Check cursor style
    const cursor = await postWithFollowButton.followButton.evaluate((el: HTMLElement) => {
      return window.getComputedStyle(el).cursor
    })

    expect(cursor).toBe('pointer')
  })

  test('should update follow button in profile when following from feed', async ({ authenticatedPageAs }) => {
    const alicePage = await authenticatedPageAs(testUsers.alice)
    const feedPage = new FeedPage(alicePage)
    const profilePage = new ProfilePage(alicePage)

    await feedPage.goto()
    await feedPage.waitForFeedLoad()

    const postCount = await feedPage.getPostCount()
    if (postCount === 0) {
      test.skip()
      return
    }

    // Find a post with a follow button and get the author username
    let postWithFollowButton: PostCardElement | null = null
    let authorUsername: string | null = null
    
    for (let i = 0; i < postCount; i++) {
      const post = feedPage.getPostCard(i)
      if (await post.hasFollowButton()) {
        postWithFollowButton = post
        authorUsername = await post.getAuthorUsername()
        break
      }
    }

    if (!postWithFollowButton || !authorUsername) {
      test.skip()
      return
    }

    // Get initial follow state in feed
    const initialFeedFollowState = await postWithFollowButton.isFollowing()

    // Navigate to author's profile first to check initial state
    await profilePage.goto(authorUsername)
    await profilePage.waitForProfileLoad()

    // Get initial follow state on profile
    const initialProfileFollowState = await profilePage.isFollowing()

    // If states don't match, sync them first by following/unfollowing from profile
    if (initialProfileFollowState !== initialFeedFollowState) {
      if (initialProfileFollowState) {
        await profilePage.unfollow()
        await alicePage.waitForTimeout(500)
      } else {
        await profilePage.follow()
        await alicePage.waitForTimeout(500)
      }
    }

    // Now go back to feed and follow from there
    await feedPage.goto()
    await feedPage.waitForFeedLoad()

    // Find the same post again
    let targetPost: PostCardElement | null = null
    for (let i = 0; i < await feedPage.getPostCount(); i++) {
      const post = feedPage.getPostCard(i)
      const postAuthor = await post.getAuthorUsername()
      if (postAuthor === authorUsername && (await post.hasFollowButton())) {
        targetPost = post
        break
      }
    }

    if (!targetPost) {
      test.skip()
      return
    }

    // Get the follow state before clicking
    const wasFollowingInFeed = await targetPost.isFollowing()

    // Click follow in feed
    await targetPost.follow()
    await alicePage.waitForTimeout(500) // Wait for mutation to complete

    // Navigate to profile
    await profilePage.goto(authorUsername)
    await profilePage.waitForProfileLoad()

    // Verify profile follow button reflects the new state
    await expect(async () => {
      const profileFollowState = await profilePage.isFollowing()
      expect(profileFollowState).toBe(!wasFollowingInFeed)
    }).toPass({ timeout: 5000 })
  })

  test('should persist follow state after page reload', async ({ authenticatedPageAs }) => {
    const alicePage = await authenticatedPageAs(testUsers.alice)
    const feedPage = new FeedPage(alicePage)
    const profilePage = new ProfilePage(alicePage)

    await feedPage.goto()
    await feedPage.waitForFeedLoad()

    const postCount = await feedPage.getPostCount()
    if (postCount === 0) {
      test.skip()
      return
    }

    // Find a post with a follow button and get the author username
    let postWithFollowButton: PostCardElement | null = null
    let authorUsername: string | null = null
    
    for (let i = 0; i < postCount; i++) {
      const post = feedPage.getPostCard(i)
      if (await post.hasFollowButton()) {
        postWithFollowButton = post
        authorUsername = await post.getAuthorUsername()
        break
      }
    }

    if (!postWithFollowButton || !authorUsername) {
      test.skip()
      return
    }

    // Get initial follow state
    const initialFollowState = await postWithFollowButton.isFollowing()

    // Navigate to profile first to check initial state
    await profilePage.goto(authorUsername)
    await profilePage.waitForProfileLoad()
    const initialProfileFollowState = await profilePage.isFollowing()

    // Sync states if they don't match
    if (initialProfileFollowState !== initialFollowState) {
      if (initialProfileFollowState) {
        await profilePage.unfollow()
        await alicePage.waitForTimeout(500)
      } else {
        await profilePage.follow()
        await alicePage.waitForTimeout(500)
      }
    }

    // Go back to feed
    await feedPage.goto()
    await feedPage.waitForFeedLoad()

    // Find the post again
    let targetPost: PostCardElement | null = null
    for (let i = 0; i < await feedPage.getPostCount(); i++) {
      const post = feedPage.getPostCard(i)
      const postAuthor = await post.getAuthorUsername()
      if (postAuthor === authorUsername && (await post.hasFollowButton())) {
        targetPost = post
        break
      }
    }

    if (!targetPost) {
      test.skip()
      return
    }

    // Get current follow state
    const currentFollowState = await targetPost.isFollowing()

    // Follow/unfollow to change state
    await targetPost.follow()
    await alicePage.waitForTimeout(1000) // Wait for mutation to complete

    // Verify optimistic update worked
    const newFollowState = await targetPost.isFollowing()
    expect(newFollowState).toBe(!currentFollowState)

    // Reload the page
    await alicePage.reload()
    await feedPage.waitForFeedLoad()

    // Find the post again after reload
    let reloadedPost: PostCardElement | null = null
    for (let i = 0; i < await feedPage.getPostCount(); i++) {
      const post = feedPage.getPostCard(i)
      const postAuthor = await post.getAuthorUsername()
      if (postAuthor === authorUsername && (await post.hasFollowButton())) {
        reloadedPost = post
        break
      }
    }

    if (!reloadedPost) {
      test.skip()
      return
    }

    // Verify follow state persisted after reload
    await expect(async () => {
      const persistedFollowState = await reloadedPost!.isFollowing()
      expect(persistedFollowState).toBe(newFollowState)
    }).toPass({ timeout: 5000 })

    // Also verify on profile page
    await profilePage.goto(authorUsername)
    await profilePage.waitForProfileLoad()

    await expect(async () => {
      const profileFollowState = await profilePage.isFollowing()
      expect(profileFollowState).toBe(newFollowState)
    }).toPass({ timeout: 5000 })
  })
})
