/**
 * PlaylistClient - API client for playlist operations
 * Handles CRUD operations for playlists
 */

import { apiClient } from './client'
import { Outcome } from './types'
import { Playlist, PlaylistModel, PlaylistCollaborator, PlaylistCollaboratorRole } from '@/models/Playlist'

// API DTO Types
interface PlaylistDTO {
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
  collaborators: CollaboratorDTO[]
  user_role?: PlaylistCollaboratorRole
  created_at: string
  updated_at: string
}

interface CollaboratorDTO {
  user_id: string
  username: string
  display_name: string
  user_avatar_url: string
  role: PlaylistCollaboratorRole
  added_at: string
}

// API Response Interfaces
interface PlaylistResponse {
  playlist: PlaylistDTO
}

interface PlaylistsResponse {
  playlists: PlaylistDTO[]
}

interface CollaboratorResponse {
  collaborator: CollaboratorDTO
}

interface CollaboratorsResponse {
  collaborators: CollaboratorDTO[]
}

interface PlaylistUpdatePayload {
  name?: string
  description?: string
  is_public?: boolean
  is_collaborative?: boolean
  cover_url?: string
}

export class PlaylistClient {
  /**
   * Get all playlists for current user
   */
  static async getUserPlaylists(limit: number = 20, offset: number = 0): Promise<Outcome<Playlist[]>> {
    const result = await apiClient.get<PlaylistsResponse>('/playlists', { limit, offset })

    if (result.isOk()) {
      const playlists = result.getValue().playlists.map(PlaylistModel.fromJson)
      return Outcome.ok(playlists)
    }

    return Outcome.error(result.getError())
  }

  /**
   * Get public playlists (discovery/feed)
   */
  static async getPublicPlaylists(limit: number = 20, offset: number = 0): Promise<Outcome<Playlist[]>> {
    const result = await apiClient.get<PlaylistsResponse>('/playlists/public', { limit, offset })

    if (result.isOk()) {
      const playlists = result.getValue().playlists.map(PlaylistModel.fromJson)
      return Outcome.ok(playlists)
    }

    return Outcome.error(result.getError())
  }

  /**
   * Get playlist by ID
   */
  static async getPlaylist(playlistId: string): Promise<Outcome<Playlist>> {
    const result = await apiClient.get<PlaylistResponse>(`/playlists/${playlistId}`)

    if (result.isOk()) {
      const playlist = PlaylistModel.fromJson(result.getValue().playlist)
      return Outcome.ok(playlist)
    }

    return Outcome.error(result.getError())
  }

  /**
   * Get playlists for a specific user
   */
  static async getUserPlaylistsByUsername(username: string, limit: number = 20, offset: number = 0): Promise<Outcome<Playlist[]>> {
    const result = await apiClient.get<PlaylistsResponse>(`/users/${username}/playlists`, {
      limit,
      offset,
    })

    if (result.isOk()) {
      const playlists = result.getValue().playlists.map(PlaylistModel.fromJson)
      return Outcome.ok(playlists)
    }

    return Outcome.error(result.getError())
  }

  /**
   * Create new playlist
   */
  static async createPlaylist(title: string, description: string = '', isPublic: boolean = false): Promise<Outcome<Playlist>> {
    const result = await apiClient.post<PlaylistResponse>('/playlists', {
      name: title,
      description,
      is_public: isPublic,
    })

    if (result.isOk()) {
      const playlist = PlaylistModel.fromJson(result.getValue().playlist)
      return Outcome.ok(playlist)
    }

    return Outcome.error(result.getError())
  }

  /**
   * Update playlist metadata
   */
  static async updatePlaylist(
    playlistId: string,
    updates: {
      title?: string
      description?: string
      isPublic?: boolean
      isCollaborative?: boolean
      coverUrl?: string
    }
  ): Promise<Outcome<Playlist>> {
    const payload: PlaylistUpdatePayload = {}
    if (updates.title) payload.name = updates.title
    if (updates.description) payload.description = updates.description
    if (updates.isPublic !== undefined) payload.is_public = updates.isPublic
    if (updates.isCollaborative !== undefined) payload.is_collaborative = updates.isCollaborative
    if (updates.coverUrl) payload.cover_url = updates.coverUrl

    const result = await apiClient.put<PlaylistResponse>(`/playlists/${playlistId}`, payload)

    if (result.isOk()) {
      const playlist = PlaylistModel.fromJson(result.getValue().playlist)
      return Outcome.ok(playlist)
    }

    return Outcome.error(result.getError())
  }

  /**
   * Delete playlist
   */
  static async deletePlaylist(playlistId: string): Promise<Outcome<void>> {
    return apiClient.delete(`/playlists/${playlistId}`)
  }

  /**
   * Add track to playlist
   */
  static async addTrackToPlaylist(playlistId: string, postId: string): Promise<Outcome<Playlist>> {
    const result = await apiClient.post<PlaylistResponse>(`/playlists/${playlistId}/tracks`, {
      post_id: postId,
    })

    if (result.isOk()) {
      const playlist = PlaylistModel.fromJson(result.getValue().playlist)
      return Outcome.ok(playlist)
    }

    return Outcome.error(result.getError())
  }

  /**
   * Remove track from playlist
   */
  static async removeTrackFromPlaylist(playlistId: string, postId: string): Promise<Outcome<Playlist>> {
    const result = await apiClient.delete<PlaylistResponse>(`/playlists/${playlistId}/tracks?post_id=${postId}`)

    if (result.isOk()) {
      const playlist = PlaylistModel.fromJson(result.getValue().playlist)
      return Outcome.ok(playlist)
    }

    return Outcome.error(result.getError())
  }

  /**
   * Reorder tracks in playlist
   */
  static async reorderTracks(playlistId: string, trackIds: string[]): Promise<Outcome<Playlist>> {
    const result = await apiClient.put<PlaylistResponse>(`/playlists/${playlistId}/reorder`, {
      track_ids: trackIds,
    })

    if (result.isOk()) {
      const playlist = PlaylistModel.fromJson(result.getValue().playlist)
      return Outcome.ok(playlist)
    }

    return Outcome.error(result.getError())
  }

  /**
   * Follow/unfollow playlist
   */
  static async toggleFollowPlaylist(playlistId: string, shouldFollow: boolean): Promise<Outcome<void>> {
    if (shouldFollow) {
      return apiClient.post(`/playlists/${playlistId}/follow`, {})
    } else {
      return apiClient.delete(`/playlists/${playlistId}/follow`)
    }
  }

  /**
   * Get playlists that contain a specific track
   */
  static async getPlaylistsContainingTrack(postId: string): Promise<Outcome<Playlist[]>> {
    const result = await apiClient.get<PlaylistsResponse>(`/playlists/containing/${postId}`)

    if (result.isOk()) {
      const playlists = result.getValue().playlists.map(PlaylistModel.fromJson)
      return Outcome.ok(playlists)
    }

    return Outcome.error(result.getError())
  }

  /**
   * Add collaborator to playlist
   */
  static async addCollaborator(
    playlistId: string,
    username: string,
    role: PlaylistCollaboratorRole = 'viewer'
  ): Promise<Outcome<PlaylistCollaborator>> {
    const result = await apiClient.post<CollaboratorResponse>(
      `/playlists/${playlistId}/collaborators`,
      { username, role }
    )

    if (result.isOk()) {
      const collab = result.getValue().collaborator
      return Outcome.ok({
        userId: collab.user_id,
        username: collab.username,
        displayName: collab.display_name,
        userAvatarUrl: collab.user_avatar_url,
        role: collab.role as PlaylistCollaboratorRole,
        addedAt: new Date(collab.added_at),
      })
    }

    return Outcome.error(result.getError())
  }

  /**
   * Remove collaborator from playlist
   */
  static async removeCollaborator(playlistId: string, userId: string): Promise<Outcome<void>> {
    return apiClient.delete(`/playlists/${playlistId}/collaborators/${userId}`)
  }

  /**
   * Update collaborator role
   */
  static async updateCollaboratorRole(
    playlistId: string,
    userId: string,
    role: PlaylistCollaboratorRole
  ): Promise<Outcome<PlaylistCollaborator>> {
    const result = await apiClient.put<CollaboratorResponse>(
      `/playlists/${playlistId}/collaborators/${userId}`,
      { role }
    )

    if (result.isOk()) {
      const collab = result.getValue().collaborator
      return Outcome.ok({
        userId: collab.user_id,
        username: collab.username,
        displayName: collab.display_name,
        userAvatarUrl: collab.user_avatar_url,
        role: collab.role as PlaylistCollaboratorRole,
        addedAt: new Date(collab.added_at),
      })
    }

    return Outcome.error(result.getError())
  }

  /**
   * Get all collaborators for a playlist
   */
  static async getCollaborators(playlistId: string): Promise<Outcome<PlaylistCollaborator[]>> {
    const result = await apiClient.get<CollaboratorsResponse>(
      `/playlists/${playlistId}/collaborators`
    )

    if (result.isOk()) {
      const collaborators = result.getValue().collaborators.map((c: CollaboratorDTO) => ({
        userId: c.user_id,
        username: c.username,
        displayName: c.display_name,
        userAvatarUrl: c.user_avatar_url,
        role: c.role,
        addedAt: new Date(c.added_at),
      }))
      return Outcome.ok(collaborators)
    }

    return Outcome.error(result.getError())
  }
}
