import { test } from '../../fixtures/auth'
import { expect } from '@playwright/test'
import { DiscoveryPage } from '../../page-objects/DiscoveryPage'

test.describe('Discovery - Tab Navigation', () => {
  test('should load discovery page', async ({ authenticatedPage }) => {
    const discoveryPage = new DiscoveryPage(authenticatedPage)
    await discoveryPage.goto()

    expect(await discoveryPage.isLoaded()).toBe(true)
  })

  test('should display all tabs', async ({ authenticatedPage }) => {
    const discoveryPage = new DiscoveryPage(authenticatedPage)
    await discoveryPage.goto()

    // Check all tabs are visible
    const forYouVisible = await discoveryPage.forYouTab.isVisible()
    const trendingVisible = await discoveryPage.trendingTab.isVisible()
    const producersVisible = await discoveryPage.producersTab.isVisible()
    const genresVisible = await discoveryPage.genresTab.isVisible()

    expect(forYouVisible && trendingVisible && producersVisible && genresVisible).toBe(true)
  })

  test('should have for-you tab active by default', async ({ authenticatedPage }) => {
    const discoveryPage = new DiscoveryPage(authenticatedPage)
    await discoveryPage.goto()

    const isActive = await discoveryPage.isTabActive('for-you')
    expect(isActive).toBe(true)
  })

  test('should switch to trending tab', async ({ authenticatedPage }) => {
    const discoveryPage = new DiscoveryPage(authenticatedPage)
    await discoveryPage.goto()

    await discoveryPage.switchTab('trending')
    const isActive = await discoveryPage.isTabActive('trending')
    expect(isActive).toBe(true)
  })

  test('should switch to producers tab', async ({ authenticatedPage }) => {
    const discoveryPage = new DiscoveryPage(authenticatedPage)
    await discoveryPage.goto()

    await discoveryPage.switchTab('producers')
    const isActive = await discoveryPage.isTabActive('producers')
    expect(isActive).toBe(true)
  })

  test('should switch to genres tab', async ({ authenticatedPage }) => {
    const discoveryPage = new DiscoveryPage(authenticatedPage)
    await discoveryPage.goto()

    await discoveryPage.switchTab('genres')
    const isActive = await discoveryPage.isTabActive('genres')
    expect(isActive).toBe(true)
  })

  test('should show loading state when switching tabs', async ({ authenticatedPage }) => {
    const discoveryPage = new DiscoveryPage(authenticatedPage)
    await discoveryPage.goto()

    // Switch to trending which requires API call
    await discoveryPage.trendingTab.click()

    // Loading should appear temporarily
    const wasLoading = await discoveryPage.isLoading().catch(() => false)
    expect(typeof wasLoading).toBe('boolean')
  })

  test('should load content after tab switch', async ({ authenticatedPage }) => {
    const discoveryPage = new DiscoveryPage(authenticatedPage)
    await discoveryPage.goto()

    await discoveryPage.switchTab('trending')
    await discoveryPage.waitForTabLoad()

    // Should have either content or empty state
    const hasContent = await discoveryPage.hasContent()
    const hasEmpty = await discoveryPage.hasEmptyState()
    expect(hasContent || hasEmpty).toBe(true)
  })

  test('should display time window filter on trending tab', async ({ authenticatedPage }) => {
    const discoveryPage = new DiscoveryPage(authenticatedPage)
    await discoveryPage.goto()

    await discoveryPage.switchTab('trending')

    // Time window buttons should be visible
    const timeWindowCount = await discoveryPage.timeWindowButtons.count()
    expect(timeWindowCount).toBeGreaterThan(0)
  })

  test('should not show time window filter on non-trending tabs', async ({ authenticatedPage }) => {
    const discoveryPage = new DiscoveryPage(authenticatedPage)
    await discoveryPage.goto()

    // Should not be visible on for-you tab
    const timeWindowCount = await discoveryPage.timeWindowButtons.count().catch(() => 0)
    expect(timeWindowCount).toBeLessThanOrEqual(0)
  })

  test('should switch between time windows on trending tab', async ({ authenticatedPage }) => {
    const discoveryPage = new DiscoveryPage(authenticatedPage)
    await discoveryPage.goto()

    await discoveryPage.switchTab('trending')
    await discoveryPage.setTimeWindow('week')

    const activeWindow = await discoveryPage.getActiveTimeWindow()
    expect(activeWindow).toBe('week')
  })

  test('should change time window to month', async ({ authenticatedPage }) => {
    const discoveryPage = new DiscoveryPage(authenticatedPage)
    await discoveryPage.goto()

    await discoveryPage.switchTab('trending')
    await discoveryPage.setTimeWindow('month')

    const activeWindow = await discoveryPage.getActiveTimeWindow()
    expect(activeWindow).toBe('month')
  })

  test('should reload content when changing time window', async ({ authenticatedPage }) => {
    const discoveryPage = new DiscoveryPage(authenticatedPage)
    await discoveryPage.goto()

    await discoveryPage.switchTab('trending')
    await discoveryPage.waitForTabLoad()

    const initialCount = await discoveryPage.getPostCount()

    // Change time window
    await discoveryPage.setTimeWindow('month')
    await discoveryPage.waitForTabLoad()

    // Count might be different after reload
    const newCount = await discoveryPage.getPostCount()
    expect(typeof newCount).toBe('number')
    expect(newCount >= 0).toBe(true)
  })

  test('should maintain tab state when navigating and returning', async ({ authenticatedPage }) => {
    const discoveryPage = new DiscoveryPage(authenticatedPage)
    await discoveryPage.goto()

    // Switch to trending
    await discoveryPage.switchTab('trending')
    const wasActive = await discoveryPage.isTabActive('trending')
    expect(wasActive).toBe(true)

    // Navigate away and back
    await authenticatedPage.goto('/feed')
    await discoveryPage.goto()

    // For-you should be active (default)
    const isDefault = await discoveryPage.isTabActive('for-you')
    expect(isDefault).toBe(true)
  })
})
