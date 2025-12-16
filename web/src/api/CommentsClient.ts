import { apiClient } from './client';
import { Outcome } from './types';
import { Comment, CommentModel } from '../models/Comment';

/**
 * CommentsClient - Comment CRUD operations
 */

export class CommentsClient {
  /**
   * Get comments for a post
   */
  static async getComments(
    postId: string,
    limit: number = 20,
    offset: number = 0
  ): Promise<Outcome<Comment[]>> {
    const result = await apiClient.get<{ comments: any[]; total: number }>(
      `/posts/${postId}/comments`,
      { limit, offset }
    );

    if (result.isOk()) {
      try {
        const comments = (result.getValue().comments || []).map(CommentModel.fromJson);
        return Outcome.ok(comments);
      } catch (error) {
        return Outcome.error((error as Error).message);
      }
    }

    return Outcome.error(result.getError());
  }

  /**
   * Create a new comment
   */
  static async createComment(
    postId: string,
    content: string,
    parentId?: string
  ): Promise<Outcome<Comment>> {
    const result = await apiClient.post<any>(`/posts/${postId}/comments`, {
      content,
      parent_id: parentId,
    });

    if (result.isOk()) {
      try {
        const comment = CommentModel.fromJson(result.getValue().comment || result.getValue());
        return Outcome.ok(comment);
      } catch (error) {
        return Outcome.error((error as Error).message);
      }
    }

    return Outcome.error(result.getError());
  }

  /**
   * Edit a comment
   */
  static async editComment(commentId: string, content: string): Promise<Outcome<Comment>> {
    const result = await apiClient.put<any>(`/comments/${commentId}`, { content });

    if (result.isOk()) {
      try {
        const comment = CommentModel.fromJson(result.getValue());
        return Outcome.ok(comment);
      } catch (error) {
        return Outcome.error((error as Error).message);
      }
    }

    return Outcome.error(result.getError());
  }

  /**
   * Delete a comment
   */
  static async deleteComment(commentId: string): Promise<Outcome<void>> {
    return apiClient.delete(`/comments/${commentId}`);
  }

  /**
   * Like a comment
   */
  static async toggleCommentLike(commentId: string, shouldLike: boolean): Promise<Outcome<void>> {
    if (shouldLike) {
      return apiClient.post(`/comments/${commentId}/like`, {});
    } else {
      return apiClient.post(`/comments/${commentId}/unlike`, {});
    }
  }
}
