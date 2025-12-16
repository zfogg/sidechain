import { test as base, Page } from '@playwright/test'
import { testUsers, TEST_PASSWORD, TestUser } from './test-users'

export interface AuthFixtures {
  authenticatedPage: Page
  authenticatedPageAs: (user: TestUser) => Promise<Page>
  apiToken: string
}

/**
 * Custom test fixture that provides an authenticated page
 * Works with REAL backend authentication against actual database users
 *
 * Usage:
 *   test('should display user profile', async ({ authenticatedPage }) => {
 *     await authenticatedPage.goto('/profile')
 *     // ...
 *   })
 *
 *   test('as different user', async ({ authenticatedPageAs }) => {
 *     const bobPage = await authenticatedPageAs(testUsers.bob)
 *     await bobPage.goto('/profile/bob')
 *   })
 *
 *   test('with API token', async ({ apiToken }) => {
 *     const response = await fetch('/api/v1/users/me', {
 *       headers: { 'Authorization': `Bearer ${apiToken}` }
 *     })
 *   })
 */
export const test = base.extend<AuthFixtures>({
  apiToken: async ({}, use) => {
    const token = await authenticateWithBackend(testUsers.alice)
    await use(token)
  },

  authenticatedPage: async ({ page, apiToken }, use) => {
    await loginAsWithToken(page, apiToken)
    await use(page)
  },

  authenticatedPageAs: async ({ page }, use) => {
    const loginAsUser = async (user: TestUser) => {
      const newPage = await page.context().newPage()
      const token = await authenticateWithBackend(user)
      await loginAsWithToken(newPage, token)
      return newPage
    }

    await use(loginAsUser)
  },
})

/**
 * Authenticate with the real backend via API
 * Returns JWT token from /api/v1/auth/login
 */
async function authenticateWithBackend(user: TestUser): Promise<string> {
  const backendUrl = process.env.BACKEND_URL || 'http://localhost:8787'

  try {
    const response = await fetch(`${backendUrl}/api/v1/auth/login`, {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
      },
      body: JSON.stringify({
        email: user.email,
        password: TEST_PASSWORD,
      }),
    })

    if (!response.ok) {
      const errorText = await response.text()
      throw new Error(
        `Backend login failed: ${response.status} - ${errorText}`
      )
    }

    const data = await response.json()

    if (!data.token) {
      throw new Error('No token in login response')
    }

    console.log(
      `✓ Authenticated as ${user.email} (${user.username}) with token`
    )
    return data.token
  } catch (error) {
    throw new Error(
      `Failed to authenticate with backend: ${error instanceof Error ? error.message : String(error)}`
    )
  }
}

/**
 * Log in using an API token by setting it in localStorage
 * This simulates browser-based auth after receiving token from login endpoint
 */
async function loginAsWithToken(page: Page, token: string): Promise<void> {
  // Navigate to app
  await page.goto('/')

  // Set auth token in localStorage (how the frontend stores it)
  await page.evaluate((t) => {
    localStorage.setItem('auth_token', t)
  }, token)

  // Navigate to feed - should be automatically authorized
  await page.goto('/feed')

  // Wait for page to load and recognize we're authenticated
  // The page should not redirect to /login
  await page.waitForTimeout(500)

  const url = page.url()
  if (url.includes('/login')) {
    throw new Error('Still on login page after setting auth token')
  }
}

/**
 * Helper to log out
 */
export async function logout(page: Page): Promise<void> {
  // Find and click logout button
  const logoutButton = page.locator('button', { hasText: /Logout|Sign Out/ })
  if (await logoutButton.isVisible({ timeout: 1000 }).catch(() => false)) {
    await logoutButton.click()
    await page.waitForURL('/login')
  }

  // Clear auth token
  await page.evaluate(() => localStorage.removeItem('auth_token'))
}
