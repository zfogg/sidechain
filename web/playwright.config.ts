import { defineConfig, devices } from '@playwright/test'

/**
 * Read environment variables from file.
 * https://github.com/motdotla/dotenv
 */
// require('dotenv').config();

/**
 * See https://playwright.dev/docs/test-configuration.
 */
export default defineConfig({
  testDir: './e2e/tests',
  /* Fast timeouts */
  timeout: 5000,
  expect: { timeout: 2000 },
  /* Run tests in files in parallel */
  fullyParallel: true,
  /* Fail the build on CI if you accidentally left test.only in the source code. */
  forbidOnly: !!process.env.CI,
  /* No retries - fail fast */
  retries: 0,
  /* Max parallelism */
  workers: process.env.CI ? 8 : 16,
  /* Minimal reporting for speed */
  reporter: [['dot']],
  /* Shared settings for all the projects below. See https://playwright.dev/docs/api/class-testoptions. */
  use: {
    /* Base URL to use in actions like `await page.goto('/')`. */
    baseURL: 'http://localhost:5173',
    /* No tracing/screenshots/video - faster */
    trace: 'off',
    screenshot: 'off',
    video: 'off',
    /* Fast action timeouts */
    actionTimeout: 3000,
    navigationTimeout: 5000,
  },

  /* Configure projects for major browsers */
  projects: [
    {
      name: 'chromium',
      use: { ...devices['Desktop Chrome'] },
    },
  ],

  /* Run your local dev server before starting the tests */
  webServer: {
    command: 'bunx vite',
    url: 'http://localhost:5173',
    reuseExistingServer: true, // Always reuse existing server if available
    timeout: 120 * 1000,
  },
})
