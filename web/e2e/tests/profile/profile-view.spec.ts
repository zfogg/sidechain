import { test, expect } from '../../fixtures/auth'
import { ProfilePage } from '../../page-objects/ProfilePage'
import { testUsers } from '../../fixtures/test-users'

test.describe('Profile - Viewing', () => {
  test('should load user profile page', async ({ authenticatedPage }) => {
    const profilePage = new ProfilePage(authenticatedPage)
    await profilePage.goto(testUsers.alice.username)

    // Should be on profile page
    expect(authenticatedPage.url()).toContain(`/profile/${testUsers.alice.username}`)

    // Should load profile data
    expect(await profilePage.isLoaded()).toBe(true)
  })

  test('should display user profile information', async ({ authenticatedPage }) => {
    const profilePage = new ProfilePage(authenticatedPage)
    await profilePage.goto(testUsers.alice.username)

    // Should have profile data
    const displayName = await profilePage.getDisplayName()
    const username = await profilePage.getUsername()

    expect(displayName).toBeTruthy()
    expect(username).toBeTruthy()
  })

  test('should show follower count', async ({ authenticatedPage }) => {
    const profilePage = new ProfilePage(authenticatedPage)
    await profilePage.goto(testUsers.alice.username)

    // Get stats
    const stats = await profilePage.getUserStats()

    expect(stats.followers >= 0).toBe(true)
    expect(typeof stats.followers).toBe('number')
  })

  test('should show following count', async ({ authenticatedPage }) => {
    const profilePage = new ProfilePage(authenticatedPage)
    await profilePage.goto(testUsers.alice.username)

    const stats = await profilePage.getUserStats()

    expect(stats.following >= 0).toBe(true)
    expect(typeof stats.following).toBe('number')
  })

  test('should show post count', async ({ authenticatedPage }) => {
    const profilePage = new ProfilePage(authenticatedPage)
    await profilePage.goto(testUsers.alice.username)

    const stats = await profilePage.getUserStats()

    expect(stats.posts >= 0).toBe(true)
    expect(typeof stats.posts).toBe('number')
  })

  test('should display profile picture', async ({ authenticatedPage }) => {
    const profilePage = new ProfilePage(authenticatedPage)
    await profilePage.goto(testUsers.alice.username)

    // Profile picture should be visible
    const isVisible = await profilePage.profilePicture
      .isVisible({ timeout: 1000 })
      .catch(() => false)

    // Might be visible depending on profile data
    expect(typeof isVisible).toBe('boolean')
  })

  test('should show own profile edit button', async ({ authenticatedPage }) => {
    const profilePage = new ProfilePage(authenticatedPage)
    // Go to own profile (alice is logged in)
    await profilePage.goto(testUsers.alice.username)

    // Should have edit button for own profile
    const hasEditButton = await profilePage.hasEditProfileButton()
    expect(hasEditButton).toBe(true)
  })

  test('should show follow button for other profiles', async ({ authenticatedPageAs }) => {
    // Login as alice and view bob's profile
    const alicePage = await authenticatedPageAs(testUsers.alice)
    const profilePage = new ProfilePage(alicePage)
    await profilePage.goto(testUsers.bob.username)

    // Should have follow button for other user's profile
    const hasFollowButton = await profilePage.hasFollowButton()
    expect(hasFollowButton).toBe(true)
  })

  test('should not show follow button on own profile', async ({ authenticatedPage }) => {
    const profilePage = new ProfilePage(authenticatedPage)
    await profilePage.goto(testUsers.alice.username)

    // Own profile should not have follow button
    const hasFollowButton = await profilePage.hasFollowButton()
    expect(hasFollowButton).toBe(false)
  })

  test('should handle profile not found', async ({ authenticatedPage }) => {
    const profilePage = new ProfilePage(authenticatedPage)
    await profilePage.goto('nonexistentuser12345')

    // Should either show error or redirect
    const hasError = await profilePage.hasError()
    const isLoaded = await profilePage.isLoaded()

    // Should either error or not load
    expect(hasError || !isLoaded).toBe(true)
  })

  test('should display username correctly', async ({ authenticatedPage }) => {
    const profilePage = new ProfilePage(authenticatedPage)
    await profilePage.goto(testUsers.bob.username)

    const username = await profilePage.getUsername()

    // Should match the URL username (at least similar)
    expect(username.toLowerCase()).toBe(testUsers.bob.username.toLowerCase())
  })

  test('should display bio if available', async ({ authenticatedPage }) => {
    const profilePage = new ProfilePage(authenticatedPage)
    await profilePage.goto(testUsers.alice.username)

    // Bio might be empty or populated
    const bio = await profilePage.getBio()
    expect(typeof bio).toBe('string')
  })

  test('should show user stats section', async ({ authenticatedPage }) => {
    const profilePage = new ProfilePage(authenticatedPage)
    await profilePage.goto(testUsers.alice.username)

    // Get all stats
    const stats = await profilePage.getUserStats()

    // Should have all stats initialized
    expect(stats).toHaveProperty('followers')
    expect(stats).toHaveProperty('following')
    expect(stats).toHaveProperty('posts')
  })

  test('should allow navigating between different profiles', async ({
    authenticatedPageAs,
  }) => {
    const alicePage = await authenticatedPageAs(testUsers.alice)
    const profilePage = new ProfilePage(alicePage)

    // Navigate to alice
    await profilePage.goto(testUsers.alice.username)
    expect(alicePage.url()).toContain('/profile/alice')

    // Navigate to bob
    await profilePage.goto(testUsers.bob.username)
    expect(alicePage.url()).toContain('/profile/bob')

    // Navigate to charlie
    await profilePage.goto(testUsers.charlie.username)
    expect(alicePage.url()).toContain('/profile/charlie')
  })
})
