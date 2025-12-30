import { Page, Locator } from '@playwright/test'

/**
 * UploadPage - Audio file upload page
 * Encapsulates interactions with the Upload page
 */
export class UploadPage {
  readonly page: Page
  readonly uploadZone: Locator
  readonly titleInput: Locator
  readonly descriptionInput: Locator
  readonly bpmInput: Locator
  readonly keyInput: Locator
  readonly dawSelect: Locator
  readonly genreSelect: Locator
  readonly uploadButton: Locator
  readonly midiUploadInput: Locator
  readonly projectUploadInput: Locator
  readonly isPublicToggle: Locator
  readonly draftManagerSection: Locator
  readonly errorMessage: Locator
  readonly uploadProgress: Locator

  constructor(page: Page) {
    this.page = page
    this.uploadZone = page.locator('[class*="upload"], [class*="dropzone"]').first()
    this.titleInput = page.locator('input[placeholder*="title" i], input[placeholder*="name" i]').first()
    this.descriptionInput = page.locator('textarea[placeholder*="description" i]')
    this.bpmInput = page.locator('input[placeholder*="BPM" i], input[type="number"]').first()
    this.keyInput = page.locator('input[placeholder*="key" i], select[aria-label*="key" i]').first()
    this.dawSelect = page.locator('select[aria-label*="DAW" i], input[placeholder*="DAW" i]')
    this.genreSelect = page.locator('select[aria-label*="genre" i], input[placeholder*="genre" i]')
    this.uploadButton = page.locator('button:has-text("Upload"), button:has-text("Share")')
    this.midiUploadInput = page.locator('input[accept*="mid"]')
    this.projectUploadInput = page.locator('input[accept*="project"]')
    this.isPublicToggle = page.locator('input[type="checkbox"], [role="switch"]').first()
    this.draftManagerSection = page.locator('[class*="draft"]')
    this.errorMessage = page.locator('[role="alert"], .text-red-500').first()
    this.uploadProgress = page.locator('[role="progressbar"], .progress')
  }

  /**
   * Navigate to upload page
   */
  async goto(): Promise<void> {
    await this.page.goto('/upload')
    await this.page.waitForLoadState('domcontentloaded')
  }

  /**
   * Check if page has loaded
   */
  async isLoaded(): Promise<boolean> {
    return await this.uploadZone.isVisible().catch(() => false)
  }

  /**
   * Fill title
   */
  async fillTitle(title: string): Promise<void> {
    await this.titleInput.fill(title)
  }

  /**
   * Get title value
   */
  async getTitle(): Promise<string> {
    return await this.titleInput.inputValue()
  }

  /**
   * Fill description
   */
  async fillDescription(description: string): Promise<void> {
    await this.descriptionInput.fill(description)
  }

  /**
   * Fill BPM
   */
  async fillBpm(bpm: number): Promise<void> {
    await this.bpmInput.fill(bpm.toString())
  }

  /**
   * Fill key
   */
  async fillKey(key: string): Promise<void> {
    await this.keyInput.fill(key)
  }

  /**
   * Select DAW
   */
  async selectDaw(daw: string): Promise<void> {
    if ((await this.dawSelect.evaluate((el) => el.tagName)) === 'SELECT') {
      await this.dawSelect.selectOption(daw)
    } else {
      await this.dawSelect.fill(daw)
    }
  }

  /**
   * Select genre
   */
  async selectGenre(genre: string): Promise<void> {
    if ((await this.genreSelect.evaluate((el) => el.tagName)) === 'SELECT') {
      await this.genreSelect.selectOption(genre)
    } else {
      await this.genreSelect.fill(genre)
    }
  }

  /**
   * Toggle public/private
   */
  async toggleIsPublic(): Promise<void> {
    await this.isPublicToggle.click()
  }

  /**
   * Check if upload button is visible
   */
  async hasUploadButton(): Promise<boolean> {
    return await this.uploadButton.isVisible().catch(() => false)
  }

  /**
   * Click upload button
   */
  async clickUpload(): Promise<void> {
    await this.uploadButton.click()
  }

  /**
   * Check if upload button is disabled
   */
  async isUploadDisabled(): Promise<boolean> {
    return await this.uploadButton.isDisabled().catch(() => false)
  }

  /**
   * Check if error message is shown
   */
  async hasError(): Promise<boolean> {
    return await this.errorMessage.isVisible().catch(() => false)
  }

  /**
   * Get error message
   */
  async getErrorMessage(): Promise<string | null> {
    return await this.errorMessage.textContent().then((t) => t?.trim() || null)
  }

  /**
   * Check if upload progress is visible
   */
  async hasUploadProgress(): Promise<boolean> {
    return await this.uploadProgress.isVisible().catch(() => false)
  }

  /**
   * Check if draft manager section exists
   */
  async hasDraftManager(): Promise<boolean> {
    return await this.draftManagerSection.isVisible().catch(() => false)
  }

  /**
   * Check if MIDI upload input exists
   */
  async hasMidiUploadInput(): Promise<boolean> {
    return await this.midiUploadInput.isVisible().catch(() => false)
  }

  /**
   * Check if project upload input exists
   */
  async hasProjectUploadInput(): Promise<boolean> {
    return await this.projectUploadInput.isVisible().catch(() => false)
  }

  /**
   * Check if form has all required fields
   */
  async hasAllRequiredFields(): Promise<boolean> {
    const titleVisible = await this.titleInput.isVisible().catch(() => false)
    const uploadZoneVisible = await this.uploadZone.isVisible().catch(() => false)

    return titleVisible && uploadZoneVisible
  }

  /**
   * Get number of filled fields
   */
  async getFilledFieldCount(): Promise<number> {
    let count = 0

    const titleValue = await this.titleInput.inputValue().catch(() => '')
    if (titleValue) count++

    const descValue = await this.descriptionInput.inputValue().catch(() => '')
    if (descValue) count++

    const bpmValue = await this.bpmInput.inputValue().catch(() => '')
    if (bpmValue) count++

    return count
  }
}
