import { apiClient } from './client'
import { Outcome } from './types'
import { RemixNode, RemixChain, RemixModel } from '../models/Remix'

interface RemixesResponse {
  remixes: any[]
  total?: number
}

interface RemixChainResponse {
  chain: any
}

interface CreateRemixPayload {
  parentPostId: string
  parentRemixId?: string
  title: string
  description?: string
  audioUrl: string
  midiUrl?: string
}

/**
 * RemixClient - API client for remix chains
 * Handles remix creation, retrieval, and lineage tracking
 */
export class RemixClient {
  /**
   * Get all remixes of a post
   */
  static async getPostRemixes(
    postId: string,
    limit: number = 20,
    offset: number = 0
  ): Promise<Outcome<RemixNode[]>> {
    const result = await apiClient.get<RemixesResponse>(`/posts/${postId}/remixes`, {
      limit,
      offset,
    })

    if (result.isOk()) {
      try {
        const remixes = (result.getValue().remixes || []).map(RemixModel.fromJson)
        return Outcome.ok(remixes)
      } catch (error) {
        return Outcome.error((error as Error).message)
      }
    }

    return Outcome.error(result.getError())
  }

  /**
   * Get remix chain/lineage for a post
   */
  static async getRemixChain(postId: string): Promise<Outcome<RemixChain>> {
    const result = await apiClient.get<RemixChainResponse>(`/posts/${postId}/remix-chain`)

    if (result.isOk()) {
      try {
        const chain = result.getValue().chain
        return Outcome.ok({
          rootPostId: postId,
          remixNodes: (chain.remixes || []).map(RemixModel.fromJson),
          totalRemixes: chain.total || 0,
          depth: chain.depth || 0,
        })
      } catch (error) {
        return Outcome.error((error as Error).message)
      }
    }

    return Outcome.error(result.getError())
  }

  /**
   * Get remix source (original post to remix from)
   */
  static async getRemixSource(postId: string): Promise<Outcome<RemixNode>> {
    const result = await apiClient.get<{ remix: any }>(`/posts/${postId}/remix-source`)

    if (result.isOk()) {
      try {
        const remix = RemixModel.fromJson(result.getValue().remix)
        return Outcome.ok(remix)
      } catch (error) {
        return Outcome.error((error as Error).message)
      }
    }

    return Outcome.error(result.getError())
  }

  /**
   * Create a remix of a post
   */
  static async createRemix(payload: CreateRemixPayload): Promise<Outcome<RemixNode>> {
    const data = {
      parent_post_id: payload.parentPostId,
      parent_remix_id: payload.parentRemixId,
      title: payload.title,
      description: payload.description,
      audio_url: payload.audioUrl,
      midi_url: payload.midiUrl,
    }

    const result = await apiClient.post<{ remix: any }>(`/posts/remixes/create`, data)

    if (result.isOk()) {
      try {
        const remix = RemixModel.fromJson(result.getValue().remix)
        return Outcome.ok(remix)
      } catch (error) {
        return Outcome.error((error as Error).message)
      }
    }

    return Outcome.error(result.getError())
  }

  /**
   * Like a remix
   */
  static async toggleRemixLike(remixId: string, shouldLike: boolean): Promise<Outcome<void>> {
    if (shouldLike) {
      return apiClient.post(`/remixes/${remixId}/like`, {})
    } else {
      return apiClient.delete(`/remixes/${remixId}/like`)
    }
  }

  /**
   * Save a remix
   */
  static async toggleRemixSave(remixId: string, shouldSave: boolean): Promise<Outcome<void>> {
    if (shouldSave) {
      return apiClient.post(`/remixes/${remixId}/save`, {})
    } else {
      return apiClient.delete(`/remixes/${remixId}/save`)
    }
  }

  /**
   * Download remix source
   */
  static async downloadRemixSource(
    remixId: string,
    format: 'audio' | 'midi' | 'both' = 'both'
  ): Promise<Outcome<void>> {
    return apiClient.post(`/remixes/${remixId}/download-source`, { format })
  }
}
