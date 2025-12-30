import { test as base, Page } from '@playwright/test'
import { testUsers, TEST_PASSWORD, TestUser } from './test-users'

export interface AuthFixtures {
  authenticatedPage: Page
  authenticatedPageAs: (user: TestUser) => Promise<Page>
  apiToken: string
}

// Token cache - shared across all tests
const tokenCache = new Map<string, string>()

export const test = base.extend<AuthFixtures>({
  apiToken: async ({}, use) => {
    const token = await getToken(testUsers.alice)
    await use(token)
  },

  authenticatedPage: async ({ page, apiToken }, use) => {
    await setupAuth(page, apiToken)
    await use(page)
  },

  authenticatedPageAs: async ({ page }, use) => {
    await use(async (user: TestUser) => {
      const newPage = await page.context().newPage()
      const token = await getToken(user)
      await setupAuth(newPage, token)
      return newPage
    })
  },
})

async function getToken(user: TestUser): Promise<string> {
  // Return cached token immediately
  const cached = tokenCache.get(user.email)
  if (cached) return cached

  const backendUrl = process.env.BACKEND_URL || 'http://localhost:8787'

  const response = await fetch(`${backendUrl}/api/v1/auth/login`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ email: user.email, password: TEST_PASSWORD }),
  })

  if (!response.ok) {
    throw new Error(`Auth failed: ${response.status} - ${await response.text()}`)
  }

  const data = await response.json()
  const token = data.auth?.token || data.token
  if (!token) throw new Error('No token in response')

  tokenCache.set(user.email, token)
  return token
}

async function setupAuth(page: Page, token: string): Promise<void> {
  // Set both auth_token and the Zustand store state
  await page.addInitScript((t) => {
    localStorage.setItem('auth_token', t)
    // Set Zustand user-store state so isAuthenticated is true on first render
    const storeState = {
      state: {
        user: { id: 'test-user', username: 'testuser', displayName: 'Test User' },
        token: t,
        isAuthenticated: true,
      },
      version: 0,
    }
    localStorage.setItem('user-store', JSON.stringify(storeState))
  }, token)
  await page.goto('/feed', { waitUntil: 'domcontentloaded' })
}

export async function logout(page: Page): Promise<void> {
  await page.evaluate(() => localStorage.removeItem('auth_token'))
}
