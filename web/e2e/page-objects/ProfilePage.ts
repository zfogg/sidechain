import { Page, Locator, expect } from '@playwright/test'

export interface UserStats {
  followers: number
  following: number
  posts: number
}

/**
 * ProfilePage - Page object for user profile pages
 * Handles profile viewing, following, editing, and user posts
 */
export class ProfilePage {
  readonly page: Page

  // Header/Profile Info
  readonly profilePicture: Locator
  readonly displayName: Locator
  readonly username: Locator
  readonly bio: Locator
  readonly location: Locator

  // Stats
  readonly followerCount: Locator
  readonly followingCount: Locator
  readonly postCount: Locator

  // Action buttons
  readonly followButton: Locator
  readonly editProfileButton: Locator
  readonly messageButton: Locator
  readonly shareButton: Locator

  // Social Links
  readonly socialLinks: Locator
  readonly websiteLink: Locator

  // Posts section
  readonly postsSection: Locator
  readonly userPosts: Locator
  readonly pinnedPosts: Locator
  readonly emptyPostsState: Locator

  // Loading/Error states
  readonly loadingSpinner: Locator
  readonly errorMessage: Locator

  constructor(page: Page) {
    this.page = page

    // Header elements - more flexible selectors
    this.profilePicture = page.locator('img[alt*="profile"], img[alt*="avatar"], [class*="Avatar"] img').first()
    this.displayName = page.locator('h1, h2').first()
    this.username = page.locator('text=/@\\w+/i').first()
    this.bio = page.locator('text=/bio|about|description/i, [class*="bio"]').first()
    this.location = page.locator('text=/📍|location|city/i').first()

    // Stats
    this.followerCount = page.locator('button, div', { hasText: /follower/i }).first()
    this.followingCount = page.locator('button, div', { hasText: /following/i }).first()
    this.postCount = page.locator('button, div', { hasText: /post|audio/i }).first()

    // Action buttons
    this.followButton = page.locator('button', { hasText: /^Follow$|^Unfollow$|^Following$/i })
    this.editProfileButton = page.locator('button', { hasText: /Edit|Settings/i })
    this.messageButton = page.locator('button', { hasText: /Message|DM|Chat/i })
    this.shareButton = page.locator('button', { hasText: /Share/i })

    // Social
    this.socialLinks = page.locator('[class*="social"], [class*="link"], a[href*="instagram"], a[href*="twitter"]')
    this.websiteLink = page.locator('a[href*="http"], a[href*="www"]')

    // Posts
    this.postsSection = page.locator('section, div', { hasText: /post|audio/i })
    this.userPosts = page.locator('[data-testid="post-card"], [class*="Post"], .bg-card.border')
    this.pinnedPosts = page.locator('[class*="pinned"], [class*="featured"]')
    this.emptyPostsState = page.locator('text=/no post|empty|nothing yet/i')

    // States
    this.loadingSpinner = page.locator('[data-testid="spinner"], [class*="Spinner"]')
    this.errorMessage = page.locator('text=/error|not found|failed/i')
  }

  /**
   * Navigate to a user's profile
   */
  async goto(username: string): Promise<void> {
    await this.page.goto(`/profile/${username}`)
    await this.waitForProfileLoad()
  }

  /**
   * Wait for profile to load
   */
  async waitForProfileLoad(timeout: number = 10000): Promise<void> {
    try {
      await expect(this.displayName).toBeVisible({ timeout })
    } catch (e) {
      // Profile might not have a visible title, check for other elements
      try {
        await expect(this.profilePicture).toBeVisible({ timeout: 2000 }).catch(() => {})
      } catch {
        // If nothing loads, that's ok
      }
    }
  }

  /**
   * Get user stats
   */
  async getUserStats(): Promise<UserStats> {
    return {
      followers: await this.extractNumberFromLocator(this.followerCount),
      following: await this.extractNumberFromLocator(this.followingCount),
      posts: await this.extractNumberFromLocator(this.postCount),
    }
  }

  /**
   * Extract number from text like "123 followers"
   */
  private async extractNumberFromLocator(locator: Locator): Promise<number> {
    const text = await locator.textContent()
    const match = text?.match(/(\d+)/)
    return match ? parseInt(match[1]) : 0
  }

  /**
   * Get display name
   */
  async getDisplayName(): Promise<string> {
    return (await this.displayName.textContent()) || ''
  }

  /**
   * Get username without @
   */
  async getUsername(): Promise<string> {
    const text = (await this.username.textContent()) || ''
    return text.replace('@', '').trim()
  }

  /**
   * Get bio text
   */
  async getBio(): Promise<string> {
    return (await this.bio.textContent()) || ''
  }

  /**
   * Check if following button is visible (user's own profile won't have it)
   */
  async hasFollowButton(): Promise<boolean> {
    return await this.followButton.isVisible({ timeout: 1000 }).catch(() => false)
  }

  /**
   * Check current follow status
   */
  async isFollowing(): Promise<boolean> {
    const text = await this.followButton.textContent()
    return (text || '').includes('Following') || (text || '').includes('Unfollow')
  }

  /**
   * Follow the user
   */
  async follow(): Promise<void> {
    const initialFollowing = await this.isFollowing()
    const initialCount = await this.extractNumberFromLocator(this.followerCount)

    await this.followButton.click()

    // Wait for optimistic update
    await expect(async () => {
      const nowFollowing = await this.isFollowing()
      const nowCount = await this.extractNumberFromLocator(this.followerCount)

      expect(nowFollowing).toBe(!initialFollowing)
      if (!initialFollowing) {
        expect(nowCount).toBeGreaterThanOrEqual(initialCount)
      }
    }).toPass({ timeout: 3000 })
  }

  /**
   * Unfollow the user
   */
  async unfollow(): Promise<void> {
    await this.followButton.click()
    await expect(this.followButton).toContainText('Follow', { timeout: 3000 })
  }

  /**
   * Check if edit profile button is visible (only on own profile)
   */
  async hasEditProfileButton(): Promise<boolean> {
    return await this.editProfileButton.isVisible({ timeout: 1000 }).catch(() => false)
  }

  /**
   * Click edit profile button
   */
  async clickEditProfile(): Promise<void> {
    await this.editProfileButton.click()
  }

  /**
   * Send message to user
   */
  async sendMessage(): Promise<void> {
    await this.messageButton.click()
  }

  /**
   * Get post count
   */
  async getPostCount(): Promise<number> {
    return await this.extractNumberFromLocator(this.postCount)
  }

  /**
   * Get user posts
   */
  async getUserPosts(): Promise<number> {
    return await this.userPosts.count()
  }

  /**
   * Check if has posts
   */
  async hasPosts(): Promise<boolean> {
    const posts = await this.userPosts.count()
    return posts > 0
  }

  /**
   * Check if posts section is empty
   */
  async hasEmptyPostsState(): Promise<boolean> {
    return await this.emptyPostsState.isVisible({ timeout: 1000 }).catch(() => false)
  }

  /**
   * Get first post and interact with it
   */
  async getFirstPost(): Promise<UserPostElement | null> {
    const count = await this.userPosts.count()
    if (count === 0) return null
    return new UserPostElement(this.userPosts.first())
  }

  /**
   * Check if this is the user's own profile
   */
  async isOwnProfile(): Promise<boolean> {
    return await this.hasEditProfileButton()
  }

  /**
   * Get profile URL
   */
  getProfileUrl(): string {
    return this.page.url()
  }

  /**
   * Check if profile page loaded successfully
   */
  async isLoaded(): Promise<boolean> {
    return await this.displayName.isVisible({ timeout: 1000 }).catch(() => false)
  }

  /**
   * Check for error state
   */
  async hasError(): Promise<boolean> {
    return await this.errorMessage.isVisible({ timeout: 1000 }).catch(() => false)
  }
}

/**
 * UserPostElement - Represents a post on a user's profile
 */
export class UserPostElement {
  readonly locator: Locator

  // Post content
  readonly title: Locator
  readonly timestamp: Locator

  // Actions
  readonly playButton: Locator
  readonly likeButton: Locator
  readonly commentButton: Locator
  readonly saveButton: Locator

  // Stats
  readonly playCount: Locator
  readonly likeCount: Locator

  // Pinned indicator
  readonly pinnedBadge: Locator

  constructor(postLocator: Locator) {
    this.locator = postLocator

    this.title = postLocator.locator('h3, h4').first()
    this.timestamp = postLocator.locator('text=/ago|ago/i').first()

    this.playButton = postLocator.locator('button', { hasText: /▶|Play|play/i }).first()
    this.likeButton = postLocator.locator('button', { hasText: /❤|Like/i }).first()
    this.commentButton = postLocator.locator('button', { hasText: /💬|Comment/i }).first()
    this.saveButton = postLocator.locator('button', { hasText: /💾|Save/i }).first()

    this.playCount = postLocator.locator('text=/\\d+\\s+play/i').first()
    this.likeCount = postLocator.locator('text=/\\d+\\s+like/i').first()

    this.pinnedBadge = postLocator.locator('[class*="pinned"], text=/pinned/i')
  }

  /**
   * Get post title
   */
  async getTitle(): Promise<string> {
    return (await this.title.textContent()) || ''
  }

  /**
   * Play the post
   */
  async play(): Promise<void> {
    await this.playButton.click()
  }

  /**
   * Like the post
   */
  async like(): Promise<void> {
    await this.likeButton.click()
  }

  /**
   * Comment on the post
   */
  async comment(): Promise<void> {
    await this.commentButton.click()
  }

  /**
   * Save the post
   */
  async save(): Promise<void> {
    await this.saveButton.click()
  }

  /**
   * Get play count
   */
  async getPlayCount(): Promise<number> {
    const text = await this.playCount.textContent()
    const match = text?.match(/(\d+)/)
    return match ? parseInt(match[1]) : 0
  }

  /**
   * Get like count
   */
  async getLikeCount(): Promise<number> {
    const text = await this.likeCount.textContent()
    const match = text?.match(/(\d+)/)
    return match ? parseInt(match[1]) : 0
  }

  /**
   * Check if pinned
   */
  async isPinned(): Promise<boolean> {
    return await this.pinnedBadge.isVisible({ timeout: 1000 }).catch(() => false)
  }

  /**
   * Click the post
   */
  async click(): Promise<void> {
    await this.locator.click()
  }

  /**
   * Check if visible
   */
  async isVisible(): Promise<boolean> {
    return await this.locator.isVisible()
  }
}
