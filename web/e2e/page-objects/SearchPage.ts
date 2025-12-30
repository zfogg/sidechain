import { Page, Locator } from '@playwright/test'

/**
 * SearchPage - Advanced search page for discovering loops
 * Encapsulates interactions with the Search page
 */
export class SearchPage {
  readonly page: Page
  readonly heading: Locator
  readonly searchInput: Locator
  readonly searchButton: Locator
  readonly filterComponent: Locator
  readonly resultsContainer: Locator
  readonly resultCards: Locator
  readonly resultCount: Locator
  readonly loadingSpinner: Locator
  readonly emptyState: Locator
  readonly errorMessage: Locator
  readonly previousButton: Locator
  readonly nextButton: Locator
  readonly playButtons: Locator

  constructor(page: Page) {
    this.page = page
    this.heading = page.locator('h1:has-text("Discover Music")')
    this.searchInput = page.locator('input[placeholder*="Search loops"]')
    this.searchButton = page.locator('button:has-text("Search")')
    this.filterComponent = page.locator('[class*="filter"]').first()
    this.resultsContainer = page.locator('.lg\\:col-span-3')
    this.resultCards = page.locator('.bg-card.border.rounded-lg.p-4')
    this.resultCount = page.locator('text=/Found \\d+ results/i')
    this.loadingSpinner = page.locator('[data-testid="spinner"], .animate-spin')
    this.emptyState = page.locator('text=/No results found|Use the filters/i')
    this.errorMessage = page.locator('text=/Failed to search/i')
    this.previousButton = page.locator('button:has-text("Previous")')
    this.nextButton = page.locator('button:has-text("Next")')
    this.playButtons = page.locator('button:has-text("▶")')
  }

  /**
   * Navigate to search page
   */
  async goto(): Promise<void> {
    await this.page.goto('/search')
    await this.page.waitForLoadState('domcontentloaded')
  }

  /**
   * Check if page has loaded
   */
  async isLoaded(): Promise<boolean> {
    return await this.heading.isVisible().catch(() => false)
  }

  /**
   * Search for a query
   */
  async search(query: string): Promise<void> {
    await this.searchInput.fill(query)
    await this.searchInput.press('Enter')
    // REMOVED: waitForTimeout // Wait for search to execute
  }

  /**
   * Get search input value
   */
  async getSearchQuery(): Promise<string> {
    return await this.searchInput.inputValue()
  }

  /**
   * Clear search input
   */
  async clearSearch(): Promise<void> {
    await this.searchInput.clear()
  }

  /**
   * Get number of result cards
   */
  async getResultCount(): Promise<number> {
    return await this.resultCards.count()
  }

  /**
   * Get total results text
   */
  async getTotalResults(): Promise<number> {
    const text = await this.resultCount.textContent().catch(() => '0')
    const match = text.match(/\d+/)
    return match ? parseInt(match[0], 10) : 0
  }

  /**
   * Check if results are displayed
   */
  async hasResults(): Promise<boolean> {
    return (await this.getResultCount()) > 0
  }

  /**
   * Check if empty state is shown
   */
  async hasEmptyState(): Promise<boolean> {
    return await this.emptyState.isVisible().catch(() => false)
  }

  /**
   * Check if loading spinner is visible
   */
  async isLoading(): Promise<boolean> {
    return await this.loadingSpinner.isVisible().catch(() => false)
  }

  /**
   * Check if error is shown
   */
  async hasError(): Promise<boolean> {
    return await this.errorMessage.isVisible().catch(() => false)
  }

  /**
   * Get a specific result card by index
   */
  async getResultCard(index: number): Promise<SearchResultCard> {
    const card = this.resultCards.nth(index)
    await card.isVisible()
    return new SearchResultCard(this.page, card)
  }

  /**
   * Get first result card
   */
  async getFirstResult(): Promise<SearchResultCard | null> {
    const count = await this.getResultCount()
    if (count > 0) {
      return await this.getResultCard(0)
    }
    return null
  }

  /**
   * Check if previous button is enabled
   */
  async isPreviousEnabled(): Promise<boolean> {
    return await this.previousButton.isEnabled().catch(() => false)
  }

  /**
   * Check if next button is enabled
   */
  async isNextEnabled(): Promise<boolean> {
    return await this.nextButton.isEnabled().catch(() => false)
  }

  /**
   * Click previous button
   */
  async clickPrevious(): Promise<void> {
    await this.previousButton.click()
  }

  /**
   * Click next button
   */
  async clickNext(): Promise<void> {
    await this.nextButton.click()
  }

  /**
   * Check if pagination buttons are visible
   */
  async hasPagination(): Promise<boolean> {
    const prevVisible = await this.previousButton.isVisible().catch(() => false)
    const nextVisible = await this.nextButton.isVisible().catch(() => false)
    return prevVisible && nextVisible
  }

  /**
   * Wait for search results to load
   */
  async waitForResults(timeout: number = 5000): Promise<void> {
    await Promise.race([
      this.resultCards.first().waitFor({ timeout }),
      this.emptyState.waitFor({ timeout }),
      this.errorMessage.waitFor({ timeout }),
    ]).catch(() => null)
  }

  /**
   * Get search filters button
   */
  async getFiltersButton(): Promise<Locator | null> {
    const btn = this.page.locator('button:has-text("Filters"), [class*="filter"]').first()
    const visible = await btn.isVisible().catch(() => false)
    return visible ? btn : null
  }

  /**
   * Scroll results to bottom
   */
  async scrollToBottom(): Promise<void> {
    await this.resultsContainer.scrollIntoViewIfNeeded()
  }
}

/**
 * SearchResultCard - Represents a single search result
 */
export class SearchResultCard {
  readonly page: Page
  readonly element: Locator
  readonly titleLink: Locator
  readonly authorLink: Locator
  readonly playButton: Locator
  readonly likeCount: Locator
  readonly commentCount: Locator
  readonly playCount: Locator
  readonly bpmText: Locator
  readonly keyText: Locator
  readonly genreText: Locator
  readonly dawText: Locator

  constructor(page: Page, element: Locator) {
    this.page = page
    this.element = element
    this.titleLink = element.locator('h3, .font-semibold').first()
    this.authorLink = element.locator('a, text=/^@/').first()
    this.playButton = element.locator('button:has-text("▶")')
    this.likeCount = element.locator('text=/❤️ \\d+ likes/i')
    this.commentCount = element.locator('text=/💬 \\d+ comments/i')
    this.playCount = element.locator('text=/🎧 \\d+ plays/i')
    this.bpmText = element.locator('text=/\\d+ BPM/i')
    this.keyText = element.locator('text=/🎵/').locator('..')
    this.genreText = element.locator('text=/[A-Z][a-z]+/').locator('..')
    this.dawText = element.locator('text=/🎹/').locator('..')
  }

  /**
   * Get result title
   */
  async getTitle(): Promise<string> {
    return await this.titleLink.textContent().then((t) => t?.trim() || '')
  }

  /**
   * Get author username
   */
  async getAuthor(): Promise<string> {
    return await this.authorLink.textContent().then((t) => t?.replace('@', '').trim() || '')
  }

  /**
   * Get BPM
   */
  async getBpm(): Promise<number | null> {
    const text = await this.bpmText.textContent().catch(() => '')
    const match = text.match(/(\d+)/)
    return match ? parseInt(match[1], 10) : null
  }

  /**
   * Get musical key
   */
  async getKey(): Promise<string | null> {
    return await this.keyText.textContent().then((t) => t?.replace('🎵', '').trim() || null)
  }

  /**
   * Get like count
   */
  async getLikeCount(): Promise<number> {
    const text = await this.likeCount.textContent().catch(() => '0')
    const match = text.match(/(\d+)/)
    return match ? parseInt(match[1], 10) : 0
  }

  /**
   * Get comment count
   */
  async getCommentCount(): Promise<number> {
    const text = await this.commentCount.textContent().catch(() => '0')
    const match = text.match(/(\d+)/)
    return match ? parseInt(match[1], 10) : 0
  }

  /**
   * Get play count
   */
  async getPlayCount(): Promise<number> {
    const text = await this.playCount.textContent().catch(() => '0')
    const match = text.match(/(\d+)/)
    return match ? parseInt(match[1], 10) : 0
  }

  /**
   * Play the audio
   */
  async play(): Promise<void> {
    await this.playButton.click()
  }

  /**
   * Click on the result to navigate
   */
  async click(): Promise<void> {
    await this.element.click()
  }

  /**
   * Check if result is visible
   */
  async isVisible(): Promise<boolean> {
    return await this.element.isVisible().catch(() => false)
  }
}
