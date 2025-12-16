import { useQuery } from '@tanstack/react-query'
import { RemixClient } from '@/api/RemixClient'
import { RemixModel, RemixTree } from '@/models/Remix'
import { Spinner } from '@/components/ui/spinner'
import { Button } from '@/components/ui/button'
import { useNavigate } from 'react-router-dom'

interface RemixChainProps {
  postId: string
  showSourceLink?: boolean
}

/**
 * RemixChain - Display remix lineage/tree visualization
 *
 * Features:
 * - Tree visualization of remix hierarchy
 * - Click to navigate to individual remixes
 * - Source download links
 * - Creator attribution
 */
export function RemixChain({ postId }: RemixChainProps) {
  const navigate = useNavigate()

  const {
    data: chain,
    isLoading,
    error,
  } = useQuery({
    queryKey: ['remixChain', postId],
    queryFn: async () => {
      const result = await RemixClient.getRemixChain(postId)
      if (result.isError()) {
        throw new Error(result.getError())
      }
      return result.getValue()
    },
  })

  if (isLoading) {
    return (
      <div className="flex items-center justify-center py-8">
        <Spinner size="md" />
      </div>
    )
  }

  if (error || !chain || chain.totalRemixes === 0) {
    return null
  }

  const tree = RemixModel.buildTree(chain.remixNodes, chain.rootPostId)

  return (
    <div className="bg-card border border-border rounded-lg p-6">
      <h2 className="text-lg font-bold text-foreground mb-4 flex items-center gap-2">
        <span>🎵</span> Remix Chain
      </h2>

      {/* Stats */}
      <div className="grid grid-cols-2 gap-4 mb-6">
        <div>
          <p className="text-2xl font-bold text-coral-pink">{chain.totalRemixes}</p>
          <p className="text-xs text-muted-foreground">Total Remixes</p>
        </div>
        <div>
          <p className="text-2xl font-bold text-lavender">{chain.depth}</p>
          <p className="text-xs text-muted-foreground">Depth</p>
        </div>
      </div>

      {/* Tree */}
      <div className="space-y-4 max-h-96 overflow-y-auto">
        <RemixTreeNode tree={tree} remixes={chain.remixNodes} depth={0} />
      </div>

      {/* View All Link */}
      <Button
        onClick={() => navigate(`/posts/${postId}/remixes`)}
        variant="outline"
        className="w-full mt-4"
      >
        View All Remixes →
      </Button>
    </div>
  )
}

interface RemixTreeNodeProps {
  tree: RemixTree
  remixes: any[]
  depth: number
}

/**
 * RemixTreeNode - Individual node in the remix tree
 */
function RemixTreeNode({ tree, remixes, depth }: RemixTreeNodeProps) {
  const navigate = useNavigate()
  const remix = remixes.find((r) => r.id === tree.nodeId)

  if (!remix) return null

  const isRoot = depth === 0

  return (
    <div className={`ml-${Math.min(depth * 4, 12)}`}>
      {/* Node */}
      <button
        onClick={() => navigate(`/remixes/${tree.nodeId}`)}
        className={`w-full text-left p-3 rounded-lg border transition-all group ${
          isRoot
            ? 'bg-gradient-to-r from-coral-pink/10 to-rose-pink/10 border-coral-pink/20 hover:border-coral-pink/50'
            : 'bg-bg-secondary border-border hover:border-coral-pink/30'
        }`}
      >
        {/* Header */}
        <div className="flex items-start justify-between mb-2">
          <div className="flex-1 min-w-0">
            <p className="font-semibold text-foreground group-hover:text-coral-pink truncate">
              {remix.title}
            </p>
            <p className="text-xs text-muted-foreground">
              by @{remix.username}
              {isRoot && <span className="text-coral-pink ml-1">(Original)</span>}
            </p>
          </div>
          <div className="flex-shrink-0 text-right ml-2">
            <p className="text-sm font-semibold text-foreground">{remix.remixCount}</p>
            <p className="text-xs text-muted-foreground">remixes</p>
          </div>
        </div>

        {/* Metadata */}
        <div className="flex items-center gap-2 text-xs text-muted-foreground">
          {remix.bpm && <span>{remix.bpm} BPM</span>}
          {remix.key && <span>•</span>}
          {remix.key && <span>Key: {remix.key}</span>}
          {remix.genre && remix.genre.length > 0 && (
            <>
              <span>•</span>
              <span>{remix.genre[0]}</span>
            </>
          )}
        </div>
      </button>

      {/* Children */}
      {tree.children && tree.children.length > 0 && (
        <div className="mt-3 pl-4 border-l-2 border-border">
          {tree.children.map((child) => (
            <div key={child.nodeId} className="mb-3">
              <RemixTreeNode tree={child} remixes={remixes} depth={depth + 1} />
            </div>
          ))}
        </div>
      )}
    </div>
  )
}
