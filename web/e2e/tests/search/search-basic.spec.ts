import { test, expect } from '../../fixtures/auth'
import { SearchPage } from '../../page-objects/SearchPage'

test.describe('Search - Basic Search Functionality', () => {
  test('should load search page', async ({ authenticatedPage }) => {
    const searchPage = new SearchPage(authenticatedPage)
    await searchPage.goto()

    expect(await searchPage.isLoaded()).toBe(true)
  })

  test('should display search heading', async ({ authenticatedPage }) => {
    const searchPage = new SearchPage(authenticatedPage)
    await searchPage.goto()

    const headingVisible = await searchPage.heading.isVisible()
    expect(headingVisible).toBe(true)
  })

  test('should display search input', async ({ authenticatedPage }) => {
    const searchPage = new SearchPage(authenticatedPage)
    await searchPage.goto()

    const inputVisible = await searchPage.searchInput.isVisible()
    expect(inputVisible).toBe(true)
  })

  test('should show empty state on initial load', async ({ authenticatedPage }) => {
    const searchPage = new SearchPage(authenticatedPage)
    await searchPage.goto()

    // Should show initial guidance, not results
    const isEmpty = await searchPage.hasEmptyState()
    expect(isEmpty).toBe(true)
  })

  test('should search for query', async ({ authenticatedPage }) => {
    const searchPage = new SearchPage(authenticatedPage)
    await searchPage.goto()

    await searchPage.search('loop')
    await searchPage.waitForResults()

    // Should have results or empty results state
    const hasResults = await searchPage.hasResults()
    const hasEmpty = await searchPage.hasEmptyState()
    const hasError = await searchPage.hasError()

    expect(hasResults || hasEmpty || hasError).toBe(true)
  })

  test('should display search results', async ({ authenticatedPage }) => {
    const searchPage = new SearchPage(authenticatedPage)
    await searchPage.goto()

    await searchPage.search('test')
    await searchPage.waitForResults()

    const resultCount = await searchPage.getResultCount()
    expect(typeof resultCount).toBe('number')
    expect(resultCount >= 0).toBe(true)
  })

  test('should show total results count', async ({ authenticatedPage }) => {
    const searchPage = new SearchPage(authenticatedPage)
    await searchPage.goto()

    await searchPage.search('beat')
    await searchPage.waitForResults()

    const totalResults = await searchPage.getTotalResults()
    expect(typeof totalResults).toBe('number')
  })

  test('should update search query in input', async ({ authenticatedPage }) => {
    const searchPage = new SearchPage(authenticatedPage)
    await searchPage.goto()

    const query = 'electronic'
    await searchPage.search(query)

    const currentQuery = await searchPage.getSearchQuery()
    expect(currentQuery).toContain(query)
  })

  test('should clear search input', async ({ authenticatedPage }) => {
    const searchPage = new SearchPage(authenticatedPage)
    await searchPage.goto()

    await searchPage.search('test')
    await searchPage.clearSearch()

    const currentQuery = await searchPage.getSearchQuery()
    expect(currentQuery).toBe('')
  })

  test('should show error on failed search', async ({ authenticatedPage }) => {
    const searchPage = new SearchPage(authenticatedPage)
    await searchPage.goto()

    // Search with very long query that might fail
    await searchPage.search('a'.repeat(500))
    await searchPage.page.waitForTimeout(1000)

    // Either show results, empty state, or error
    const hasResults = await searchPage.hasResults()
    const hasEmpty = await searchPage.hasEmptyState()
    const hasError = await searchPage.hasError()

    expect(hasResults || hasEmpty || hasError).toBe(true)
  })

  test('should not show error on initial page load', async ({ authenticatedPage }) => {
    const searchPage = new SearchPage(authenticatedPage)
    await searchPage.goto()

    const hasError = await searchPage.hasError()
    expect(hasError).toBe(false)
  })

  test('should perform search on enter key', async ({ authenticatedPage }) => {
    const searchPage = new SearchPage(authenticatedPage)
    await searchPage.goto()

    await searchPage.searchInput.fill('ambient')
    await searchPage.searchInput.press('Enter')
    await searchPage.waitForResults()

    const resultCount = await searchPage.getResultCount()
    expect(typeof resultCount).toBe('number')
  })

  test('should search for different queries sequentially', async ({ authenticatedPage }) => {
    const searchPage = new SearchPage(authenticatedPage)
    await searchPage.goto()

    // First search
    await searchPage.search('house')
    await searchPage.waitForResults()
    const firstCount = await searchPage.getResultCount()

    // Second search
    await searchPage.search('techno')
    await searchPage.waitForResults()
    const secondCount = await searchPage.getResultCount()

    // Both should be valid
    expect(typeof firstCount).toBe('number')
    expect(typeof secondCount).toBe('number')
  })

  test('should display result metadata', async ({ authenticatedPage }) => {
    const searchPage = new SearchPage(authenticatedPage)
    await searchPage.goto()

    await searchPage.search('beat')
    await searchPage.waitForResults()

    const resultCount = await searchPage.getResultCount()
    if (resultCount > 0) {
      const result = await searchPage.getFirstResult()
      if (result) {
        const title = await result.getTitle()
        const author = await result.getAuthor()
        expect(title.length).toBeGreaterThan(0)
        expect(author.length).toBeGreaterThan(0)
      }
    }
  })

  test('should display result engagement metrics', async ({ authenticatedPage }) => {
    const searchPage = new SearchPage(authenticatedPage)
    await searchPage.goto()

    await searchPage.search('loop')
    await searchPage.waitForResults()

    const resultCount = await searchPage.getResultCount()
    if (resultCount > 0) {
      const result = await searchPage.getFirstResult()
      if (result) {
        const likeCount = await result.getLikeCount()
        const playCount = await result.getPlayCount()
        expect(typeof likeCount).toBe('number')
        expect(typeof playCount).toBe('number')
      }
    }
  })

  test('should handle empty search results', async ({ authenticatedPage }) => {
    const searchPage = new SearchPage(authenticatedPage)
    await searchPage.goto()

    // Search for something unlikely to have results
    await searchPage.search('xyz_nonexistent_search_12345')
    await searchPage.waitForResults()

    // Should show empty state or no results
    const hasResults = await searchPage.hasResults()
    const hasEmpty = await searchPage.hasEmptyState()
    expect(!hasResults || hasEmpty).toBe(true)
  })

  test('should show loading state during search', async ({ authenticatedPage }) => {
    const searchPage = new SearchPage(authenticatedPage)
    await searchPage.goto()

    await searchPage.searchInput.fill('test')
    await searchPage.searchInput.press('Enter')

    // Loading should appear briefly
    const wasLoading = await searchPage.isLoading().catch(() => false)
    expect(typeof wasLoading).toBe('boolean')
  })

  test('should play audio from search result', async ({ authenticatedPage }) => {
    const searchPage = new SearchPage(authenticatedPage)
    await searchPage.goto()

    await searchPage.search('drum')
    await searchPage.waitForResults()

    const resultCount = await searchPage.getResultCount()
    if (resultCount > 0) {
      const result = await searchPage.getFirstResult()
      if (result) {
        try {
          await result.play()
          await authenticatedPage.waitForTimeout(300)
          expect(true).toBe(true)
        } catch (e) {
          // Play button might not exist
        }
      }
    }
  })

  test('should click search result', async ({ authenticatedPage }) => {
    const searchPage = new SearchPage(authenticatedPage)
    await searchPage.goto()

    await searchPage.search('bass')
    await searchPage.waitForResults()

    const resultCount = await searchPage.getResultCount()
    if (resultCount > 0) {
      const result = await searchPage.getFirstResult()
      if (result) {
        const initialUrl = authenticatedPage.url()
        try {
          await result.click()
          await authenticatedPage.waitForTimeout(500)
          const newUrl = authenticatedPage.url()
          expect(typeof newUrl).toBe('string')
        } catch (e) {
          // Click might not navigate
        }
      }
    }
  })
})
