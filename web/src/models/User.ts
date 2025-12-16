/**
 * User model
 * Represents a user profile
 */

export interface User {
  id: string;
  email: string;
  username: string;
  displayName: string;
  bio: string;
  profilePictureUrl: string;

  // Social stats
  followerCount: number;
  followingCount: number;
  postCount: number;

  // Privacy
  isPrivate: boolean;
  isFollowing: boolean;
  isPublic: boolean;
  allowComments: boolean;
  allowDmsFromFollowersOnly: boolean;

  // Social links
  socialLinks: {
    twitter?: string;
    instagram?: string;
    website?: string;
  };

  // Timestamps
  createdAt: Date;
  lastActiveAt?: Date;
}

export class UserModel {
  static fromJson(json: any): User {
    if (!json) throw new Error('Cannot create User from null/undefined');

    return {
      id: json.id || json.user_id || '',
      email: json.email || '',
      username: json.username || '',
      displayName: json.display_name || json.name || json.username || '',
      bio: json.bio || '',
      profilePictureUrl: json.profile_picture_url || json.avatar_url || '',

      followerCount: parseInt(json.follower_count || json.followers_count || '0', 10),
      followingCount: parseInt(json.following_count || json.following || '0', 10),
      postCount: parseInt(json.post_count || json.posts_count || '0', 10),

      isPrivate: json.is_private || json.private || false,
      isFollowing: json.is_following || false,
      isPublic: json.is_public !== false,
      allowComments: json.allow_comments !== false,
      allowDmsFromFollowersOnly: json.allow_dms_from_followers_only || false,

      socialLinks: {
        twitter: json.social_links?.twitter || '',
        instagram: json.social_links?.instagram || '',
        website: json.social_links?.website || '',
      },

      createdAt: new Date(json.created_at || Date.now()),
      lastActiveAt: json.last_active_at ? new Date(json.last_active_at) : undefined,
    };
  }

  static isValid(user: User): boolean {
    return user.id !== '' && user.username !== '';
  }

  static toJson(user: User): any {
    return {
      id: user.id,
      email: user.email,
      username: user.username,
      display_name: user.displayName,
      bio: user.bio,
      profile_picture_url: user.profilePictureUrl,
      created_at: user.createdAt.toISOString(),
    };
  }
}
