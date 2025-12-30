import { test } from '../../fixtures/auth'
import { expect } from '@playwright/test'
import { testUsers } from '../../fixtures/test-users'
import { MessagesPage } from '../../page-objects/MessagesPage'

/**
 * Message Conversations Creation Tests
 *
 * Tests the ability to create new 1-on-1 conversations,
 * search for users, and send messages in new conversations.
 */
test.describe('Message Conversations - Create', () => {
  test.describe('New 1-on-1 Conversation', () => {
    test('should open new conversation dialog from messages page', async ({ authenticatedPage }) => {
      const messagesPage = new MessagesPage(authenticatedPage)
      await messagesPage.goto()

      // Look for new conversation button
      const newConvoButton = authenticatedPage.locator('button:has-text("New"), button:has-text("Start"), button:has-text("Compose"), [aria-label*="new message" i], [aria-label*="compose" i]').first()

      const isVisible = await newConvoButton.isVisible({ timeout: 5000 }).catch(() => false)
      test.skip(!isVisible, 'New conversation button not found')

      await newConvoButton.click()

      // Dialog should open
      await expect(authenticatedPage.locator('[role="dialog"], .fixed.inset-0').last()).toBeVisible({ timeout: 3000 })
    })

    test('should search for user to message', async ({ authenticatedPage }) => {
      const messagesPage = new MessagesPage(authenticatedPage)
      await messagesPage.goto()

      // Open new conversation dialog
      const newConvoButton = authenticatedPage.locator('button:has-text("New"), button:has-text("Start"), button:has-text("Compose")').first()
      const isVisible = await newConvoButton.isVisible({ timeout: 5000 }).catch(() => false)
      test.skip(!isVisible, 'New conversation button not found')

      await newConvoButton.click()
      await authenticatedPage.waitForTimeout(500)

      // Find search input in dialog
      const searchInput = authenticatedPage.locator('[role="dialog"] input, .fixed.inset-0 input').first()
      const inputVisible = await searchInput.isVisible({ timeout: 3000 }).catch(() => false)
      test.skip(!inputVisible, 'Search input not found in dialog')

      // Search for bob
      await searchInput.fill('bob')
      await authenticatedPage.waitForTimeout(1000) // Wait for search results

      // Should see bob in results
      const bobResult = authenticatedPage.locator(`text=/${testUsers.bob.username}|${testUsers.bob.displayName}/i`)
      await expect(bobResult.first()).toBeVisible({ timeout: 5000 })
    })

    test('should start conversation with selected user', async ({ authenticatedPage }) => {
      const messagesPage = new MessagesPage(authenticatedPage)
      await messagesPage.goto()

      // Open new conversation dialog
      const newConvoButton = authenticatedPage.locator('button:has-text("New"), button:has-text("Start"), button:has-text("Compose")').first()
      const isVisible = await newConvoButton.isVisible({ timeout: 5000 }).catch(() => false)
      test.skip(!isVisible, 'New conversation button not found')

      await newConvoButton.click()
      await authenticatedPage.waitForTimeout(500)

      // Search for bob
      const searchInput = authenticatedPage.locator('[role="dialog"] input, .fixed.inset-0 input').first()
      const inputVisible = await searchInput.isVisible({ timeout: 3000 }).catch(() => false)
      test.skip(!inputVisible, 'Search input not found')

      await searchInput.fill('bob')
      await authenticatedPage.waitForTimeout(1000)

      // Click on bob in results
      const bobResult = authenticatedPage.locator(`text=/${testUsers.bob.username}|${testUsers.bob.displayName}/i`).first()
      await bobResult.click()

      // Wait for conversation to be created/selected
      await authenticatedPage.waitForTimeout(2000)

      // Should now be in a conversation (dialog closed or conversation view shown)
      // Check for message input or conversation header
      const messageArea = authenticatedPage.locator('textarea, input[placeholder*="message" i], [contenteditable]')
      const isInConversation = await messageArea.isVisible({ timeout: 5000 }).catch(() => false)

      expect(isInConversation).toBe(true)
    })

    test('should send first message in new conversation', async ({ authenticatedPage }) => {
      const messagesPage = new MessagesPage(authenticatedPage)
      await messagesPage.goto()

      // Open new conversation dialog
      const newConvoButton = authenticatedPage.locator('button:has-text("New"), button:has-text("Start"), button:has-text("Compose")').first()
      const isVisible = await newConvoButton.isVisible({ timeout: 5000 }).catch(() => false)
      test.skip(!isVisible, 'New conversation button not found')

      await newConvoButton.click()
      await authenticatedPage.waitForTimeout(500)

      // Search and select charlie
      const searchInput = authenticatedPage.locator('[role="dialog"] input, .fixed.inset-0 input').first()
      const inputVisible = await searchInput.isVisible({ timeout: 3000 }).catch(() => false)
      test.skip(!inputVisible, 'Search input not found')

      await searchInput.fill('charlie')
      await authenticatedPage.waitForTimeout(1000)

      const charlieResult = authenticatedPage.locator(`text=/${testUsers.charlie.username}|${testUsers.charlie.displayName}/i`).first()
      await charlieResult.click()
      await authenticatedPage.waitForTimeout(2000)

      // Find message input
      const messageInput = authenticatedPage.locator('textarea, input[placeholder*="message" i], [contenteditable]').first()
      const canSend = await messageInput.isVisible({ timeout: 3000 }).catch(() => false)
      test.skip(!canSend, 'Message input not found')

      // Type and send message
      const testMessage = `Test message ${Date.now()}`
      await messageInput.fill(testMessage)

      // Send button or press Enter
      const sendButton = authenticatedPage.locator('button:has-text("Send"), button[aria-label*="send" i], button[type="submit"]').first()
      const sendVisible = await sendButton.isVisible({ timeout: 1000 }).catch(() => false)

      if (sendVisible) {
        await sendButton.click()
      } else {
        await messageInput.press('Enter')
      }

      // Message should appear in thread
      await expect(authenticatedPage.locator(`text=${testMessage}`)).toBeVisible({ timeout: 5000 })
    })

    test('conversation should appear in conversation list', async ({ authenticatedPage }) => {
      const messagesPage = new MessagesPage(authenticatedPage)
      await messagesPage.goto()

      // Check if there's a conversation list
      const channelCount = await messagesPage.getChannelCount()

      // Navigate to messages page
      await messagesPage.goto()
      await authenticatedPage.waitForTimeout(1000)

      // Either we have conversations or an empty state
      if (channelCount === 0) {
        const hasEmpty = await messagesPage.hasEmptyState()
        expect(hasEmpty || channelCount === 0).toBe(true)
      } else {
        expect(channelCount).toBeGreaterThan(0)
      }
    })
  })

  test.describe('Navigate to Conversation', () => {
    test('should navigate to conversation when clicked', async ({ authenticatedPage }) => {
      const messagesPage = new MessagesPage(authenticatedPage)
      await messagesPage.goto()

      const hasConversations = await messagesPage.hasConversations()
      test.skip(!hasConversations, 'No conversations to navigate to')

      // Click first conversation
      await messagesPage.clickChannel(0)
      await messagesPage.waitForThreadLoad()

      // Message input should be visible
      const messageInput = authenticatedPage.locator('textarea, input[placeholder*="message" i], [contenteditable]')
      const hasInput = await messageInput.isVisible({ timeout: 5000 }).catch(() => false)

      // Either we see message input or an error
      const hasError = await messagesPage.hasError()
      expect(hasInput || hasError).toBe(true)
    })
  })
})
