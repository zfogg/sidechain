import { useParams, useNavigate } from 'react-router-dom'
import { useQuery } from '@tanstack/react-query'
import { FeedClient } from '@/api/FeedClient'
import { RemixesList } from '@/components/remixes/RemixesList'
import { RemixChain } from '@/components/remixes/RemixChain'
import { CreateRemixDialog } from '@/components/remixes/CreateRemixDialog'
import { Button } from '@/components/ui/button'
import { Spinner } from '@/components/ui/spinner'

/**
 * Remixes - Page showing all remixes of a post
 *
 * Features:
 * - View remix chain/lineage
 * - Browse all remixes with pagination
 * - Create new remix
 * - Click to view individual remixes
 */
export function Remixes() {
  const { postId } = useParams<{ postId: string }>()
  const navigate = useNavigate()

  if (!postId) {
    return (
      <div className="min-h-screen bg-gradient-to-br from-bg-primary via-bg-secondary to-bg-tertiary flex items-center justify-center">
        <div className="text-center">
          <p className="text-muted-foreground">No post specified</p>
          <Button onClick={() => navigate(-1)} variant="outline" className="mt-4">
            Go Back
          </Button>
        </div>
      </div>
    )
  }

  const { data: post, isLoading: postLoading } = useQuery({
    queryKey: ['post', postId],
    queryFn: async () => {
      const result = await FeedClient.getPost(postId)
      if (result.isError()) {
        throw new Error(result.getError())
      }
      return result.getValue()
    },
  })

  if (postLoading) {
    return (
      <div className="min-h-screen bg-gradient-to-br from-bg-primary via-bg-secondary to-bg-tertiary flex items-center justify-center">
        <Spinner size="lg" />
      </div>
    )
  }

  return (
    <div className="min-h-screen bg-gradient-to-br from-bg-primary via-bg-secondary to-bg-tertiary">
      {/* Header */}
      <div className="bg-card/50 border-b border-border">
        <div className="max-w-4xl mx-auto px-4 py-8">
          <div className="flex items-center justify-between mb-4">
            <Button
              onClick={() => navigate(-1)}
              variant="ghost"
              className="text-muted-foreground hover:text-foreground"
            >
              ← Back
            </Button>
          </div>

          <div>
            <h1 className="text-3xl font-bold text-foreground flex items-center gap-2 mb-2">
              <span>🎵</span> Remix Chain
            </h1>
            {post && (
              <p className="text-muted-foreground">
                Remixes of "{post.filename}" by @{post.username}
              </p>
            )}
          </div>
        </div>
      </div>

      {/* Content */}
      <div className="max-w-4xl mx-auto px-4 py-8">
        <div className="grid grid-cols-1 lg:grid-cols-3 gap-8">
          {/* Main Content */}
          <div className="lg:col-span-2">
            {/* Create Remix Button */}
            <div className="mb-8">
              <CreateRemixDialog
                postId={postId}
                trigger={
                  <Button className="w-full bg-coral-pink hover:bg-coral-pink/90 text-white text-lg py-6">
                    + Create Your Remix
                  </Button>
                }
              />
            </div>

            {/* Remixes List */}
            <RemixesList postId={postId} />
          </div>

          {/* Sidebar */}
          <div className="space-y-6">
            {/* Remix Chain Visualization */}
            <RemixChain postId={postId} showSourceLink={false} />

            {/* Original Post Info */}
            {post && (
              <div className="bg-card border border-border rounded-lg p-6">
                <h3 className="font-semibold text-foreground mb-3">Original Post</h3>
                <div>
                  <p className="font-medium text-foreground">{post.filename}</p>
                  <p className="text-sm text-muted-foreground">by @{post.username}</p>
                  <div className="flex items-center gap-2 mt-3 text-xs text-muted-foreground">
                    {post.bpm && <span>{post.bpm} BPM</span>}
                    {post.key && <span className="text-border">•</span>}
                    {post.key && <span>Key: {post.key}</span>}
                    {post.genre && post.genre.length > 0 && (
                      <>
                        <span className="text-border">•</span>
                        <span>{post.genre.join(', ')}</span>
                      </>
                    )}
                  </div>
                </div>
              </div>
            )}
          </div>
        </div>
      </div>
    </div>
  )
}
