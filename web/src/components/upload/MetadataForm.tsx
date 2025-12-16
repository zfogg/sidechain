import { Input } from '@/components/ui/input'
import { Label } from '@/components/ui/label'

interface MetadataFormProps {
  formData: {
    title: string
    description: string
    bpm: number | null
    key: string
    daw: string
    genre: string[]
    isPublic: boolean
  }
  onChange: (field: string, value: any) => void
  errors: Record<string, string>
}

const GENRES = [
  'Hip-Hop',
  'Trap',
  'Drill',
  'Grime',
  'Dubstep',
  'Garage',
  'Drum & Bass',
  'Jungle',
  'UK Bass',
  'Gqom',
  'Afrobeats',
  'Amapiano',
  'House',
  'Techno',
  'Deep House',
  'Minimal',
  'IDM',
  'Ambient',
  'Experimental',
  'Electronic',
  'Synthwave',
  'Vaporwave',
  'Indie',
  'Indie Rock',
  'Alternative',
  'Pop',
  'R&B',
  'Soul',
  'Jazz',
  'Funk',
  'Reggae',
  'Reggaeton',
  'Salsa',
  'Flamenco',
  'Classical',
  'Other',
]

const KEYS = [
  'C',
  'Cm',
  'C#',
  'C#m',
  'D',
  'Dm',
  'D#',
  'D#m',
  'E',
  'Em',
  'F',
  'Fm',
  'F#',
  'F#m',
  'G',
  'Gm',
  'G#',
  'G#m',
  'A',
  'Am',
  'A#',
  'A#m',
  'B',
  'Bm',
]

const DAWS = [
  'Ableton Live',
  'FL Studio',
  'Logic Pro',
  'Pro Tools',
  'Studio One',
  'Cubase',
  'Reaper',
  'Bitwig Studio',
  'Garageband',
  'Audacity',
  'Adobe Audition',
  'Other',
]

/**
 * MetadataForm - Form for loop metadata (title, BPM, key, DAW, genre)
 */
export function MetadataForm({ formData, onChange, errors }: MetadataFormProps) {
  return (
    <div className="bg-card border border-border/50 rounded-2xl p-8 space-y-6">
      {/* Title */}
      <div className="space-y-3">
        <Label htmlFor="title" className="text-sm font-semibold text-foreground">
          Loop Title *
        </Label>
        <Input
          id="title"
          value={formData.title}
          onChange={(e) => onChange('title', e.target.value)}
          placeholder="Give your loop a catchy name"
          className={`h-11 px-4 bg-bg-secondary/80 border-border/50 text-foreground text-sm ${
            errors.title ? 'border-red-500/50' : 'focus:border-coral-pink'
          }`}
        />
        {errors.title && <p className="text-red-400 text-xs">{errors.title}</p>}
      </div>

      {/* Description */}
      <div className="space-y-3">
        <Label htmlFor="description" className="text-sm font-semibold text-foreground">
          Description
        </Label>
        <textarea
          id="description"
          value={formData.description}
          onChange={(e) => onChange('description', e.target.value)}
          placeholder="Tell creators about your loop (optional)"
          rows={3}
          className="w-full px-4 py-2 rounded-lg bg-bg-secondary/80 border border-border/50 text-foreground text-sm placeholder:text-muted-foreground/50 focus:border-coral-pink focus:ring-1 focus:ring-coral-pink/50"
        />
      </div>

      {/* BPM and Key */}
      <div className="grid grid-cols-2 gap-4">
        <div className="space-y-3">
          <Label htmlFor="bpm" className="text-sm font-semibold text-foreground">
            BPM
          </Label>
          <Input
            id="bpm"
            type="number"
            min="20"
            max="300"
            value={formData.bpm || ''}
            onChange={(e) => onChange('bpm', e.target.value ? parseInt(e.target.value) : null)}
            placeholder="e.g., 140"
            className="h-11 px-4 bg-bg-secondary/80 border-border/50 text-foreground text-sm focus:border-coral-pink"
          />
        </div>

        <div className="space-y-3">
          <Label htmlFor="key" className="text-sm font-semibold text-foreground">
            Key
          </Label>
          <select
            id="key"
            value={formData.key}
            onChange={(e) => onChange('key', e.target.value)}
            className="h-11 px-4 rounded-lg bg-bg-secondary/80 border border-border/50 text-foreground text-sm focus:border-coral-pink focus:ring-1 focus:ring-coral-pink/50"
          >
            <option value="">Select key</option>
            {KEYS.map((k) => (
              <option key={k} value={k}>
                {k}
              </option>
            ))}
          </select>
        </div>
      </div>

      {/* DAW */}
      <div className="space-y-3">
        <Label htmlFor="daw" className="text-sm font-semibold text-foreground">
          DAW / Software
        </Label>
        <select
          id="daw"
          value={formData.daw}
          onChange={(e) => onChange('daw', e.target.value)}
          className="w-full h-11 px-4 rounded-lg bg-bg-secondary/80 border border-border/50 text-foreground text-sm focus:border-coral-pink focus:ring-1 focus:ring-coral-pink/50"
        >
          <option value="">Select software</option>
          {DAWS.map((d) => (
            <option key={d} value={d}>
              {d}
            </option>
          ))}
        </select>
      </div>

      {/* Genres */}
      <div className="space-y-3">
        <Label className="text-sm font-semibold text-foreground">
          Genres * ({formData.genre.length} selected)
        </Label>
        <div className="grid grid-cols-2 gap-2 max-h-48 overflow-y-auto p-3 rounded-lg bg-bg-secondary/50 border border-border/30">
          {GENRES.map((g) => (
            <label
              key={g}
              className="flex items-center gap-2 cursor-pointer hover:bg-bg-secondary/70 p-2 rounded transition-colors"
            >
              <input
                type="checkbox"
                checked={formData.genre.includes(g)}
                onChange={(e) => {
                  if (e.target.checked) {
                    onChange('genre', [...formData.genre, g])
                  } else {
                    onChange(
                      'genre',
                      formData.genre.filter((genre) => genre !== g)
                    )
                  }
                }}
                className="w-4 h-4 rounded cursor-pointer"
              />
              <span className="text-sm text-foreground">{g}</span>
            </label>
          ))}
        </div>
        {errors.genre && <p className="text-red-400 text-xs">{errors.genre}</p>}
      </div>

      {/* Visibility */}
      <div className="flex items-center gap-2 p-3 rounded-lg bg-bg-secondary/50 border border-border/30">
        <input
          id="isPublic"
          type="checkbox"
          checked={formData.isPublic}
          onChange={(e) => onChange('isPublic', e.target.checked)}
          className="w-4 h-4 rounded cursor-pointer"
        />
        <Label htmlFor="isPublic" className="text-sm font-semibold text-foreground cursor-pointer flex-1">
          Make this loop public
        </Label>
      </div>
    </div>
  )
}
