/**
 * CollectionClient - API client for user collections
 * Handles creation, updating, and management of post collections
 */

import { apiClient } from './client'
import { Outcome } from './types'
import { CollectionModel, type Collection } from '../models/Collection'

export class CollectionClient {
  /**
   * Get user's collections
   */
  static async getUserCollections(
    limit: number = 20,
    offset: number = 0
  ): Promise<Outcome<Collection[]>> {
    const result = await apiClient.get<{ collections: any[] }>('/collections', {
      limit,
      offset,
    })

    if (result.isOk()) {
      try {
        const collections = result.getValue().collections.map(CollectionModel.fromJson)
        return Outcome.ok(collections)
      } catch (error) {
        return Outcome.error((error as Error).message)
      }
    }

    return Outcome.error(result.getError())
  }

  /**
   * Get a single collection with all posts
   */
  static async getCollection(collectionId: string): Promise<Outcome<Collection>> {
    const result = await apiClient.get<{ collection: any }>(`/collections/${collectionId}`)

    if (result.isOk()) {
      try {
        const collection = CollectionModel.fromJson(result.getValue().collection)
        return Outcome.ok(collection)
      } catch (error) {
        return Outcome.error((error as Error).message)
      }
    }

    return Outcome.error(result.getError())
  }

  /**
   * Create a new collection
   */
  static async createCollection(
    title: string,
    description: string = '',
    icon: string = '📁',
    isPublic: boolean = false
  ): Promise<Outcome<Collection>> {
    const result = await apiClient.post<{ collection: any }>('/collections', {
      title,
      description,
      icon,
      is_public: isPublic,
    })

    if (result.isOk()) {
      try {
        const collection = CollectionModel.fromJson(result.getValue().collection)
        return Outcome.ok(collection)
      } catch (error) {
        return Outcome.error((error as Error).message)
      }
    }

    return Outcome.error(result.getError())
  }

  /**
   * Update collection metadata
   */
  static async updateCollection(
    collectionId: string,
    updates: {
      title?: string
      description?: string
      icon?: string
      isPublic?: boolean
    }
  ): Promise<Outcome<Collection>> {
    const payload: any = {}
    if (updates.title !== undefined) payload.title = updates.title
    if (updates.description !== undefined) payload.description = updates.description
    if (updates.icon !== undefined) payload.icon = updates.icon
    if (updates.isPublic !== undefined) payload.is_public = updates.isPublic

    const result = await apiClient.put<{ collection: any }>(
      `/collections/${collectionId}`,
      payload
    )

    if (result.isOk()) {
      try {
        const collection = CollectionModel.fromJson(result.getValue().collection)
        return Outcome.ok(collection)
      } catch (error) {
        return Outcome.error((error as Error).message)
      }
    }

    return Outcome.error(result.getError())
  }

  /**
   * Delete a collection
   */
  static async deleteCollection(collectionId: string): Promise<Outcome<void>> {
    return apiClient.delete(`/collections/${collectionId}`)
  }

  /**
   * Add a post to a collection
   */
  static async addPostToCollection(
    collectionId: string,
    postId: string
  ): Promise<Outcome<void>> {
    return apiClient.post(`/collections/${collectionId}/posts`, {
      post_id: postId,
    })
  }

  /**
   * Remove a post from a collection
   */
  static async removePostFromCollection(
    collectionId: string,
    postId: string
  ): Promise<Outcome<void>> {
    return apiClient.delete(`/collections/${collectionId}/posts/${postId}`)
  }

  /**
   * Check if a post is in a collection
   */
  static async isPostInCollection(
    collectionId: string,
    postId: string
  ): Promise<Outcome<boolean>> {
    const result = await apiClient.get<{ isInCollection: boolean }>(
      `/collections/${collectionId}/posts/${postId}`
    )

    if (result.isOk()) {
      return Outcome.ok(result.getValue().isInCollection)
    }

    return Outcome.error(result.getError())
  }

  /**
   * Get collections that contain a specific post
   */
  static async getCollectionsContainingPost(postId: string): Promise<Outcome<Collection[]>> {
    const result = await apiClient.get<{ collections: any[] }>(`/posts/${postId}/collections`)

    if (result.isOk()) {
      try {
        const collections = result.getValue().collections.map(CollectionModel.fromJson)
        return Outcome.ok(collections)
      } catch (error) {
        return Outcome.error((error as Error).message)
      }
    }

    return Outcome.error(result.getError())
  }
}
