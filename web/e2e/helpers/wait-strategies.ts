import { Page, expect } from '@playwright/test'

/**
 * Wait for optimistic updates to complete
 * Useful for testing like/unlike, follow/unfollow operations
 */
export async function waitForOptimisticUpdate(
  page: Page,
  check: () => Promise<boolean>,
  timeout: number = 3000
): Promise<void> {
  await expect(async () => {
    const result = await check()
    if (!result) {
      throw new Error('Optimistic update check failed')
    }
  }).toPass({ timeout })
}

/**
 * Wait for React Query to settle (all queries to finish)
 */
export async function waitForReactQueryToSettle(
  page: Page,
  timeout: number = 5000
): Promise<void> {
  const settled = await page.evaluate(
    ({ timeout: t }) => {
      return new Promise<boolean>((resolve) => {
        const startTime = Date.now()

        const check = () => {
          // Try to access React Query's query client
          const queryClient = (window as any).__reactQueryClient

          if (queryClient?.isFetching?.() === 0) {
            resolve(true)
            return
          }

          if (Date.now() - startTime > t) {
            resolve(false)
            return
          }

          setTimeout(check, 50)
        }

        check()
      })
    },
    { timeout }
  )

  if (!settled) {
    throw new Error('React Query did not settle within timeout')
  }
}

/**
 * Wait for a specific number of elements to appear
 */
export async function waitForElementCount(
  page: Page,
  selector: string,
  expectedCount: number,
  timeout: number = 5000
): Promise<void> {
  await expect(async () => {
    const count = await page.locator(selector).count()
    if (count !== expectedCount) {
      throw new Error(
        `Expected ${expectedCount} elements, but found ${count}`
      )
    }
  }).toPass({ timeout })
}

/**
 * Wait for loading spinner to appear and disappear
 */
export async function waitForLoadingToComplete(
  page: Page,
  spinner: string = '[data-testid="spinner"]',
  timeout: number = 10000
): Promise<void> {
  // Wait for spinner to appear
  const spinnerLocator = page.locator(spinner)

  // If spinner appears, wait for it to disappear
  const isVisible = await spinnerLocator.isVisible({ timeout: 1000 }).catch(
    () => false
  )

  if (isVisible) {
    await expect(spinnerLocator).not.toBeVisible({ timeout })
  }
}

/**
 * Wait for content to load after navigation or action
 */
export async function waitForContentLoad(
  page: Page,
  contentSelector: string,
  timeout: number = 5000
): Promise<void> {
  // Wait for at least one content element to be visible
  await expect(page.locator(contentSelector).first()).toBeVisible({ timeout })
}

/**
 * Wait for all images to load
 */
export async function waitForImagesLoad(
  page: Page,
  timeout: number = 5000
): Promise<void> {
  await page.evaluate(({ timeout: t }) => {
    return new Promise<void>((resolve) => {
      const startTime = Date.now()

      const check = () => {
        const images = Array.from(document.querySelectorAll('img'))
        const allLoaded = images.every(
          (img) => img.complete || img.naturalHeight !== 0
        )

        if (allLoaded) {
          resolve()
          return
        }

        if (Date.now() - startTime > t) {
          resolve() // Resolve anyway after timeout
          return
        }

        setTimeout(check, 100)
      }

      check()
    })
  }, { timeout })
}

/**
 * Wait for a specific state change in the UI
 */
export async function waitForStateChange(
  page: Page,
  getState: () => Promise<unknown>,
  isStateChanged: (oldState: unknown, newState: unknown) => boolean,
  timeout: number = 3000
): Promise<void> {
  const initialState = await getState()

  await expect(async () => {
    const currentState = await getState()
    if (!isStateChanged(initialState, currentState)) {
      throw new Error('State has not changed')
    }
  }).toPass({ timeout })
}

/**
 * Wait for URL to match pattern (more flexible than waitForURL)
 */
export async function waitForUrlPattern(
  page: Page,
  pattern: RegExp,
  timeout: number = 5000
): Promise<void> {
  await expect(async () => {
    if (!pattern.test(page.url())) {
      throw new Error(`URL ${page.url()} does not match pattern ${pattern}`)
    }
  }).toPass({ timeout })
}

/**
 * Wait for network activity to settle (all pending requests to complete)
 */
export async function waitForNetworkIdle(
  page: Page,
  timeout: number = 5000
): Promise<void> {
  try {
    // In Playwright, we can use page.waitForLoadState
    await page.waitForLoadState('networkidle', { timeout })
  } catch (error) {
    // If network idle times out, that's okay - continue
    console.log('Network idle timeout (continuing anyway):', error)
  }
}

/**
 * Wait for local storage to be updated
 */
export async function waitForLocalStorageUpdate(
  page: Page,
  key: string,
  expectedValue?: string,
  timeout: number = 3000
): Promise<void> {
  await expect(async () => {
    const value = await page.evaluate((k) => localStorage.getItem(k), key)

    if (expectedValue && value !== expectedValue) {
      throw new Error(
        `LocalStorage ${key} is "${value}", expected "${expectedValue}"`
      )
    }

    if (!value) {
      throw new Error(`LocalStorage ${key} is not set`)
    }
  }).toPass({ timeout })
}

/**
 * Wait for a toast/notification message to appear and disappear
 */
export async function waitForNotification(
  page: Page,
  messagePattern: RegExp | string,
  timeout: number = 5000
): Promise<void> {
  const pattern =
    typeof messagePattern === 'string'
      ? new RegExp(messagePattern)
      : messagePattern

  // Wait for notification to appear
  const notification = page.locator('text=' + pattern.source)
  await expect(notification).toBeVisible({ timeout })

  // Wait for notification to disappear (toast auto-dismisses)
  // Or just verify it was shown
  // await expect(notification).not.toBeVisible({ timeout: 3000 })
}
