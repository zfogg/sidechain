import { apiClient } from './client'
import { Outcome } from './types'

interface CreatePostData {
  audioUrl: string
  waveformUrl: string
  filename: string
  description?: string
  bpm?: number
  key?: string
  daw?: string
  genre: string[]
  isPublic: boolean
}

/**
 * UploadClient - Handles file uploads and post creation
 *
 * API Endpoints:
 * POST /upload - Upload audio file
 * POST /posts - Create post with metadata
 */
export class UploadClient {
  /**
   * Upload audio file to CDN
   *
   * Returns:
   * - audioUrl: URL to the uploaded audio file
   * - waveformUrl: URL to the generated waveform image
   */
  static async uploadFile(file: File): Promise<Outcome<{ audioUrl: string; waveformUrl: string }>> {
    try {
      const formData = new FormData()
      formData.append('file', file)

      // Use native fetch for better progress tracking (could be enhanced with XMLHttpRequest)
      const response = await fetch('/api/v1/upload', {
        method: 'POST',
        body: formData,
        headers: {
          Authorization: `Bearer ${localStorage.getItem('auth_token') || ''}`,
        },
      })

      if (!response.ok) {
        const error = await response.json().catch(() => ({ error: 'Upload failed' }))
        return Outcome.error(error.error || 'Upload failed')
      }

      const data = await response.json()
      return Outcome.ok({
        audioUrl: data.audio_url,
        waveformUrl: data.waveform_url,
      })
    } catch (error: any) {
      return Outcome.error(error.message || 'Upload failed')
    }
  }

  /**
   * Create a post with metadata
   */
  static async createPost(data: CreatePostData): Promise<Outcome<{ id: string; success: boolean }>> {
    try {
      const result = await apiClient.post<{ id: string }>(
        '/posts',
        {
          audio_url: data.audioUrl,
          waveform_url: data.waveformUrl,
          filename: data.filename,
          description: data.description,
          bpm: data.bpm,
          key: data.key,
          daw: data.daw,
          genre: data.genre,
          is_public: data.isPublic,
        }
      )

      if (result.isOk()) {
        return Outcome.ok({ id: result.getValue().id, success: true })
      }

      return Outcome.error(result.getError())
    } catch (error: any) {
      return Outcome.error(error.message || 'Failed to create post')
    }
  }

  /**
   * Get user's draft posts from localStorage
   */
  static getDrafts(): Array<{
    title: string
    description: string
    bpm: number | null
    key: string
    daw: string
    genre: string[]
    isPublic: boolean
    savedAt: string
  }> {
    const drafts = localStorage.getItem('uploadDrafts')
    return drafts ? JSON.parse(drafts) : []
  }

  /**
   * Save draft to localStorage
   */
  static saveDraft(data: {
    title: string
    description: string
    bpm: number | null
    key: string
    daw: string
    genre: string[]
    isPublic: boolean
  }): void {
    const drafts = this.getDrafts()
    drafts.push({
      ...data,
      savedAt: new Date().toISOString(),
    })
    localStorage.setItem('uploadDrafts', JSON.stringify(drafts))
  }

  /**
   * Delete a draft
   */
  static deleteDraft(index: number): void {
    const drafts = this.getDrafts()
    drafts.splice(index, 1)
    localStorage.setItem('uploadDrafts', JSON.stringify(drafts))
  }
}
