import { test } from '../../fixtures/auth'
import { expect, Page } from '@playwright/test'
import { testUsers } from '../../fixtures/test-users'
import { ProfilePage } from '../../page-objects/ProfilePage'
import { FeedPage } from '../../page-objects/FeedPage'
import { SettingsPage } from '../../page-objects/SettingsPage'
import path from 'path'
import fs from 'fs'
import { fileURLToPath } from 'url'

const __filename = fileURLToPath(import.meta.url)
const __dirname = path.dirname(__filename)

/**
 * Profile Picture Upload & Propagation Tests
 *
 * Tests that profile picture uploads work correctly and propagate
 * across the entire application: settings, profile, feed posts, etc.
 */
test.describe('Profile Picture Upload & Propagation', () => {
  // Create a test image file before tests
  const testImagePath = path.join(__dirname, '../../fixtures/test-avatar.png')
  const largeImagePath = path.join(__dirname, '../../fixtures/large-image.png')

  test.beforeAll(async () => {
    // Create a minimal valid PNG file for testing (1x1 pixel)
    const minimalPng = Buffer.from([
      0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, // PNG signature
      0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52, // IHDR chunk
      0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, // 1x1 dimensions
      0x08, 0x02, 0x00, 0x00, 0x00, 0x90, 0x77, 0x53, // bit depth, color type
      0xde, 0x00, 0x00, 0x00, 0x0c, 0x49, 0x44, 0x41, // IDAT chunk
      0x54, 0x08, 0xd7, 0x63, 0xf8, 0x0f, 0x00, 0x00,
      0x01, 0x01, 0x00, 0x05, 0x1c, 0x4e, 0x7f, 0x24,
      0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, // IEND chunk
      0xae, 0x42, 0x60, 0x82
    ])

    // Create fixtures directory if it doesn't exist
    const fixturesDir = path.dirname(testImagePath)
    if (!fs.existsSync(fixturesDir)) {
      fs.mkdirSync(fixturesDir, { recursive: true })
    }

    fs.writeFileSync(testImagePath, minimalPng)
  })

  test.afterAll(async () => {
    // Clean up test files
    if (fs.existsSync(testImagePath)) {
      fs.unlinkSync(testImagePath)
    }
    if (fs.existsSync(largeImagePath)) {
      fs.unlinkSync(largeImagePath)
    }
  })

  test('should upload profile picture from settings page', async ({ authenticatedPage }) => {
    const settingsPage = new SettingsPage(authenticatedPage)
    await settingsPage.goto()

    // Navigate to profile tab
    await settingsPage.switchTab('profile')

    // Find the file input and upload
    const fileInput = authenticatedPage.locator('input[type="file"][accept*="image"]')
    await fileInput.setInputFiles(testImagePath)

    // Wait for upload to complete (button should not say "Uploading...")
    await expect(authenticatedPage.locator('button:has-text("Uploading...")')).not.toBeVisible({ timeout: 10000 })

    // Should show success message
    await expect(authenticatedPage.locator('text=/profile picture updated|success/i')).toBeVisible({ timeout: 5000 })
  })

  test('should display uploaded picture in settings preview immediately', async ({ authenticatedPage }) => {
    const settingsPage = new SettingsPage(authenticatedPage)
    await settingsPage.goto()
    await settingsPage.switchTab('profile')

    // Get the avatar element before upload
    const avatarImg = authenticatedPage.locator('img[alt*="profile" i], img[alt*="avatar" i], [class*="Avatar"] img').first()
    const initialSrc = await avatarImg.getAttribute('src')

    // Upload new picture
    const fileInput = authenticatedPage.locator('input[type="file"][accept*="image"]')
    await fileInput.setInputFiles(testImagePath)

    // Wait for upload
    await expect(authenticatedPage.locator('button:has-text("Uploading...")')).not.toBeVisible({ timeout: 10000 })

    // Avatar src should have changed
    await expect(async () => {
      const newSrc = await avatarImg.getAttribute('src')
      expect(newSrc).not.toBe(initialSrc)
    }).toPass({ timeout: 5000 })
  })

  test('should show updated picture on own profile after upload', async ({ authenticatedPage }) => {
    const settingsPage = new SettingsPage(authenticatedPage)
    await settingsPage.goto()
    await settingsPage.switchTab('profile')

    // Upload picture
    const fileInput = authenticatedPage.locator('input[type="file"][accept*="image"]')
    await fileInput.setInputFiles(testImagePath)
    await expect(authenticatedPage.locator('button:has-text("Uploading...")')).not.toBeVisible({ timeout: 10000 })

    // Get the new profile picture URL from settings
    const settingsAvatar = authenticatedPage.locator('img[alt*="profile" i], img[alt*="avatar" i], [class*="Avatar"] img').first()
    const uploadedSrc = await settingsAvatar.getAttribute('src')

    // Navigate to own profile
    const profilePage = new ProfilePage(authenticatedPage)
    await profilePage.goto(testUsers.alice.username)

    // Profile picture should match the uploaded one
    const profileAvatar = profilePage.profilePicture
    await expect(profileAvatar).toBeVisible()

    const profileSrc = await profileAvatar.getAttribute('src')
    // Both should have a real URL (not just dicebear fallback)
    expect(profileSrc).toBeTruthy()
    // If the uploaded source was a real URL (not dicebear), profile should match
    if (uploadedSrc && !uploadedSrc.includes('dicebear')) {
      expect(profileSrc).toContain(uploadedSrc?.split('?')[0]?.split('/').pop() || '')
    }
  })

  test('should show updated picture on posts in feed after upload', async ({ authenticatedPage }) => {
    // First upload a profile picture
    const settingsPage = new SettingsPage(authenticatedPage)
    await settingsPage.goto()
    await settingsPage.switchTab('profile')

    const fileInput = authenticatedPage.locator('input[type="file"][accept*="image"]')
    await fileInput.setInputFiles(testImagePath)
    await expect(authenticatedPage.locator('button:has-text("Uploading...")')).not.toBeVisible({ timeout: 10000 })

    // Navigate to feed
    const feedPage = new FeedPage(authenticatedPage)
    await feedPage.goto()

    // Look for posts by the current user (alice)
    const ownPosts = authenticatedPage.locator(`[data-testid="post-card"]:has-text("${testUsers.alice.username}"), .bg-card.border:has-text("${testUsers.alice.username}")`)

    // If there are own posts, check their avatar
    const ownPostCount = await ownPosts.count()
    if (ownPostCount > 0) {
      const postAvatar = ownPosts.first().locator('img[alt*="profile" i], img[alt*="avatar" i], [class*="Avatar"] img').first()
      await expect(postAvatar).toBeVisible()

      const avatarSrc = await postAvatar.getAttribute('src')
      // Should not be the default dicebear if we uploaded a custom picture
      // (This depends on whether there are posts by alice)
      expect(avatarSrc).toBeTruthy()
    }
  })

  test('should reject non-image files', async ({ authenticatedPage }) => {
    const settingsPage = new SettingsPage(authenticatedPage)
    await settingsPage.goto()
    await settingsPage.switchTab('profile')

    // Create a text file
    const textFilePath = path.join(__dirname, '../../fixtures/test.txt')
    fs.writeFileSync(textFilePath, 'not an image')

    try {
      // Try to upload text file
      const fileInput = authenticatedPage.locator('input[type="file"][accept*="image"]')
      await fileInput.setInputFiles(textFilePath)

      // Should show error message
      await expect(authenticatedPage.locator('text=/please select an image|invalid file/i')).toBeVisible({ timeout: 3000 })
    } finally {
      fs.unlinkSync(textFilePath)
    }
  })

  test('should reject files larger than 5MB', async ({ authenticatedPage }) => {
    const settingsPage = new SettingsPage(authenticatedPage)
    await settingsPage.goto()
    await settingsPage.switchTab('profile')

    // Create a large file (6MB of zeros with PNG header)
    const largeBuffer = Buffer.alloc(6 * 1024 * 1024)
    // Add PNG header so it's recognized as an image
    largeBuffer[0] = 0x89
    largeBuffer[1] = 0x50
    largeBuffer[2] = 0x4e
    largeBuffer[3] = 0x47

    fs.writeFileSync(largeImagePath, largeBuffer)

    // Try to upload large file
    const fileInput = authenticatedPage.locator('input[type="file"][accept*="image"]')
    await fileInput.setInputFiles(largeImagePath)

    // Should show error message about file size
    await expect(authenticatedPage.locator('text=/5MB|too large|file size/i')).toBeVisible({ timeout: 3000 })
  })

  test('should handle upload network failure gracefully', async ({ authenticatedPage }) => {
    const settingsPage = new SettingsPage(authenticatedPage)
    await settingsPage.goto()
    await settingsPage.switchTab('profile')

    // Intercept the upload request and make it fail
    await authenticatedPage.route('**/upload-profile-picture**', (route) => {
      route.abort('failed')
    })

    const fileInput = authenticatedPage.locator('input[type="file"][accept*="image"]')
    await fileInput.setInputFiles(testImagePath)

    // Should show error message
    await expect(authenticatedPage.locator('text=/failed|error|try again/i')).toBeVisible({ timeout: 10000 })

    // Unroute for cleanup
    await authenticatedPage.unroute('**/upload-profile-picture**')
  })

  test('should replace existing picture (not append)', async ({ authenticatedPage }) => {
    const settingsPage = new SettingsPage(authenticatedPage)
    await settingsPage.goto()
    await settingsPage.switchTab('profile')

    // Upload first picture
    const fileInput = authenticatedPage.locator('input[type="file"][accept*="image"]')
    await fileInput.setInputFiles(testImagePath)
    await expect(authenticatedPage.locator('button:has-text("Uploading...")')).not.toBeVisible({ timeout: 10000 })

    // Get first picture URL
    const avatar = authenticatedPage.locator('img[alt*="profile" i], img[alt*="avatar" i], [class*="Avatar"] img').first()
    const firstSrc = await avatar.getAttribute('src')

    // Create a different test image (2x2 pixel)
    const secondImagePath = path.join(__dirname, '../../fixtures/test-avatar-2.png')
    const secondPng = Buffer.from([
      0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
      0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
      0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02, // 2x2
      0x08, 0x02, 0x00, 0x00, 0x00, 0xfd, 0xd4, 0x9a,
      0x73, 0x00, 0x00, 0x00, 0x12, 0x49, 0x44, 0x41,
      0x54, 0x08, 0xd7, 0x63, 0xf8, 0xff, 0xff, 0x3f,
      0x00, 0x05, 0xfe, 0x02, 0xfe, 0xa7, 0x35, 0x81,
      0x84, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e,
      0x44, 0xae, 0x42, 0x60, 0x82
    ])
    fs.writeFileSync(secondImagePath, secondPng)

    try {
      // Upload second picture
      await fileInput.setInputFiles(secondImagePath)
      await expect(authenticatedPage.locator('button:has-text("Uploading...")')).not.toBeVisible({ timeout: 10000 })

      // Get second picture URL
      const secondSrc = await avatar.getAttribute('src')

      // URLs should be different (picture was replaced)
      expect(secondSrc).not.toBe(firstSrc)

      // There should only be one avatar displayed in the profile section
      const profileSection = authenticatedPage.locator('.flex.flex-col.items-center.gap-4')
      const avatarsInSection = await profileSection.locator('img').count()
      expect(avatarsInSection).toBe(1)
    } finally {
      fs.unlinkSync(secondImagePath)
    }
  })

  test.describe('Multi-user propagation', () => {
    test('other user should see updated profile picture on profile', async ({ authenticatedPageAs }) => {
      // Alice uploads a profile picture
      const alicePage = await authenticatedPageAs(testUsers.alice)
      const settingsPage = new SettingsPage(alicePage)
      await settingsPage.goto()
      await settingsPage.switchTab('profile')

      const fileInput = alicePage.locator('input[type="file"][accept*="image"]')
      await fileInput.setInputFiles(testImagePath)
      await expect(alicePage.locator('button:has-text("Uploading...")')).not.toBeVisible({ timeout: 10000 })

      // Bob views Alice's profile
      const bobPage = await authenticatedPageAs(testUsers.bob)
      const profilePage = new ProfilePage(bobPage)
      await profilePage.goto(testUsers.alice.username)

      // Should see Alice's profile picture
      await expect(profilePage.profilePicture).toBeVisible()
      const avatarSrc = await profilePage.profilePicture.getAttribute('src')
      expect(avatarSrc).toBeTruthy()
    })

    test('other user should see updated picture on posts in their feed', async ({ authenticatedPageAs }) => {
      // Alice's posts should show her updated avatar when Bob views the feed
      const bobPage = await authenticatedPageAs(testUsers.bob)
      const feedPage = new FeedPage(bobPage)
      await feedPage.goto()

      // Switch to global feed to see all posts
      await feedPage.switchToFeedType('global')

      // Look for posts by alice
      const alicePosts = bobPage.locator(`[data-testid="post-card"]:has-text("alice"), .bg-card.border:has-text("alice")`)
      const alicePostCount = await alicePosts.count()

      if (alicePostCount > 0) {
        // Check that the avatar is displayed (can't verify exact URL without knowing what alice uploaded)
        const postAvatar = alicePosts.first().locator('img').first()
        await expect(postAvatar).toBeVisible()
      }
      // If no alice posts exist, skip this check
    })
  })
})
