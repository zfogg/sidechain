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

interface UploadMetadata {
  filename: string
  bpm?: number
  key?: string
  daw?: string
  duration_bars?: number
  genre?: string
  sample_rate?: number
}

/**
 * UploadClient - Handles file uploads and post creation
 *
 * API Endpoints (relative to baseURL http://localhost:8787/api/v1):
 * POST /audio/upload - Upload audio file with metadata
 * POST /posts - Create post with metadata
 */
export class UploadClient {
  /**
   * Upload audio file to CDN
   *
   * Parameters:
   * - file: Audio file to upload
   * - metadata: Upload metadata (filename required, others optional)
   *
   * Returns:
   * - audioUrl: URL to the uploaded audio file
   * - waveformUrl: URL to the generated waveform image
   */
  static async uploadFile(file: File, metadata: UploadMetadata): Promise<Outcome<{ audioUrl: string; waveformUrl: string }>> {
    try {
      const formData = new FormData()
      formData.append('audio', file)
      formData.append('filename', metadata.filename)

      // Add optional metadata fields
      if (metadata.bpm !== undefined) {
        formData.append('bpm', String(metadata.bpm))
      }
      if (metadata.key) {
        formData.append('key', metadata.key)
      }
      if (metadata.daw) {
        formData.append('daw', metadata.daw)
      }
      if (metadata.duration_bars !== undefined) {
        formData.append('duration_bars', String(metadata.duration_bars))
      }
      if (metadata.genre) {
        formData.append('genre', metadata.genre)
      }
      if (metadata.sample_rate !== undefined) {
        formData.append('sample_rate', String(metadata.sample_rate))
      }

      // Debug: log what we're sending
      console.log('[UploadClient] FormData contents:', {
        filename: metadata.filename,
        fileSize: file.size,
        fileName: file.name,
      })

      // Use apiClient.upload() which properly handles FormData without JSON header
      const result = await apiClient.upload<{ audio_url: string; waveform_url: string }>(
        '/audio/upload',
        formData
      )

      if (result.isOk()) {
        const data = result.getValue()
        return Outcome.ok({
          audioUrl: data.audio_url,
          waveformUrl: data.waveform_url,
        })
      }

      return Outcome.error(result.getError())
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
