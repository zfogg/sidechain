/**
 * Comment model
 * Represents a comment on a post
 */

export interface Comment {
  id: string;
  postId: string;
  userId: string;
  username: string;
  displayName: string;
  userAvatarUrl: string;

  content: string;
  parentId?: string;

  likeCount: number;
  isLiked: boolean;
  isOwnComment: boolean;

  createdAt: Date;
  updatedAt: Date;
  isEdited: boolean;
}

export class CommentModel {
  static fromJson(json: any): Comment {
    if (!json) throw new Error('Cannot create Comment from null/undefined');

    return {
      id: json.id || '',
      postId: json.post_id || '',
      userId: json.user_id || '',
      username: json.username || '',
      displayName: json.display_name || json.username || '',
      userAvatarUrl: json.user_avatar_url || json.profile_picture_url || '',

      content: json.content || json.text || '',
      parentId: json.parent_id,

      likeCount: parseInt(json.like_count || json.reaction_counts?.like || '0', 10),
      isLiked: json.is_liked || json.own_reactions?.like?.length > 0 || false,
      isOwnComment: json.is_own_comment || json.own_comment || false,

      createdAt: new Date(json.created_at || Date.now()),
      updatedAt: new Date(json.updated_at || Date.now()),
      isEdited: json.is_edited || json.edited || false,
    };
  }

  static isValid(comment: Comment): boolean {
    return comment.id !== '' && comment.userId !== '' && comment.content !== '';
  }

  static toJson(comment: Comment): any {
    return {
      id: comment.id,
      post_id: comment.postId,
      user_id: comment.userId,
      username: comment.username,
      display_name: comment.displayName,
      content: comment.content,
      parent_id: comment.parentId,
      created_at: comment.createdAt.toISOString(),
      updated_at: comment.updatedAt.toISOString(),
    };
  }
}
