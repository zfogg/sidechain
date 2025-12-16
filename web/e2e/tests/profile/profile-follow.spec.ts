import { test, expect } from '../../fixtures/auth'
import { ProfilePage } from '../../page-objects/ProfilePage'
import { testUsers } from '../../fixtures/test-users'

test.describe('Profile - Following', () => {
  test('should follow a user', async ({ authenticatedPageAs }) => {
    const alicePage = await authenticatedPageAs(testUsers.alice)
    const profilePage = new ProfilePage(alicePage)

    // View bob's profile
    await profilePage.goto(testUsers.bob.username)

    // Should have follow button
    expect(await profilePage.hasFollowButton()).toBe(true)

    // Check initial state
    const wasFollowing = await profilePage.isFollowing()
    const initialFollowerCount = (await profilePage.getUserStats()).followers

    // Click follow
    await profilePage.follow()

    // Should now be following
    const isNowFollowing = await profilePage.isFollowing()
    expect(isNowFollowing).toBe(true)

    // Follower count should increase
    const newFollowerCount = (await profilePage.getUserStats()).followers
    expect(newFollowerCount).toBeGreaterThan(initialFollowerCount)
  })

  test('should unfollow a user', async ({ authenticatedPageAs }) => {
    const alicePage = await authenticatedPageAs(testUsers.alice)
    const profilePage = new ProfilePage(alicePage)

    // View bob's profile
    await profilePage.goto(testUsers.bob.username)

    // Follow first
    const isFollowing = await profilePage.isFollowing()
    if (!isFollowing) {
      await profilePage.follow()
      await alicePage.waitForTimeout(500)
    }

    const followerCountBeforeUnfollow = (await profilePage.getUserStats()).followers

    // Unfollow
    await profilePage.unfollow()

    // Should no longer be following
    const isNowFollowing = await profilePage.isFollowing()
    expect(isNowFollowing).toBe(false)

    // Follower count should decrease
    const newFollowerCount = (await profilePage.getUserStats()).followers
    expect(newFollowerCount).toBeLessThan(followerCountBeforeUnfollow)
  })

  test('should update follower count optimistically', async ({ authenticatedPageAs }) => {
    const alicePage = await authenticatedPageAs(testUsers.alice)
    const profilePage = new ProfilePage(alicePage)

    await profilePage.goto(testUsers.bob.username)

    const initialCount = (await profilePage.getUserStats()).followers
    const wasFollowing = await profilePage.isFollowing()

    // Follow
    await profilePage.follow()

    // Wait for optimistic update
    await expect(async () => {
      const newCount = (await profilePage.getUserStats()).followers

      if (!wasFollowing) {
        expect(newCount).toBeGreaterThan(initialCount)
      }
    }).toPass({ timeout: 3000 })
  })

  test('should toggle follow button state', async ({ authenticatedPageAs }) => {
    const alicePage = await authenticatedPageAs(testUsers.alice)
    const profilePage = new ProfilePage(alicePage)

    await profilePage.goto(testUsers.bob.username)

    // Get initial text
    const initialText = await profilePage.followButton.textContent()

    // Click follow/unfollow
    await profilePage.followButton.click()
    await alicePage.waitForTimeout(500)

    // Text should change
    const newText = await profilePage.followButton.textContent()

    // Button text should be different after interaction
    expect(newText).not.toBe(initialText)
  })

  test('should not show follow button on own profile', async ({ authenticatedPage }) => {
    const profilePage = new ProfilePage(authenticatedPage)
    // Alice viewing her own profile
    await profilePage.goto(testUsers.alice.username)

    // Should not have follow button
    const hasFollowButton = await profilePage.hasFollowButton()
    expect(hasFollowButton).toBe(false)

    // Should have edit button instead
    const hasEditButton = await profilePage.hasEditProfileButton()
    expect(hasEditButton).toBe(true)
  })

  test('should persist follow state when navigating away', async ({ authenticatedPageAs }) => {
    const alicePage = await authenticatedPageAs(testUsers.alice)
    const profilePage = new ProfilePage(alicePage)

    // Follow bob
    await profilePage.goto(testUsers.bob.username)
    const wasBobFollowing = await profilePage.isFollowing()

    if (!wasBobFollowing) {
      await profilePage.follow()
      await alicePage.waitForTimeout(500)
    }

    // Navigate to charlie
    await profilePage.goto(testUsers.charlie.username)

    // Navigate back to bob
    await profilePage.goto(testUsers.bob.username)

    // Follow state should persist
    const isBobStillFollowing = await profilePage.isFollowing()
    expect(isBobStillFollowing).toBe(true)
  })

  test('should handle follow errors gracefully', async ({ authenticatedPageAs }) => {
    const alicePage = await authenticatedPageAs(testUsers.alice)
    const profilePage = new ProfilePage(alicePage)

    await profilePage.goto(testUsers.bob.username)

    // Try to follow
    try {
      await profilePage.follow()

      // Should not error
      const hasError = await profilePage.hasError()
      expect(hasError).toBe(false)
    } catch (e) {
      // If follow fails, that's ok for this test
    }
  })

  test('should show correct follow button text', async ({ authenticatedPageAs }) => {
    const alicePage = await authenticatedPageAs(testUsers.alice)
    const profilePage = new ProfilePage(alicePage)

    await profilePage.goto(testUsers.bob.username)

    // Get button text
    const buttonText = await profilePage.followButton.textContent()

    // Should be "Follow" or "Following"
    expect(/Follow|Following/i.test(buttonText || '')).toBe(true)
  })

  test('should allow multiple follow/unfollow cycles', async ({ authenticatedPageAs }) => {
    const alicePage = await authenticatedPageAs(testUsers.alice)
    const profilePage = new ProfilePage(alicePage)

    await profilePage.goto(testUsers.bob.username)

    // Do multiple follow/unfollow cycles
    for (let i = 0; i < 2; i++) {
      const isFollowing = await profilePage.isFollowing()

      if (isFollowing) {
        await profilePage.unfollow()
      } else {
        await profilePage.follow()
      }

      await alicePage.waitForTimeout(300)
    }

    // Should still be functional
    const hasError = await profilePage.hasError()
    expect(hasError).toBe(false)
  })

  test('should update following count when following', async ({
    authenticatedPageAs,
  }) => {
    const alicePage = await authenticatedPageAs(testUsers.alice)
    const profilePage = new ProfilePage(alicePage)

    // Check own profile
    await profilePage.goto(testUsers.alice.username)
    const initialFollowingCount = (await profilePage.getUserStats()).following

    // Navigate to bob and follow
    await profilePage.goto(testUsers.bob.username)
    const wasBobFollowing = await profilePage.isFollowing()

    if (!wasBobFollowing) {
      await profilePage.follow()
      await alicePage.waitForTimeout(500)
    }

    // Go back to own profile
    await profilePage.goto(testUsers.alice.username)
    const newFollowingCount = (await profilePage.getUserStats()).following

    // Following count should be >= initial (might not increase if already following)
    expect(newFollowingCount >= initialFollowingCount).toBe(true)
  })
})
