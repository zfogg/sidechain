import { test, expect } from '@playwright/test'
import { test as authenticatedTest, logout } from '../../fixtures/auth'
import { testUsers, TEST_PASSWORD } from '../../fixtures/test-users'

// Tests that work through the UI login flow
test.describe('Authentication - Login (UI Flow)', () => {
  test('should login with valid credentials from real database', async ({
    page,
  }) => {
    await page.goto('/login')

    // Fill in credentials for real test user from database
    await page.locator('input[type="email"]').fill(testUsers.alice.email)
    await page.locator('input[type="password"]').fill(TEST_PASSWORD)

    // Submit form
    await page.locator('button[type="submit"]').click()

    // Should redirect to feed or dashboard
    await page.waitForURL(/\/(feed|dashboard|profile)/, { timeout: 10000 })

    // Verify auth token is set in localStorage
    const token = await page.evaluate(() => localStorage.getItem('auth_token'))
    expect(token).toBeTruthy()
  })

  test('should show error with invalid credentials', async ({ page }) => {
    await page.goto('/login')

    // Fill in invalid credentials
    await page.locator('input[type="email"]').fill('nonexistent@example.com')
    await page.locator('input[type="password"]').fill('wrongpassword')

    // Submit form
    await page.locator('button[type="submit"]').click()

    // Should show error message (backend will return 401 Unauthorized)
    const errorMessage = page.locator('text=/error|invalid|incorrect|unauthorized/i')
    await expect(errorMessage).toBeVisible({ timeout: 5000 })

    // Should NOT redirect
    expect(page.url()).toContain('/login')
  })

  test('should persist auth token across page reloads', async ({ page }) => {
    // Login first through UI
    await page.goto('/login')
    await page.locator('input[type="email"]').fill(testUsers.alice.email)
    await page.locator('input[type="password"]').fill(TEST_PASSWORD)
    await page.locator('button[type="submit"]').click()
    await page.waitForURL(/\/(feed|dashboard|profile)/)

    // Get token after login
    const token1 = await page.evaluate(() => localStorage.getItem('auth_token'))
    expect(token1).toBeTruthy()

    // Reload page
    await page.reload()

    // Token should still be there
    const token2 = await page.evaluate(() => localStorage.getItem('auth_token'))
    expect(token2).toBe(token1)

    // Should not redirect to login
    expect(page.url()).not.toContain('/login')
  })

  test('smoke test: login and view feed', async ({ page }) => {
    // Navigate to login
    await page.goto('/login')

    // Login with real test user
    await page.locator('input[type="email"]').fill(testUsers.alice.email)
    await page.locator('input[type="password"]').fill(TEST_PASSWORD)
    await page.locator('button[type="submit"]').click()

    // Wait for redirect and feed to load
    await page.waitForURL('/feed', { timeout: 10000 })

    // Verify feed page elements are visible
    const timelineButton = page.locator('button', { hasText: 'Timeline' })
    await expect(timelineButton).toBeVisible()

    // Wait for page to fully load
    await page.waitForTimeout(1000)

    // Page should not redirect back to login
    expect(page.url()).toContain('/feed')
  })
})

// Tests using the authenticated fixture (works directly with API tokens)
authenticatedTest.describe('Authentication - Token & Fixtures', () => {
  authenticatedTest(
    'should provide authenticated page via fixture',
    async ({ authenticatedPage }) => {
      // authenticatedPage is already logged in as alice
      // Navigate to profile and verify we see alice's profile
      await authenticatedPage.goto('/profile/alice')

      // Profile should be visible (not redirected to login)
      await authenticatedPage.waitForTimeout(500)
      expect(authenticatedPage.url()).toContain('/profile/alice')
    }
  )

  authenticatedTest(
    'should authenticate as different users',
    async ({ authenticatedPageAs }) => {
      // Login as bob
      const bobPage = await authenticatedPageAs(testUsers.bob)

      // View bob's profile
      await bobPage.goto('/profile/bob')
      await bobPage.waitForTimeout(500)

      expect(bobPage.url()).toContain('/profile/bob')

      // Should see bob's profile data, not alice's
      const displayName = bobPage.locator('text=' + testUsers.bob.displayName)
      await expect(displayName).toBeVisible({ timeout: 5000 })
    }
  )

  authenticatedTest('should have valid JWT token', async ({ apiToken }) => {
    // Parse JWT to verify it contains expected claims
    const parts = apiToken.split('.')
    expect(parts).toHaveLength(3) // JWT has 3 parts: header.payload.signature

    // Decode payload (it's base64url encoded)
    const payload = JSON.parse(
      Buffer.from(parts[1], 'base64url').toString('utf-8')
    )

    // Verify essential claims
    expect(payload.user_id).toBeTruthy()
    expect(payload.email).toBeTruthy()
    expect(payload.exp).toBeTruthy()

    console.log('JWT Payload:', {
      user_id: payload.user_id,
      email: payload.email,
      username: payload.username,
      exp: new Date(payload.exp * 1000).toISOString(),
    })
  })

  authenticatedTest('should logout successfully', async ({
    authenticatedPage,
  }) => {
    // Start authenticated
    await authenticatedPage.goto('/profile/alice')
    expect(await authenticatedPage.evaluate(() => localStorage.getItem('auth_token'))).toBeTruthy()

    // Call logout helper
    await logout(authenticatedPage)

    // Should be logged out
    const token = await authenticatedPage.evaluate(() =>
      localStorage.getItem('auth_token')
    )
    expect(token).toBeFalsy()

    // Should redirect to login on next navigation
    await authenticatedPage.goto('/')
    expect(authenticatedPage.url()).toContain('/login')
  })
})
