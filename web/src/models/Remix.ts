/**
 * Remix model - Remix chain and lineage tracking
 * Users can remix posts with their own audio/MIDI
 */

export interface RemixNode {
  id: string
  postId: string
  userId: string
  username: string
  displayName: string
  userAvatarUrl?: string

  title: string
  description?: string

  audioUrl: string
  midiUrl?: string
  waveformUrl?: string

  bpm?: number
  key?: string
  genre?: string[]

  // Metadata
  remixCount: number
  likeCount: number
  playCount: number
  downloadCount: number

  isLiked?: boolean
  isSaved?: boolean
  isOwnRemix?: boolean

  // Lineage
  parentRemixId?: string
  parentPostId?: string

  createdAt: Date
  updatedAt: Date
}

export interface RemixChain {
  rootPostId: string
  remixNodes: RemixNode[]
  totalRemixes: number
  depth: number
}

export interface RemixTree {
  id: string
  nodeId: string
  children: RemixTree[]
}

export class RemixModel {
  static fromJson(json: any): RemixNode {
    if (!json) throw new Error('Cannot create Remix from null/undefined')

    return {
      id: json.id || '',
      postId: json.post_id || '',
      userId: json.user_id || '',
      username: json.username || '',
      displayName: json.display_name || '',
      userAvatarUrl: json.user_avatar_url,

      title: json.title || '',
      description: json.description,

      audioUrl: json.audio_url || '',
      midiUrl: json.midi_url,
      waveformUrl: json.waveform_url,

      bpm: json.bpm,
      key: json.key,
      genre: json.genre || [],

      remixCount: json.remix_count || 0,
      likeCount: json.like_count || 0,
      playCount: json.play_count || 0,
      downloadCount: json.download_count || 0,

      isLiked: json.is_liked || false,
      isSaved: json.is_saved || false,
      isOwnRemix: json.is_own_remix || false,

      parentRemixId: json.parent_remix_id,
      parentPostId: json.parent_post_id,

      createdAt: new Date(json.created_at || Date.now()),
      updatedAt: new Date(json.updated_at || Date.now()),
    }
  }

  static isValid(remix: RemixNode): boolean {
    return remix.id !== '' && remix.postId !== '' && remix.userId !== ''
  }

  static toJson(remix: RemixNode): any {
    return {
      id: remix.id,
      post_id: remix.postId,
      user_id: remix.userId,
      username: remix.username,
      display_name: remix.displayName,
      user_avatar_url: remix.userAvatarUrl,
      title: remix.title,
      description: remix.description,
      audio_url: remix.audioUrl,
      midi_url: remix.midiUrl,
      waveform_url: remix.waveformUrl,
      bpm: remix.bpm,
      key: remix.key,
      genre: remix.genre,
      remix_count: remix.remixCount,
      like_count: remix.likeCount,
      play_count: remix.playCount,
      download_count: remix.downloadCount,
      is_liked: remix.isLiked,
      is_saved: remix.isSaved,
      is_own_remix: remix.isOwnRemix,
      parent_remix_id: remix.parentRemixId,
      parent_post_id: remix.parentPostId,
      created_at: remix.createdAt.toISOString(),
      updated_at: remix.updatedAt.toISOString(),
    }
  }

  static buildTree(remixes: RemixNode[], rootPostId: string): RemixTree {
    // Build tree structure for visualization
    const nodeMap = new Map<string, RemixNode>()
    remixes.forEach((r) => nodeMap.set(r.id, r))

    const buildNode = (nodeId: string, depth = 0): RemixTree => {
      const children: RemixTree[] = []

      // Find direct children
      for (const remix of remixes) {
        if (remix.parentRemixId === nodeId) {
          children.push(buildNode(remix.id, depth + 1))
        }
      }

      return {
        id: nodeId,
        nodeId: nodeId,
        children,
      }
    }

    // Start with remixes that have no parent (direct remixes of root post)
    const rootChildren: RemixTree[] = []
    for (const remix of remixes) {
      if (remix.parentPostId === rootPostId && !remix.parentRemixId) {
        rootChildren.push(buildNode(remix.id, 1))
      }
    }

    return {
      id: rootPostId,
      nodeId: rootPostId,
      children: rootChildren,
    }
  }
}
