/**
 * Story model
 * Represents a user activity event (post created, user followed, post liked, etc.)
 */

export type StoryVerb =
  | 'post'
  | 'like'
  | 'comment'
  | 'follow'
  | 'repost'
  | 'save'
  | 'share'
  | 'joined'

export interface Story {
  id: string
  userId: string
  username: string
  displayName: string
  userAvatarUrl: string

  verb: StoryVerb
  description: string

  // Related objects
  relatedUserId?: string
  relatedUserName?: string
  relatedPostId?: string
  relatedPostTitle?: string

  createdAt: Date
  updatedAt: Date
}

export class StoryModel {
  static fromJson(json: any): Story {
    if (!json) throw new Error('Cannot create Story from null/undefined')

    return {
      id: json.id || '',
      userId: json.user_id || json.actor?.split(':')[1] || '',
      username: json.username || '',
      displayName: json.display_name || json.username || '',
      userAvatarUrl: json.user_avatar_url || json.profile_picture_url || '',

      verb: (json.verb || 'post') as StoryVerb,
      description: json.description || this.generateDescription(json),

      relatedUserId: json.related_user_id || json.target?.split(':')[1],
      relatedUserName: json.related_username,
      relatedPostId: json.related_post_id || json.object?.split(':')[1],
      relatedPostTitle: json.related_post_title,

      createdAt: new Date(json.created_at || json.time || Date.now()),
      updatedAt: new Date(json.updated_at || json.created_at || json.time || Date.now()),
    }
  }

  static isValid(story: Story): boolean {
    return story.id !== '' && story.userId !== ''
  }

  static toJson(story: Story): any {
    return {
      id: story.id,
      user_id: story.userId,
      username: story.username,
      display_name: story.displayName,
      user_avatar_url: story.userAvatarUrl,
      verb: story.verb,
      description: story.description,
      related_user_id: story.relatedUserId,
      related_username: story.relatedUserName,
      related_post_id: story.relatedPostId,
      related_post_title: story.relatedPostTitle,
      created_at: story.createdAt.toISOString(),
      updated_at: story.updatedAt.toISOString(),
    }
  }

  private static generateDescription(json: any): string {
    const verb = json.verb || 'post'
    const name = json.username || 'User'

    switch (verb) {
      case 'post':
        return `${name} posted "${json.related_post_title || 'a new loop'}"`
      case 'like':
        return `${name} liked "${json.related_post_title || 'a post'}"`
      case 'comment':
        return `${name} commented on "${json.related_post_title || 'a post'}"`
      case 'follow':
        return `${name} followed ${json.related_username || 'someone'}`
      case 'repost':
        return `${name} reposted "${json.related_post_title || 'a post'}"`
      case 'save':
        return `${name} saved "${json.related_post_title || 'a post'}"`
      case 'share':
        return `${name} shared "${json.related_post_title || 'a post'}"`
      case 'joined':
        return `${name} joined Sidechain`
      default:
        return `${name} did something`
    }
  }
}
