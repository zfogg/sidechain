import { useState } from 'react'
import { useQuery } from '@tanstack/react-query'
import { SearchClient, SearchFilters as SearchFiltersType } from '@/api/SearchClient'
import { Button } from '@/components/ui/button'

interface SearchFiltersProps {
  filters: SearchFiltersType
  onFiltersChange: (filters: SearchFiltersType) => void
  onSearch?: () => void
}

const BPM_PRESETS = [
  { label: 'House (120-130)', min: 120, max: 130 },
  { label: 'Techno (120-150)', min: 120, max: 150 },
  { label: 'Hip-Hop (85-115)', min: 85, max: 115 },
  { label: 'Electronic (100-140)', min: 100, max: 140 },
  { label: 'Drum & Bass (160-180)', min: 160, max: 180 },
  { label: 'All BPMs', min: 40, max: 200 },
]

/**
 * SearchFilters - Advanced filter controls for post discovery
 *
 * Features:
 * - BPM range slider with presets
 * - Key selection
 * - Genre multi-select
 * - DAW filter
 * - Dynamic filter options from API
 */
export function SearchFilters({ filters, onFiltersChange, onSearch }: SearchFiltersProps) {
  const [showAdvanced, setShowAdvanced] = useState(false)
  const [selectedGenres, setSelectedGenres] = useState<string[]>(filters.genres || [])

  // Fetch available options
  const { data: genres = [] } = useQuery({
    queryKey: ['searchGenres'],
    queryFn: async () => {
      const result = await SearchClient.getGenres()
      return result.isOk() ? result.getValue() : []
    },
  })

  const { data: keys = [] } = useQuery({
    queryKey: ['searchKeys'],
    queryFn: async () => {
      const result = await SearchClient.getKeys()
      return result.isOk() ? result.getValue() : []
    },
  })

  const { data: daws = [] } = useQuery({
    queryKey: ['searchDAWs'],
    queryFn: async () => {
      const result = await SearchClient.getDAWs()
      return result.isOk() ? result.getValue() : []
    },
  })

  const handleGenreToggle = (genre: string) => {
    const newGenres = selectedGenres.includes(genre)
      ? selectedGenres.filter((g) => g !== genre)
      : [...selectedGenres, genre]
    setSelectedGenres(newGenres)
    onFiltersChange({ ...filters, genres: newGenres })
  }

  const handleBPMPreset = (min: number, max: number) => {
    onFiltersChange({ ...filters, bpmMin: min, bpmMax: max })
  }

  const handleClearFilters = () => {
    setSelectedGenres([])
    onFiltersChange({
      query: filters.query,
      limit: filters.limit,
      offset: filters.offset,
    })
  }

  const hasActiveFilters =
    filters.bpmMin ||
    filters.bpmMax ||
    filters.key ||
    (filters.genres && filters.genres.length > 0) ||
    filters.daw

  return (
    <div className="space-y-4">
      {/* BPM Filters */}
      <div className="bg-card border border-border rounded-lg p-4">
        <h3 className="font-semibold text-foreground mb-3">BPM Range</h3>

        {/* BPM Preset Buttons */}
        <div className="grid grid-cols-2 gap-2 mb-4">
          {BPM_PRESETS.map((preset) => (
            <Button
              key={preset.label}
              onClick={() => handleBPMPreset(preset.min, preset.max)}
              variant={
                filters.bpmMin === preset.min && filters.bpmMax === preset.max
                  ? 'default'
                  : 'outline'
              }
              size="sm"
              className="text-xs"
            >
              {preset.label}
            </Button>
          ))}
        </div>

        {/* Custom BPM Range */}
        <div className="space-y-3">
          <div>
            <label className="text-xs text-muted-foreground">Min BPM: {filters.bpmMin || 40}</label>
            <input
              type="range"
              min="40"
              max="200"
              value={filters.bpmMin || 40}
              onChange={(e) =>
                onFiltersChange({
                  ...filters,
                  bpmMin: parseInt(e.target.value),
                })
              }
              className="w-full"
            />
          </div>

          <div>
            <label className="text-xs text-muted-foreground">Max BPM: {filters.bpmMax || 200}</label>
            <input
              type="range"
              min="40"
              max="200"
              value={filters.bpmMax || 200}
              onChange={(e) =>
                onFiltersChange({
                  ...filters,
                  bpmMax: parseInt(e.target.value),
                })
              }
              className="w-full"
            />
          </div>
        </div>
      </div>

      {/* Show Advanced Filters */}
      <button
        onClick={() => setShowAdvanced(!showAdvanced)}
        className="text-sm text-coral-pink hover:text-rose-pink transition-colors"
      >
        {showAdvanced ? '▼ Hide Advanced Filters' : '▶ Show Advanced Filters'}
      </button>

      {showAdvanced && (
        <>
          {/* Key Filter */}
          {keys.length > 0 && (
            <div className="bg-card border border-border rounded-lg p-4">
              <h3 className="font-semibold text-foreground mb-3">Musical Key</h3>
              <div className="grid grid-cols-4 gap-2">
                {keys.map((key) => (
                  <Button
                    key={key}
                    onClick={() => onFiltersChange({ ...filters, key: filters.key === key ? undefined : key })}
                    variant={filters.key === key ? 'default' : 'outline'}
                    size="sm"
                    className="text-xs"
                  >
                    {key}
                  </Button>
                ))}
              </div>
            </div>
          )}

          {/* Genre Filter */}
          {genres.length > 0 && (
            <div className="bg-card border border-border rounded-lg p-4">
              <h3 className="font-semibold text-foreground mb-3">Genres</h3>
              <div className="grid grid-cols-2 gap-2">
                {genres.map((genre) => (
                  <label
                    key={genre}
                    className="flex items-center gap-2 cursor-pointer p-2 hover:bg-bg-secondary rounded"
                  >
                    <input
                      type="checkbox"
                      checked={selectedGenres.includes(genre)}
                      onChange={() => handleGenreToggle(genre)}
                      className="w-4 h-4 rounded"
                    />
                    <span className="text-sm text-foreground">{genre}</span>
                  </label>
                ))}
              </div>
            </div>
          )}

          {/* DAW Filter */}
          {daws.length > 0 && (
            <div className="bg-card border border-border rounded-lg p-4">
              <h3 className="font-semibold text-foreground mb-3">DAW</h3>
              <div className="space-y-2">
                {daws.map((daw) => (
                  <label key={daw} className="flex items-center gap-2 cursor-pointer p-2 hover:bg-bg-secondary rounded">
                    <input
                      type="radio"
                      name="daw"
                      checked={filters.daw === daw}
                      onChange={() => onFiltersChange({ ...filters, daw })}
                      className="w-4 h-4"
                    />
                    <span className="text-sm text-foreground">{daw}</span>
                  </label>
                ))}
              </div>
            </div>
          )}
        </>
      )}

      {/* Clear and Search Buttons */}
      <div className="flex gap-2">
        {hasActiveFilters && (
          <Button
            onClick={handleClearFilters}
            variant="outline"
            className="flex-1"
            size="sm"
          >
            Clear Filters
          </Button>
        )}
        {onSearch && (
          <Button
            onClick={onSearch}
            className="flex-1 bg-coral-pink hover:bg-coral-pink/90 text-white"
            size="sm"
          >
            Search
          </Button>
        )}
      </div>
    </div>
  )
}
