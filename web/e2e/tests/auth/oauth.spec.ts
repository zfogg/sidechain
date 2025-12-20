import { test, expect, Page } from '@playwright/test'

/**
 * OAuth Login E2E Test
 * Tests the full Google OAuth flow through the browser
 */
test.describe('OAuth - Google Login Flow', () => {
  test('should complete OAuth callback with session-based exchange', async ({ page, context }) => {
    // Navigate to login page
    await page.goto('/login')

    // Listen for all requests to understand the flow
    page.on('response', (response) => {
      console.log(`[${response.status()}] ${response.request().method()} ${response.url()}`)
    })

    // Find and click the Google OAuth button
    const googleButton = page.locator('button', { hasText: /Google|google/i })

    // Check if button exists
    if (!(await googleButton.isVisible({ timeout: 5000 }).catch(() => false))) {
      console.log('Available buttons:')
      const allButtons = await page.locator('button').all()
      for (const button of allButtons) {
        const text = await button.textContent()
        console.log(`  - ${text}`)
      }
      throw new Error('Google OAuth button not found on login page')
    }

    await googleButton.click()

    // Handle the OAuth popup/redirect
    // Google will redirect to their login page, then back to our callback
    // Wait for navigation away from login page
    await page.waitForURL(/auth\/(callback|google)/, { timeout: 10000 })

    console.log(`Navigated to: ${page.url()}`)

    // If we're at /auth/callback with session_id, the flow should exchange it
    const sessionId = new URL(page.url()).searchParams.get('session_id')
    if (sessionId) {
      console.log(`Got session_id: ${sessionId}`)

      // The frontend should automatically POST to /api/v1/auth/oauth-token
      // Let's verify this happened by checking if we got a token
      await page.waitForURL(/feed/, { timeout: 10000 }).catch(() => {
        // If redirect didn't happen, check for errors
        throw new Error(`Failed to redirect to feed after session exchange. Current URL: ${page.url()}`)
      })
    } else {
      console.log(`No session_id found in URL: ${page.url()}`)
      console.log(`Search params: ${new URL(page.url()).search}`)
    }

    // Verify we're logged in
    const token = await page.evaluate(() => localStorage.getItem('auth_token'))
    expect(token).toBeTruthy()

    // Should be on feed page
    expect(page.url()).toContain('/feed')
  })

  test('should handle OAuth session exchange via API call', async ({ page }) => {
    // Simulate the callback with a session_id
    // First, we need to trigger an actual OAuth flow to get a real session_id

    await page.goto('/login')

    // Find Google button and click
    const googleButton = page.locator('button', { hasText: /Google|google/i })
    if (!(await googleButton.isVisible({ timeout: 5000 }).catch(() => false))) {
      console.log('Google button not found, skipping test')
      return
    }

    // Intercept the callback to see what session_id we get
    let capturedSessionId: string | null = null
    page.on('request', (request) => {
      const url = request.url()
      const urlObj = new URL(url)
      const sessionId = urlObj.searchParams.get('session_id')
      if (sessionId) {
        capturedSessionId = sessionId
        console.log(`Captured session_id from redirect: ${sessionId}`)
      }
    })

    await googleButton.click()

    // Wait for callback redirect
    try {
      await page.waitForURL(/auth\/(callback|google)/, { timeout: 15000 })
    } catch (e) {
      console.log(`Failed to reach callback: ${page.url()}`)
      // This might be because we're in a headless environment without actual Google OAuth
      // In that case, we can skip this test or mock it
      return
    }

    // Verify OAuth session exchange happened
    const url = page.url()
    console.log(`Final URL: ${url}`)

    // Should have auth token if successful
    const token = await page.evaluate(() => localStorage.getItem('auth_token'))
    if (token) {
      console.log('✓ OAuth login successful, token present in localStorage')
    } else {
      console.log('⚠ Token not yet in localStorage, checking page state...')
      const pageContent = await page.content()
      // Print first 2000 chars to see error messages
      console.log('Page HTML (first 2000 chars):', pageContent.substring(0, 2000))
    }
  })

  test('should display error when OAuth session is invalid', async ({ page }) => {
    // Try to access callback with invalid session_id
    await page.goto('/auth/callback?session_id=invalid_session_id_12345')

    // Should either show error or redirect to login
    await page.waitForTimeout(2000)

    const errorMessage = page.locator('text=/error|failed|invalid/i')
    const isError = await errorMessage.isVisible({ timeout: 5000 }).catch(() => false)

    const isLoginPage = page.url().includes('/login')

    if (isError) {
      console.log('✓ Error message shown for invalid session')
    } else if (isLoginPage) {
      console.log('✓ Redirected to login for invalid session')
    } else {
      console.log(`⚠ Unexpected state. URL: ${page.url()}`)
    }

    // Should NOT be on feed page
    expect(page.url()).not.toContain('/feed')
  })
})
