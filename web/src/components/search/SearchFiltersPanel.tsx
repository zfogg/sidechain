import { useState } from 'react'
import { useQuery } from '@tanstack/react-query'
import { SearchClient } from '@/api/SearchClient'
import { Button } from '@/components/ui/button'
import { Spinner } from '@/components/ui/spinner'

interface SearchFiltersPanelProps {
  selectedGenres: string[]
  onGenresChange: (genres: string[]) => void
  selectedKey: string
  onKeyChange: (key: string) => void
  selectedDAW: string
  onDAWChange: (daw: string) => void
  bpmRange: [number, number]
  onBpmRangeChange: (range: [number, number]) => void
}

/**
 * SearchFiltersPanel - Filter sidebar for search results
 */
export function SearchFiltersPanel({
  selectedGenres,
  onGenresChange,
  selectedKey,
  onKeyChange,
  selectedDAW,
  onDAWChange,
  bpmRange,
  onBpmRangeChange,
}: SearchFiltersPanelProps) {
  const [showAllGenres, setShowAllGenres] = useState(false)

  // Fetch filter options
  const { data: genres, isLoading: genresLoading } = useQuery({
    queryKey: ['search-genres'],
    queryFn: async () => {
      const result = await SearchClient.getGenres()
      return result.isOk() ? result.getValue() : []
    },
  })

  const { data: keys, isLoading: keysLoading } = useQuery({
    queryKey: ['search-keys'],
    queryFn: async () => {
      const result = await SearchClient.getKeys()
      return result.isOk() ? result.getValue() : []
    },
  })

  const { data: daws, isLoading: dawsLoading } = useQuery({
    queryKey: ['search-daws'],
    queryFn: async () => {
      const result = await SearchClient.getDAWs()
      return result.isOk() ? result.getValue() : []
    },
  })

  const handleGenreToggle = (genre: string) => {
    if (selectedGenres.includes(genre)) {
      onGenresChange(selectedGenres.filter((g) => g !== genre))
    } else {
      onGenresChange([...selectedGenres, genre])
    }
  }

  const displayedGenres = showAllGenres ? genres : genres?.slice(0, 6)

  return (
    <aside className="lg:col-span-1">
      <div className="sticky top-24 space-y-6">
        {/* Genres Filter */}
        <div className="bg-card border border-border rounded-lg p-4">
          <h3 className="font-semibold text-foreground mb-3">Genre</h3>
          {genresLoading ? (
            <div className="flex justify-center py-4">
              <Spinner size="sm" />
            </div>
          ) : (
            <div className="space-y-2">
              {displayedGenres?.map((genre) => (
                <label key={genre} className="flex items-center gap-2 cursor-pointer">
                  <input
                    type="checkbox"
                    checked={selectedGenres.includes(genre)}
                    onChange={() => handleGenreToggle(genre)}
                    className="w-4 h-4 rounded border border-border bg-bg-secondary text-coral-pink"
                  />
                  <span className="text-sm text-foreground">{genre}</span>
                </label>
              ))}
              {(genres?.length || 0) > 6 && (
                <Button
                  variant="ghost"
                  size="sm"
                  onClick={() => setShowAllGenres(!showAllGenres)}
                  className="w-full justify-start text-coral-pink text-xs"
                >
                  {showAllGenres ? 'Show less' : `Show ${(genres?.length || 0) - 6} more`}
                </Button>
              )}
            </div>
          )}
        </div>

        {/* BPM Range Filter */}
        <div className="bg-card border border-border rounded-lg p-4">
          <h3 className="font-semibold text-foreground mb-3">BPM Range</h3>
          <div className="space-y-4">
            <div>
              <label className="text-sm text-muted-foreground mb-2 block">
                Min: {bpmRange[0]} BPM
              </label>
              <input
                type="range"
                min="40"
                max="240"
                value={bpmRange[0]}
                onChange={(e) => {
                  const newMin = parseInt(e.target.value)
                  if (newMin <= bpmRange[1]) {
                    onBpmRangeChange([newMin, bpmRange[1]])
                  }
                }}
                className="w-full h-2 bg-bg-secondary rounded-lg appearance-none cursor-pointer"
              />
            </div>
            <div>
              <label className="text-sm text-muted-foreground mb-2 block">
                Max: {bpmRange[1]} BPM
              </label>
              <input
                type="range"
                min="40"
                max="240"
                value={bpmRange[1]}
                onChange={(e) => {
                  const newMax = parseInt(e.target.value)
                  if (newMax >= bpmRange[0]) {
                    onBpmRangeChange([bpmRange[0], newMax])
                  }
                }}
                className="w-full h-2 bg-bg-secondary rounded-lg appearance-none cursor-pointer"
              />
            </div>
          </div>
        </div>

        {/* Key Filter */}
        <div className="bg-card border border-border rounded-lg p-4">
          <h3 className="font-semibold text-foreground mb-3">Key</h3>
          {keysLoading ? (
            <div className="flex justify-center py-4">
              <Spinner size="sm" />
            </div>
          ) : (
            <select
              value={selectedKey}
              onChange={(e) => onKeyChange(e.target.value)}
              className="w-full px-3 py-2 bg-bg-secondary border border-border rounded text-foreground text-sm"
            >
              <option value="">All Keys</option>
              {keys?.map((key) => (
                <option key={key} value={key}>
                  {key}
                </option>
              ))}
            </select>
          )}
        </div>

        {/* DAW Filter */}
        <div className="bg-card border border-border rounded-lg p-4">
          <h3 className="font-semibold text-foreground mb-3">DAW</h3>
          {dawsLoading ? (
            <div className="flex justify-center py-4">
              <Spinner size="sm" />
            </div>
          ) : (
            <select
              value={selectedDAW}
              onChange={(e) => onDAWChange(e.target.value)}
              className="w-full px-3 py-2 bg-bg-secondary border border-border rounded text-foreground text-sm"
            >
              <option value="">All DAWs</option>
              {daws?.map((daw) => (
                <option key={daw} value={daw}>
                  {daw}
                </option>
              ))}
            </select>
          )}
        </div>

        {/* Clear Filters Button */}
        {(selectedGenres.length > 0 ||
          selectedKey ||
          selectedDAW ||
          bpmRange[0] !== 60 ||
          bpmRange[1] !== 180) && (
          <Button
            variant="outline"
            className="w-full"
            onClick={() => {
              onGenresChange([])
              onKeyChange('')
              onDAWChange('')
              onBpmRangeChange([60, 180])
            }}
          >
            Clear Filters
          </Button>
        )}
      </div>
    </aside>
  )
}
