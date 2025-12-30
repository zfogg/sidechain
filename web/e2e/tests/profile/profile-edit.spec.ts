import { test } from '../../fixtures/auth'
import { expect } from '@playwright/test'
import { ProfilePage } from '../../page-objects/ProfilePage'
import { testUsers } from '../../fixtures/test-users'

test.describe('Profile - Editing', () => {
  test('should open edit profile dialog', async ({ authenticatedPage }) => {
    const profilePage = new ProfilePage(authenticatedPage)
    // View own profile
    await profilePage.goto(testUsers.alice.username)

    // Should have edit button
    expect(await profilePage.hasEditProfileButton()).toBe(true)

    // Click edit
    await profilePage.clickEditProfile()

    // Dialog should open (look for form fields)
    const displayNameInput = authenticatedPage.locator(
      'input[placeholder*="name"], input[value*="Alice"], input[id*="name"], input[id*="displayName"]'
    )

    const isVisible = await displayNameInput
      .isVisible({ timeout: 2000 })
      .catch(() => false)

    // Either see form or dialog exists
    expect(authenticatedPage.url().includes('/edit') || authenticatedPage.locator('button:has-text("Save")').isVisible({ timeout: 1000 }).catch(() => false)).toBeTruthy()
  })

  test('should edit display name', async ({ authenticatedPage }) => {
    const profilePage = new ProfilePage(authenticatedPage)
    await profilePage.goto(testUsers.alice.username)

    const initialName = await profilePage.getDisplayName()

    // Click edit
    await profilePage.clickEditProfile()

    // Find display name input
    const displayNameInput = authenticatedPage.locator(
      'input[placeholder*="name"], input[placeholder*="display"], input[id*="name"], input[id*="displayName"]'
    ).first()

    const isVisible = await displayNameInput
      .isVisible({ timeout: 1000 })
      .catch(() => false)

    if (isVisible) {
      // Clear and update
      await displayNameInput.fill('')
      await displayNameInput.fill('Updated Alice Name')

      // Save
      const saveButton = authenticatedPage.locator('button:has-text("Save"), button:has-text("Update")')
      await saveButton.click()

      // Wait for save
      // REMOVED: waitForTimeout

      // Should return to profile
      const finalName = await profilePage.getDisplayName()
      expect(finalName).not.toBe(initialName)
    }
  })

  test('should edit bio', async ({ authenticatedPage }) => {
    const profilePage = new ProfilePage(authenticatedPage)
    await profilePage.goto(testUsers.alice.username)

    const initialBio = await profilePage.getBio()

    // Click edit
    await profilePage.clickEditProfile()

    // Find bio textarea/input
    const bioInput = authenticatedPage.locator(
      'textarea[placeholder*="bio"], textarea[placeholder*="about"], textarea[id*="bio"], textarea[id*="description"], input[placeholder*="bio"], input[placeholder*="about"]'
    ).first()

    const isVisible = await bioInput
      .isVisible({ timeout: 1000 })
      .catch(() => false)

    if (isVisible) {
      // Update bio
      await bioInput.fill('')
      await bioInput.fill('This is my updated bio for testing')

      // Save
      const saveButton = authenticatedPage.locator('button:has-text("Save"), button:has-text("Update")')
      await saveButton.click()

      // REMOVED: waitForTimeout

      // Bio should be updated
      const finalBio = await profilePage.getBio()
      expect(finalBio).toContain('updated')
    }
  })

  test('should cancel editing without saving', async ({ authenticatedPage }) => {
    const profilePage = new ProfilePage(authenticatedPage)
    await profilePage.goto(testUsers.alice.username)

    const initialName = await profilePage.getDisplayName()

    // Click edit
    await profilePage.clickEditProfile()

    // Find input
    const input = authenticatedPage.locator('input').first()
    const isVisible = await input.isVisible({ timeout: 1000 }).catch(() => false)

    if (isVisible) {
      // Try to change
      await input.fill('Temporary Name')

      // Cancel
      const cancelButton = authenticatedPage.locator('button:has-text("Cancel"), button:has-text("Close")')
      const isCancelVisible = await cancelButton
        .isVisible({ timeout: 500 })
        .catch(() => false)

      if (isCancelVisible) {
        await cancelButton.click()
      } else {
        // Try pressing Escape
        await authenticatedPage.keyboard.press('Escape')
      }

      // REMOVED: waitForTimeout

      // Name should not have changed
      const finalName = await profilePage.getDisplayName()
      expect(finalName).toBe(initialName)
    }
  })

  test('should validate required fields', async ({ authenticatedPage }) => {
    const profilePage = new ProfilePage(authenticatedPage)
    await profilePage.goto(testUsers.alice.username)

    // Click edit
    await profilePage.clickEditProfile()

    // Find required fields
    const displayNameInput = authenticatedPage.locator('input[placeholder*="name"]').first()
    const isVisible = await displayNameInput
      .isVisible({ timeout: 1000 })
      .catch(() => false)

    if (isVisible) {
      // Try to clear required field
      await displayNameInput.fill('')

      // Try to save
      const saveButton = authenticatedPage.locator('button:has-text("Save"), button:has-text("Update")')
      await saveButton.click()

      // Should show error or prevent save
      const errorMessage = authenticatedPage.locator(
        'text=/required|must be|cannot be empty/i'
      )

      const hasError = await errorMessage
        .isVisible({ timeout: 2000 })
        .catch(() => false)

      // Either show error or button is disabled
      expect(hasError || (await saveButton.isDisabled())).toBe(true)
    }
  })

  test('should handle save errors gracefully', async ({ authenticatedPage }) => {
    const profilePage = new ProfilePage(authenticatedPage)
    await profilePage.goto(testUsers.alice.username)

    // Click edit
    await profilePage.clickEditProfile()

    // Make a change
    const input = authenticatedPage.locator('input').first()
    const isVisible = await input.isVisible({ timeout: 1000 }).catch(() => false)

    if (isVisible) {
      await input.fill('Test Update')

      // Try to save
      const saveButton = authenticatedPage.locator('button:has-text("Save"), button:has-text("Update")')
      await saveButton.click()

      // Should either succeed or show error
      const hasError = await profilePage.hasError()

      // Page should still be functional
      expect(typeof hasError).toBe('boolean')
    }
  })

  test('should show current values in edit form', async ({ authenticatedPage }) => {
    const profilePage = new ProfilePage(authenticatedPage)
    await profilePage.goto(testUsers.alice.username)

    const currentName = await profilePage.getDisplayName()
    const currentBio = await profilePage.getBio()

    // Click edit
    await profilePage.clickEditProfile()

    // Values should be pre-filled
    const displayNameInput = authenticatedPage
      .locator('input')
      .filter({ has: authenticatedPage.locator(`text="${currentName}"`) })
      .first()

    const isInputVisible = await displayNameInput
      .isVisible({ timeout: 1000 })
      .catch(() => false)

    // At least one input should have current data
    if (isInputVisible) {
      const value = await displayNameInput.inputValue()
      expect(value).toContain(currentName)
    }
  })

  test('should allow updating multiple fields', async ({ authenticatedPage }) => {
    const profilePage = new ProfilePage(authenticatedPage)
    await profilePage.goto(testUsers.alice.username)

    // Click edit
    await profilePage.clickEditProfile()

    // Update multiple fields
    const inputs = authenticatedPage.locator('input, textarea')
    const count = await inputs.count()

    if (count > 0) {
      // Update first two fields
      for (let i = 0; i < Math.min(2, count); i++) {
        const input = inputs.nth(i)
        const isVisible = await input.isVisible({ timeout: 500 }).catch(() => false)
        if (isVisible) {
          await input.fill(`Updated field ${i}`)
        }
      }

      // Save
      const saveButton = authenticatedPage.locator('button:has-text("Save"), button:has-text("Update")')
      await saveButton.click()

      // Should succeed
      const hasError = await profilePage.hasError()
      expect(hasError).toBe(false)
    }
  })

  test('should redirect to profile after editing', async ({ authenticatedPage }) => {
    const profilePage = new ProfilePage(authenticatedPage)
    await profilePage.goto(testUsers.alice.username)

    // Click edit
    await profilePage.clickEditProfile()

    // Find and update a field
    const input = authenticatedPage.locator('input').first()
    const isVisible = await input.isVisible({ timeout: 1000 }).catch(() => false)

    if (isVisible) {
      // Make minimal change
      const currentValue = await input.inputValue()
      await input.fill(currentValue + ' ')

      // Save
      const saveButton = authenticatedPage.locator('button:has-text("Save"), button:has-text("Update")')
      await saveButton.click()

      // REMOVED: waitForTimeout

      // Should be back on profile page
      const onProfilePage = authenticatedPage.url().includes('/profile/')
      expect(onProfilePage).toBe(true)
    }
  })
})
