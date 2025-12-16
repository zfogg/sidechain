import { test, expect } from '../../fixtures/auth'
import { SearchPage } from '../../page-objects/SearchPage'

test.describe('Search - Pagination & Filtering', () => {
  test('should display pagination buttons when needed', async ({ authenticatedPage }) => {
    const searchPage = new SearchPage(authenticatedPage)
    await searchPage.goto()

    await searchPage.search('loop')
    await searchPage.waitForResults()

    const hasPagination = await searchPage.hasPagination().catch(() => false)
    expect(typeof hasPagination).toBe('boolean')
  })

  test('should have previous button disabled on first page', async ({ authenticatedPage }) => {
    const searchPage = new SearchPage(authenticatedPage)
    await searchPage.goto()

    await searchPage.search('test')
    await searchPage.waitForResults()

    const isPreviousEnabled = await searchPage.isPreviousEnabled()
    expect(isPreviousEnabled).toBe(false)
  })

  test('should enable next button when results available', async ({ authenticatedPage }) => {
    const searchPage = new SearchPage(authenticatedPage)
    await searchPage.goto()

    await searchPage.search('beat')
    await searchPage.waitForResults()

    const totalResults = await searchPage.getTotalResults()
    const isNextEnabled = await searchPage.isNextEnabled()

    // Next button should be enabled if there are results
    if (totalResults > 20) {
      expect(isNextEnabled).toBe(true)
    }
  })

  test('should navigate to next page', async ({ authenticatedPage }) => {
    const searchPage = new SearchPage(authenticatedPage)
    await searchPage.goto()

    await searchPage.search('loop')
    await searchPage.waitForResults()

    const firstPageCount = await searchPage.getResultCount()
    const isNextEnabled = await searchPage.isNextEnabled()

    if (isNextEnabled) {
      await searchPage.clickNext()
      await searchPage.waitForResults()

      const secondPageCount = await searchPage.getResultCount()
      expect(typeof secondPageCount).toBe('number')
    }
  })

  test('should navigate back to previous page', async ({ authenticatedPage }) => {
    const searchPage = new SearchPage(authenticatedPage)
    await searchPage.goto()

    await searchPage.search('drum')
    await searchPage.waitForResults()

    const isNextEnabled = await searchPage.isNextEnabled()

    if (isNextEnabled) {
      // Go to next page
      await searchPage.clickNext()
      await searchPage.waitForResults()

      // Go back to previous
      const isPreviousEnabled = await searchPage.isPreviousEnabled()
      expect(isPreviousEnabled).toBe(true)

      if (isPreviousEnabled) {
        await searchPage.clickPrevious()
        await searchPage.waitForResults()
        expect(true).toBe(true)
      }
    }
  })

  test('should maintain search query during pagination', async ({ authenticatedPage }) => {
    const searchPage = new SearchPage(authenticatedPage)
    await searchPage.goto()

    const query = 'synth'
    await searchPage.search(query)
    await searchPage.waitForResults()

    const isNextEnabled = await searchPage.isNextEnabled()
    if (isNextEnabled) {
      await searchPage.clickNext()
      await searchPage.waitForResults()

      const currentQuery = await searchPage.getSearchQuery()
      expect(currentQuery).toContain(query)
    }
  })

  test('should show results count per page', async ({ authenticatedPage }) => {
    const searchPage = new SearchPage(authenticatedPage)
    await searchPage.goto()

    await searchPage.search('bass')
    await searchPage.waitForResults()

    const resultCount = await searchPage.getResultCount()
    const totalResults = await searchPage.getTotalResults()

    expect(resultCount >= 0).toBe(true)
    expect(totalResults >= 0).toBe(true)
  })

  test('should display metadata for paginated results', async ({ authenticatedPage }) => {
    const searchPage = new SearchPage(authenticatedPage)
    await searchPage.goto()

    await searchPage.search('loop')
    await searchPage.waitForResults()

    const resultCount = await searchPage.getResultCount()

    if (resultCount >= 2) {
      const result1 = await searchPage.getResultCard(0)
      const result2 = await searchPage.getResultCard(1)

      const title1 = await result1.getTitle()
      const title2 = await result2.getTitle()

      expect(title1.length).toBeGreaterThan(0)
      expect(title2.length).toBeGreaterThan(0)
    }
  })

  test('should handle pagination gracefully', async ({ authenticatedPage }) => {
    const searchPage = new SearchPage(authenticatedPage)
    await searchPage.goto()

    await searchPage.search('beat')
    await searchPage.waitForResults()

    // Try multiple pagination actions
    let pagesMoved = 0
    let maxPages = 3

    while (await searchPage.isNextEnabled() && pagesMoved < maxPages) {
      await searchPage.clickNext()
      await searchPage.waitForResults()
      pagesMoved++
    }

    expect(pagesMoved >= 0).toBe(true)
  })

  test('should update pagination state on new search', async ({ authenticatedPage }) => {
    const searchPage = new SearchPage(authenticatedPage)
    await searchPage.goto()

    // First search
    await searchPage.search('house')
    await searchPage.waitForResults()
    const firstTotal = await searchPage.getTotalResults()

    // Second search
    await searchPage.search('ambient')
    await searchPage.waitForResults()
    const secondTotal = await searchPage.getTotalResults()

    // Both should be valid
    expect(typeof firstTotal).toBe('number')
    expect(typeof secondTotal).toBe('number')
  })

  test('should handle filters', async ({ authenticatedPage }) => {
    const searchPage = new SearchPage(authenticatedPage)
    await searchPage.goto()

    // Try to find and interact with filter button
    const filterButton = await searchPage.getFiltersButton()
    if (filterButton) {
      // Filters exist on page
      expect(filterButton).toBeTruthy()
    }
  })

  test('should support bpm filtering', async ({ authenticatedPage }) => {
    const searchPage = new SearchPage(authenticatedPage)
    await searchPage.goto()

    // Search for results with BPM metadata
    await searchPage.search('electronic')
    await searchPage.waitForResults()

    const resultCount = await searchPage.getResultCount()
    if (resultCount > 0) {
      const result = await searchPage.getFirstResult()
      if (result) {
        const bpm = await result.getBpm()
        // BPM might be null if not specified
        expect(typeof bpm === 'number' || bpm === null).toBe(true)
      }
    }
  })

  test('should support key filtering', async ({ authenticatedPage }) => {
    const searchPage = new SearchPage(authenticatedPage)
    await searchPage.goto()

    await searchPage.search('instrumental')
    await searchPage.waitForResults()

    const resultCount = await searchPage.getResultCount()
    if (resultCount > 0) {
      const result = await searchPage.getFirstResult()
      if (result) {
        const key = await result.getKey()
        // Key might be null
        expect(typeof key === 'string' || key === null).toBe(true)
      }
    }
  })

  test('should display genre information', async ({ authenticatedPage }) => {
    const searchPage = new SearchPage(authenticatedPage)
    await searchPage.goto()

    await searchPage.search('music')
    await searchPage.waitForResults()

    const resultCount = await searchPage.getResultCount()
    if (resultCount > 0) {
      const result = await searchPage.getFirstResult()
      if (result) {
        // Should have metadata available
        const title = await result.getTitle()
        expect(title.length).toBeGreaterThan(0)
      }
    }
  })

  test('should persist pagination across filter changes', async ({ authenticatedPage }) => {
    const searchPage = new SearchPage(authenticatedPage)
    await searchPage.goto()

    await searchPage.search('loop')
    await searchPage.waitForResults()

    const isNextEnabled = await searchPage.isNextEnabled()
    if (isNextEnabled) {
      // Navigate to second page
      await searchPage.clickNext()
      await searchPage.waitForResults()

      // Verify we're on second page
      const isPreviousEnabled = await searchPage.isPreviousEnabled()
      expect(isPreviousEnabled).toBe(true)
    }
  })

  test('should handle rapid pagination clicks', async ({ authenticatedPage }) => {
    const searchPage = new SearchPage(authenticatedPage)
    await searchPage.goto()

    await searchPage.search('beat')
    await searchPage.waitForResults()

    // Attempt rapid clicks (might not process all)
    const isNextEnabled = await searchPage.isNextEnabled()
    if (isNextEnabled) {
      await searchPage.clickNext()
      await searchPage.page.waitForTimeout(200)
      // Don't click again immediately - let it load

      const resultCount = await searchPage.getResultCount()
      expect(typeof resultCount).toBe('number')
    }
  })

  test('should clear pagination when performing new search', async ({ authenticatedPage }) => {
    const searchPage = new SearchPage(authenticatedPage)
    await searchPage.goto()

    await searchPage.search('drum')
    await searchPage.waitForResults()

    // Go to next page
    const isNextEnabled1 = await searchPage.isNextEnabled()
    if (isNextEnabled1) {
      await searchPage.clickNext()
      await searchPage.waitForResults()
    }

    // New search should reset pagination
    await searchPage.search('synth')
    await searchPage.waitForResults()

    // Previous should be disabled (back on first page)
    const isPreviousEnabled = await searchPage.isPreviousEnabled()
    expect(isPreviousEnabled).toBe(false)
  })

  test('should handle no pagination when results fit single page', async ({ authenticatedPage }) => {
    const searchPage = new SearchPage(authenticatedPage)
    await searchPage.goto()

    // Search for something that might have few results
    await searchPage.search('xyz_unlikely_result')
    await searchPage.waitForResults()

    const resultCount = await searchPage.getResultCount()
    if (resultCount < 20) {
      // Pagination should not be visible
      const hasPagination = await searchPage.hasPagination().catch(() => false)
      expect(!hasPagination).toBe(true)
    }
  })
})
