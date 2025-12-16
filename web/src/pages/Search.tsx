import { useState, useEffect } from 'react'
import { useSearchParams } from 'react-router-dom'
import { useQuery } from '@tanstack/react-query'
import { SearchClient, SearchFilters, SearchResults } from '@/api/SearchClient'
import { PostCard } from '@/components/feed/PostCard'
import { SearchUserCard } from '@/components/search/SearchUserCard'
import { SearchFiltersPanel } from '@/components/search/SearchFiltersPanel'
import { Input } from '@/components/ui/input'
import { Button } from '@/components/ui/button'
import { Spinner } from '@/components/ui/spinner'

/**
 * Search - Global search page with filtering
 *
 * Features:
 * - Full-text search across posts and users
 * - Filter by genre, BPM, key, DAW
 * - Browse all available genres and keys
 * - User search with follower counts
 * - Post search with metadata
 * - Real-time search results
 */
export function Search() {
  const [searchParams, setSearchParams] = useSearchParams()
  const [query, setQuery] = useState(searchParams.get('q') || '')
  const [selectedGenres, setSelectedGenres] = useState<string[]>(
    searchParams.get('genres')?.split(',').filter(Boolean) || []
  )
  const [selectedKey, setSelectedKey] = useState(searchParams.get('key') || '')
  const [selectedDAW, setSelectedDAW] = useState(searchParams.get('daw') || '')
  const [bpmRange, setBpmRange] = useState<[number, number]>([
    parseInt(searchParams.get('bpm_min') || '60'),
    parseInt(searchParams.get('bpm_max') || '180'),
  ])
  const [searchType, setSearchType] = useState<'all' | 'posts' | 'users'>(
    (searchParams.get('type') as any) || 'all'
  )

  // Update URL when filters change
  useEffect(() => {
    const params = new URLSearchParams()
    if (query) params.set('q', query)
    if (selectedGenres.length) params.set('genres', selectedGenres.join(','))
    if (selectedKey) params.set('key', selectedKey)
    if (selectedDAW) params.set('daw', selectedDAW)
    if (bpmRange[0] !== 60) params.set('bpm_min', String(bpmRange[0]))
    if (bpmRange[1] !== 180) params.set('bpm_max', String(bpmRange[1]))
    if (searchType !== 'all') params.set('type', searchType)

    setSearchParams(params)
  }, [query, selectedGenres, selectedKey, selectedDAW, bpmRange, searchType, setSearchParams])

  // Build search filters
  const filters: SearchFilters = {
    query: query || undefined,
    genre: selectedGenres.length > 0 ? selectedGenres : undefined,
    key: selectedKey || undefined,
    daw: selectedDAW || undefined,
    bpm: bpmRange ? { min: bpmRange[0], max: bpmRange[1] } : undefined,
    limit: 50,
  }

  // Fetch search results
  const { data: results, isLoading, error } = useQuery<SearchResults>({
    queryKey: ['search', filters, searchType],
    queryFn: async () => {
      if (searchType === 'posts' || searchType === 'all') {
        const result = await SearchClient.search(filters)
        if (result.isError()) {
          throw new Error(result.getError())
        }
        return result.getValue()
      }
      return { posts: [], users: [], total: 0, took: 0 }
    },
    staleTime: 30000, // 30 seconds
    enabled: query.length > 0 || selectedGenres.length > 0,
  })

  const posts = results?.posts || []
  const users = results?.users || []
  const totalResults = results?.total || 0

  return (
    <div className="min-h-screen bg-gradient-to-br from-bg-primary via-bg-secondary to-bg-tertiary">
      {/* Search Header */}
      <div className="sticky top-0 z-40 bg-card/95 backdrop-blur border-b border-border">
        <div className="max-w-6xl mx-auto px-4 py-6">
          <h1 className="text-3xl font-bold text-foreground mb-6">Search</h1>

          {/* Search Input */}
          <div className="flex gap-2 mb-4">
            <div className="flex-1 relative">
              <Input
                type="text"
                placeholder="Search posts, users, sounds... (min 2 characters)"
                value={query}
                onChange={(e) => setQuery(e.target.value)}
                className="h-11 text-base"
              />
            </div>
            <Button variant="default" disabled={query.length < 2}>
              Search
            </Button>
          </div>

          {/* Quick Filter Tabs */}
          {query && (
            <div className="flex gap-2 items-center">
              <span className="text-sm text-muted-foreground">Show:</span>
              <div className="flex gap-1">
                <Button
                  variant={searchType === 'all' ? 'default' : 'outline'}
                  onClick={() => setSearchType('all')}
                  size="sm"
                >
                  All
                </Button>
                <Button
                  variant={searchType === 'posts' ? 'default' : 'outline'}
                  onClick={() => setSearchType('posts')}
                  size="sm"
                >
                  Posts
                </Button>
                <Button
                  variant={searchType === 'users' ? 'default' : 'outline'}
                  onClick={() => setSearchType('users')}
                  size="sm"
                >
                  Users
                </Button>
              </div>
            </div>
          )}
        </div>
      </div>

      {/* Main Content */}
      <main className="max-w-6xl mx-auto px-4 py-8">
        <div className="grid grid-cols-1 lg:grid-cols-4 gap-6">
          {/* Filters Sidebar */}
          <SearchFiltersPanel
            selectedGenres={selectedGenres}
            onGenresChange={setSelectedGenres}
            selectedKey={selectedKey}
            onKeyChange={setSelectedKey}
            selectedDAW={selectedDAW}
            onDAWChange={setSelectedDAW}
            bpmRange={bpmRange}
            onBpmRangeChange={setBpmRange}
          />

          {/* Results */}
          <div className="lg:col-span-3">
            {query.length < 2 ? (
              <div className="bg-card border border-border rounded-lg p-12 text-center space-y-4">
                <div className="text-4xl">🔍</div>
                <div className="text-lg font-semibold text-foreground">
                  Start searching to find posts and users
                </div>
                <p className="text-muted-foreground max-w-md mx-auto">
                  Use the search box above to find posts by title or description, discover users,
                  and filter by genre, BPM, key, and DAW
                </p>
              </div>
            ) : isLoading ? (
              <div className="flex justify-center py-12">
                <Spinner size="lg" />
              </div>
            ) : error ? (
              <div className="bg-card border border-border rounded-lg p-6 text-center space-y-4">
                <div className="text-destructive font-medium">Search failed</div>
                <p className="text-muted-foreground">{error instanceof Error ? error.message : 'Unknown error'}</p>
              </div>
            ) : totalResults === 0 ? (
              <div className="bg-card border border-border rounded-lg p-12 text-center space-y-4">
                <div className="text-4xl">😕</div>
                <div className="text-lg font-semibold text-foreground">No results found</div>
                <p className="text-muted-foreground max-w-md mx-auto">
                  Try adjusting your search query or filters to find what you're looking for
                </p>
              </div>
            ) : (
              <div className="space-y-4">
                {/* Results Summary */}
                <div className="text-sm text-muted-foreground mb-4">
                  Found {totalResults} result{totalResults !== 1 ? 's' : ''} in {results?.took}ms
                </div>

                {/* Posts Results */}
                {(searchType === 'all' || searchType === 'posts') && posts.length > 0 && (
                  <div className="space-y-4">
                    <h3 className="text-lg font-semibold text-foreground">Posts</h3>
                    {posts.map((post) => (
                      <PostCard key={post.id} post={post} />
                    ))}
                  </div>
                )}

                {/* Users Results */}
                {(searchType === 'all' || searchType === 'users') && users.length > 0 && (
                  <div className="space-y-4">
                    <h3 className="text-lg font-semibold text-foreground">Users</h3>
                    <div className="grid grid-cols-1 sm:grid-cols-2 gap-4">
                      {users.map((user) => (
                        <SearchUserCard key={user.id} user={user} />
                      ))}
                    </div>
                  </div>
                )}
              </div>
            )}
          </div>
        </div>
      </main>
    </div>
  )
}
