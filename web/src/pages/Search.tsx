import { useState } from 'react'
import { useQuery } from '@tanstack/react-query'
import { SearchClient, SearchFilters as SearchFiltersType } from '@/api/SearchClient'
import { Button } from '@/components/ui/button'
import { Input } from '@/components/ui/input'
import { Spinner } from '@/components/ui/spinner'
import { SearchFilters } from '@/components/search/SearchFilters'
import { SearchUserCard } from '@/components/search/SearchUserCard'
import { FeedPost } from '@/models/FeedPost'
import type { User } from '@/models/User'

/**
 * Search - Advanced search and discovery page
 *
 * Features:
 * - Full-text search
 * - Advanced filters (BPM, key, genre, DAW)
 * - Filter presets
 * - User search
 * - Result pagination
 */
export function Search() {
  const [query, setQuery] = useState('')
  const [filters, setFilters] = useState<SearchFiltersType>({ limit: 20, offset: 0 })
  const [hasSearched, setHasSearched] = useState(false)
  const [offset, setOffset] = useState(0)

  // Posts search query
  const {
    data: results,
    isLoading,
    error,
    refetch,
  } = useQuery({
    queryKey: ['search', { ...filters, offset }],
    queryFn: async () => {
      if (!query && !hasSearched && !Object.keys(filters).some((k) => filters[k as keyof SearchFiltersType])) {
        return null
      }

      const result = await SearchClient.searchPosts({
        ...filters,
        query: query || undefined,
        offset,
      })

      if (result.isError()) {
        throw new Error(result.getError())
      }

      return result.getValue()
    },
    enabled: hasSearched || query.length > 0,
  })

  // Users search query - only search users when there's a text query
  const {
    data: userResults,
    isLoading: usersLoading,
  } = useQuery({
    queryKey: ['searchUsers', query],
    queryFn: async () => {
      if (!query) return null

      const result = await SearchClient.searchUsers(query, 5)
      if (result.isError()) {
        return null
      }
      return result.getValue()
    },
    enabled: query.length > 0,
  })

  const handleSearch = () => {
    setOffset(0)
    setHasSearched(true)
    refetch()
  }

  const handleFiltersChange = (newFilters: SearchFiltersType) => {
    setFilters(newFilters)
    setOffset(0)
  }

  const handleKeyPress = (e: React.KeyboardEvent<HTMLInputElement>) => {
    if (e.key === 'Enter') {
      handleSearch()
    }
  }

  return (
    <div className="min-h-screen bg-gradient-to-br from-bg-primary via-bg-secondary to-bg-tertiary p-4">
      <div className="max-w-6xl mx-auto">
        {/* Header */}
        <div className="mb-8">
          <h1 className="text-4xl font-bold text-foreground mb-2">Discover Music & People</h1>
          <p className="text-muted-foreground">Search loops by BPM, key, genre, or find other producers</p>
        </div>

        <div className="grid grid-cols-1 lg:grid-cols-4 gap-6">
          {/* Sidebar Filters */}
          <div className="lg:col-span-1">
            <div className="sticky top-4">
              <div className="space-y-4">
                {/* Search input */}
                <div>
                  <Input
                    value={query}
                    onChange={(e) => setQuery(e.target.value)}
                    onKeyPress={handleKeyPress}
                    placeholder="Search loops or people..."
                    className="h-11"
                  />
                </div>

                {/* Filters */}
                <FilterComponent
                  filters={filters}
                  onFiltersChange={handleFiltersChange}
                  onSearch={handleSearch}
                />
              </div>
            </div>
          </div>

          {/* Results */}
          <div className="lg:col-span-3">
            {(isLoading || usersLoading) ? (
              <div className="flex items-center justify-center py-12">
                <Spinner size="lg" />
              </div>
            ) : error ? (
              <div className="text-center py-8 text-muted-foreground">
                <p>Failed to search</p>
                <p className="text-sm mt-2">{error.message}</p>
              </div>
            ) : !hasSearched && !query ? (
              <div className="text-center py-12 text-muted-foreground">
                <p className="text-lg mb-2">🔍</p>
                <p>Use the filters to discover amazing loops</p>
                <p className="text-sm mt-2">Or search by keyword, producer name, loop title, or find other producers</p>
              </div>
            ) : (userResults && userResults.length > 0) || (results && results.posts.length > 0) ? (
              <div className="space-y-6">
                {/* Users Section */}
                {userResults && userResults.length > 0 && (
                  <div>
                    <h3 className="text-lg font-semibold text-foreground mb-3">People</h3>
                    <div className="grid grid-cols-1 gap-3">
                      {userResults.map((user: User) => (
                        <SearchUserCard key={user.id} user={user} />
                      ))}
                    </div>
                  </div>
                )}

                {/* Posts Section */}
                {results && results.posts.length > 0 && (
                  <div>
                    <h3 className="text-lg font-semibold text-foreground mb-3">
                      Loops ({results.total} found)
                    </h3>

                    {/* Search results */}
                    <div className="space-y-4">
                      {results.posts.map((post: FeedPost) => (
                        <SearchResultCard key={post.id} post={post} />
                      ))}
                    </div>

                    {/* Pagination */}
                    {results.posts.length >= (filters.limit || 20) && (
                      <div className="flex gap-2 justify-center pt-6">
                        <Button
                          onClick={() => setOffset(Math.max(0, offset - (filters.limit || 20)))}
                          disabled={offset === 0}
                          variant="outline"
                        >
                          Previous
                        </Button>
                        <Button
                          onClick={() => setOffset(offset + (filters.limit || 20))}
                          variant="outline"
                        >
                          Next
                        </Button>
                      </div>
                    )}
                  </div>
                )}
              </div>
            ) : (
              <div className="text-center py-8 text-muted-foreground">
                <p>No results found</p>
                <p className="text-sm mt-2">Try adjusting your filters or search terms</p>
              </div>
            )}
          </div>
        </div>
      </div>
    </div>
  )
}

/**
 * SearchResultCard - Individual search result
 */
function SearchResultCard({ post }: { post: FeedPost }) {
  return (
    <div className="bg-card border border-border rounded-lg p-4 hover:border-coral-pink/50 transition-colors">
      <div className="flex gap-4">
        {/* Thumbnail / Waveform */}
        {post.waveformUrl && (
          <img src={post.waveformUrl} alt={post.filename} className="w-20 h-20 rounded object-cover" />
        )}

        {/* Content */}
        <div className="flex-1 min-w-0">
          <div className="flex items-start justify-between gap-2 mb-2">
            <div>
              <h3 className="font-semibold text-foreground truncate">{post.filename}</h3>
              <p className="text-sm text-muted-foreground">
                by <span className="text-foreground">@{post.username}</span>
              </p>
            </div>
          </div>

          {/* Metadata */}
          <div className="flex flex-wrap gap-3 text-xs text-muted-foreground mb-3">
            {post.bpm && (
              <span>
                <span className="font-medium">{post.bpm}</span> BPM
              </span>
            )}
            {post.key && <span>🎵 {post.key}</span>}
            {post.daw && <span>🎹 {post.daw}</span>}
            {post.genre && post.genre.length > 0 && (
              <span>{post.genre.slice(0, 2).join(', ')}</span>
            )}
          </div>

          {/* Stats */}
          <div className="flex gap-4 text-xs text-muted-foreground">
            <span>❤️ {post.likeCount} likes</span>
            <span>💬 {post.commentCount} comments</span>
            <span>🎧 {post.playCount} plays</span>
          </div>
        </div>

        {/* Play button */}
        {post.audioUrl && (
          <button className="flex-shrink-0 w-10 h-10 rounded-full bg-coral-pink hover:bg-coral-pink/90 flex items-center justify-center text-white transition-colors">
            ▶
          </button>
        )}
      </div>
    </div>
  )
}
