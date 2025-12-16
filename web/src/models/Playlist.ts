/**
 * Playlist Model
 * Represents a user-created collection of audio tracks
 */

export type PlaylistCollaboratorRole = 'owner' | 'editor' | 'viewer'

export interface PlaylistCollaborator {
  userId: string
  username: string
  displayName: string
  userAvatarUrl?: string
  role: PlaylistCollaboratorRole
  addedAt: Date
}

export interface Playlist {
  id: string
  userId: string
  username: string
  displayName: string
  userAvatarUrl: string

  title: string
  description: string
  coverUrl?: string

  trackIds: string[] // Array of post/track IDs
  trackCount: number

  isPublic: boolean
  isCollaborative: boolean

  followerCount: number
  isFollowing: boolean
  isOwnPlaylist: boolean

  // Collaboration
  collaborators: PlaylistCollaborator[]
  userRole?: PlaylistCollaboratorRole

  createdAt: Date
  updatedAt: Date
}

export interface PlaylistTrack {
  postId: string
  audioUrl: string
  filename: string
  duration: number
  bpm: number
  key: string
  daw: string
  genre: string[]
  displayName: string
  username: string
  userAvatarUrl: string
  addedAt: Date
}

interface PlaylistJSON {
  id: string
  user_id: string
  username: string
  display_name: string
  user_avatar_url?: string
  profile_picture_url?: string
  title: string
  description: string
  cover_url?: string
  track_ids: string[]
  track_count: number
  is_public: boolean
  is_collaborative: boolean
  follower_count: number
  is_following: boolean
  is_own_playlist: boolean
  collaborators: Array<{
    user_id: string
    username: string
    display_name: string
    user_avatar_url: string
    role: PlaylistCollaboratorRole
    added_at: string
  }>
  user_role?: PlaylistCollaboratorRole
  created_at: string
  updated_at: string
}

export class PlaylistModel {
  static fromJson(json: PlaylistJSON): Playlist {
    return {
      id: json.id || '',
      userId: json.user_id || '',
      username: json.username || '',
      displayName: json.display_name || json.username || '',
      userAvatarUrl: json.user_avatar_url || json.profile_picture_url || '',

      title: json.title || 'Untitled Playlist',
      description: json.description || '',
      coverUrl: json.cover_url,

      trackIds: json.track_ids || [],
      trackCount: json.track_count || 0,

      isPublic: json.is_public || false,
      isCollaborative: json.is_collaborative || false,

      followerCount: json.follower_count || 0,
      isFollowing: json.is_following || false,
      isOwnPlaylist: json.is_own_playlist || false,

      collaborators: (json.collaborators || []).map((c) => ({
        userId: c.user_id,
        username: c.username,
        displayName: c.display_name,
        userAvatarUrl: c.user_avatar_url,
        role: (c.role || 'viewer') as PlaylistCollaboratorRole,
        addedAt: new Date(c.added_at),
      })),
      userRole: json.user_role as PlaylistCollaboratorRole | undefined,

      createdAt: new Date(json.created_at || new Date()),
      updatedAt: new Date(json.updated_at || new Date()),
    }
  }

  static isValid(playlist: Playlist): boolean {
    return playlist.id !== '' && playlist.title !== ''
  }

  static toJson(playlist: Playlist): Partial<PlaylistJSON> {
    return {
      title: playlist.title,
      description: playlist.description,
      cover_url: playlist.coverUrl,
      track_ids: playlist.trackIds,
      is_public: playlist.isPublic,
      is_collaborative: playlist.isCollaborative,
    }
  }
}
