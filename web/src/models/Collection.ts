/**
 * Collection model
 * Represents a user's custom collection of saved posts
 */

export interface Collection {
  id: string
  userId: string
  title: string
  description: string
  icon: string // emoji or icon name
  color?: string // hex color
  postIds: string[]
  postCount: number
  isPublic: boolean
  createdAt: Date
  updatedAt: Date
}

// API DTO Type
export interface CollectionJSON {
  id: string
  user_id: string
  title: string
  description: string
  icon: string
  color?: string
  post_ids: string[]
  post_count: string | number
  is_public: boolean
  created_at: string
  updated_at: string
}

export class CollectionModel {
  static fromJson(json: CollectionJSON): Collection {
    if (!json) throw new Error('Cannot create Collection from null/undefined')

    return {
      id: json.id || '',
      userId: json.user_id || '',
      title: json.title || 'Untitled Collection',
      description: json.description || '',
      icon: json.icon || '📁',
      color: json.color,
      postIds: json.post_ids || [],
      postCount: parseInt(String(json.post_count) || '0', 10),
      isPublic: json.is_public || false,
      createdAt: new Date(json.created_at || Date.now()),
      updatedAt: new Date(json.updated_at || Date.now()),
    }
  }

  static isValid(collection: Collection): boolean {
    return collection.id !== '' && collection.userId !== '' && collection.title !== ''
  }

  static toJson(collection: Collection): Partial<CollectionJSON> {
    return {
      id: collection.id,
      user_id: collection.userId,
      title: collection.title,
      description: collection.description,
      icon: collection.icon,
      color: collection.color,
      post_ids: collection.postIds,
      is_public: collection.isPublic,
      created_at: collection.createdAt.toISOString(),
      updated_at: collection.updatedAt.toISOString(),
    }
  }
}
