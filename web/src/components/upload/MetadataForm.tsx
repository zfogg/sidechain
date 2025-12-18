import { Input } from '@/components/ui/input'
import { Label } from '@/components/ui/label'
import Select from 'react-select'

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
  onChange: (field: 'title' | 'description' | 'bpm' | 'key' | 'daw' | 'genre' | 'isPublic', value: string | number | string[] | boolean | null) => void
  errors: Record<string, string>
}

const GENRES = [
  // Hip-Hop & Rap
  'Hip-Hop',
  'Trap',
  'Drill',
  'Cloud Rap',
  'Crunk',
  'Glitch-Hop',
  'Mumble Rap',
  'Boom Bap',
  'Horrorcore',
  'Trap Soul',
  'Plugg',
  'Jersey Club',
  'Baltimore Club',
  'Juke',
  'Footwork',
  'Chopped and Screwed',

  // UK Garage & Grime
  'Grime',
  'UK Garage',
  'UK Bass',
  '2-Step',
  'Bassline',
  'Garage House',

  // Drum & Bass
  'Drum & Bass',
  'Liquid Drum & Bass',
  'Liquid Funk',
  'Neurofunk',
  'Jump-Up',
  'Techstep',
  'Darkstep',
  'Breakcore',

  // House
  'House',
  'Deep House',
  'Tech House',
  'Progressive House',
  'Garage House',
  'Circuit House',
  'Future House',
  'Tropical House',
  'UK Funky',
  'Acid House',
  'Acid Techno',

  // Techno & Industrial
  'Techno',
  'Detroit Techno',
  'Minimal Techno',
  'Melodic Techno',
  'Industrial',
  'Industrial Techno',
  'EBM',
  'Aggrotech',

  // Trance & Psychedelic
  'Trance',
  'Goa Trance',
  'Psytrance',
  'Hard Trance',
  'Progressive Trance',
  'Uplifting Trance',
  'Vocal Trance',

  // Dubstep & Bass
  'Dubstep',
  'Bass House',
  'Future Bass',
  'Tropical Bass',
  'Ghettotech',
  'Riddim',
  'Brostep',
  'Chillstep',
  'Deep Dubstep',

  // Breakbeat & Jungle
  'Breakbeat',
  'Jungle',
  'Liquid Jungle',
  'Darkside Jungle',
  'Hardcore Punk',
  'Happy Hardcore',

  // Experimental & Avant-Garde
  'IDM',
  'Glitch',
  'Glitch Hop',
  'Experimental',
  'Lowercase',
  'Microsound',
  'Granular',
  'Noise',
  'Power Noise',
  'Noisegrind',
  'Merzbow-inspired',

  // Vaporwave & Internet Genres
  'Vaporwave',
  'Seapunk',
  'Vaporhop',
  'Cottagecore Pop',
  'Hyperpop',
  'PC Music',
  'Chopped and Screwed',

  // Synthwave & Retro
  'Synthwave',
  'Retrowave',
  'Chillwave',
  'Darkwave',
  'Synthpop',
  'Electroclash',
  'Witch House',

  // Ambient & Atmospheric
  'Ambient',
  'Dark Ambient',
  'Drone',
  'Field Recording',
  'Soundscape',
  'Musique Concrète',
  'Ambient Techno',
  'Epicore',

  // Downtempo & Chill
  'Downtempo',
  'Trip-Hop',
  'Balearic Beat',
  'Nu-Disco',
  'Disco House',
  'Tropical Downtempo',
  'Lo-Fi',
  'Lo-Fi Hip-Hop',
  'Chillhop',

  // International & World
  'Amapiano',
  'Afro House',
  'Afrobeats',
  'Gqom',
  'Baile Funk',
  'Reggaeton',
  'Kuduro',
  'Soca',
  'Bhangra',
  'UK Funky',
  'Moombahton',
  'Dancehall',
  'Riddim',

  // Electronic & Synth
  'Electronic',
  'Synth-Punk',
  'Synth-Pop',
  'EBM',
  'Electro',
  'Electronica',
  'Electropop',

  // Rock & Alternative
  'Indie',
  'Indie Rock',
  'Alternative Rock',
  'Post-Punk',
  'Post-Punk Revival',
  'Shoegaze',
  'Dream Pop',
  'Alternative Pop',
  'Indie Pop',
  'Emo',
  'Math Rock',
  'Progressive Rock',
  'Psychedelic Rock',
  'Krautrock',

  // Pop & Mainstream
  'Pop',
  'Synth-Pop',
  'Pop Punk',
  'Dance-Pop',
  'Electronic Pop',
  'Hyperpop',

  // Soul & Funk
  'Soul',
  'R&B',
  'Trap Soul',
  'Funk',
  'Afro-Funk',
  'Nu-Funk',

  // Jazz & Classical
  'Jazz',
  'Free Jazz',
  'Avant-Garde Jazz',
  'Fusion',
  'Jazz Funk',
  'Classical',
  'Contemporary Classical',
  'Minimalism',
  'Glitch Classical',

  // Reggae & Caribbean
  'Reggae',
  'Roots Reggae',
  'Dancehall',
  'Dub',
  'Reggaeton',
  'Soca',
  'Calypso',
  'Ska',

  // Latin & Hispanic
  'Reggaeton',
  'Salsa',
  'Flamenco',
  'Bachata',
  'Cumbia',
  'Merengue',
  'Baile Funk',
  'Corridos',

  // Obscure & Experimental
  'Witch House',
  'Vaporwave',
  'Seapunk',
  'Lowercase',
  'Phonk',
  'Diva House',
  'Kawaii Metal',
  'Extratone',
  'Speedcore',
  'Hardcore Techno',
  'Dark Jazz',
  'Apocalyptic Jazz',
  'Dungeon Synth',
  'Dark Wave',

  // Metal & Punk
  'Metal',
  'Death Metal',
  'Black Metal',
  'Grindcore',
  'Metalcore',
  'Deathcore',
  'Punk',
  'Garage Punk',

  // Miscellaneous
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

// Custom styles for react-select to match dark theme
const selectStyles = {
  control: (base: any, state: any) => ({
    ...base,
    backgroundColor: '#26262C',
    borderColor: '#2E2E34',
    borderRadius: '0.5rem',
    minHeight: '44px',
    fontSize: '14px',
    cursor: 'pointer',
    boxShadow: state.isFocused ? '0 0 0 1px #FD7792' : 'none',
    border: state.isFocused ? '1px solid #FD7792' : '1px solid #2E2E34',
    '&:hover': {
      borderColor: '#363640',
    },
  }),
  input: (base: any) => ({
    ...base,
    color: '#FFFFFF',
  }),
  placeholder: (base: any) => ({
    ...base,
    color: '#a0a0a8',
  }),
  singleValue: (base: any) => ({
    ...base,
    color: '#FFFFFF',
  }),
  menu: (base: any) => ({
    ...base,
    backgroundColor: '#1C1C20',
    borderRadius: '0.5rem',
    boxShadow: '0 4px 12px rgba(0, 0, 0, 0.4)',
  }),
  menuList: (base: any) => ({
    ...base,
    backgroundColor: '#1C1C20',
    scrollBehavior: 'smooth',
    '::-webkit-scrollbar': {
      width: '8px',
    },
    '::-webkit-scrollbar-track': {
      background: 'transparent',
    },
    '::-webkit-scrollbar-thumb': {
      background: '#363640',
      borderRadius: '4px',
    },
  }),
  option: (base: any, state: any) => ({
    ...base,
    backgroundColor: state.isSelected
      ? '#FD7792'
      : state.isFocused
        ? '#363640'
        : '#1C1C20',
    color: state.isSelected ? '#FFFFFF' : '#FFFFFF',
    cursor: 'pointer',
    padding: '10px 12px',
    fontSize: '14px',
    '&:active': {
      backgroundColor: '#FD7792',
    },
  }),
  multiValue: (base: any) => ({
    ...base,
    backgroundColor: '#FD7792',
    borderRadius: '0.375rem',
  }),
  multiValueLabel: (base: any) => ({
    ...base,
    color: '#FFFFFF',
    fontWeight: '500',
  }),
  multiValueRemove: (base: any) => ({
    ...base,
    color: '#FFFFFF',
    cursor: 'pointer',
    '&:hover': {
      backgroundColor: '#E95A7D',
      color: '#FFFFFF',
    },
  }),
}

/**
 * MetadataForm - Form for loop metadata (title, BPM, key, DAW, genre)
 */
export function MetadataForm({ formData, onChange, errors }: MetadataFormProps) {
  // Convert genre strings to react-select options
  const genreOptions = GENRES.map((g) => ({ value: g, label: g }))
  const selectedGenres = formData.genre.map((g) => ({ value: g, label: g }))
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

      {/* BPM */}
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

      {/* Key */}
      <div className="space-y-3">
        <Label htmlFor="key" className="text-sm font-semibold text-foreground">
          Key
        </Label>
        <select
          id="key"
          value={formData.key}
          onChange={(e) => onChange('key', e.target.value)}
          className="w-full h-11 px-4 rounded-lg bg-bg-secondary/80 border border-border/50 text-foreground text-sm focus:border-coral-pink focus:ring-1 focus:ring-coral-pink/50"
        >
          <option value="">Select key</option>
          {KEYS.map((k) => (
            <option key={k} value={k}>
              {k}
            </option>
          ))}
        </select>
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

      {/* Genres - Searchable Autocomplete */}
      <div className="space-y-3">
        <Label className="text-sm font-semibold text-foreground">
          Genres * ({formData.genre.length} selected)
        </Label>
        <Select
          isMulti
          options={genreOptions}
          value={selectedGenres}
          onChange={(selected) => {
            onChange('genre', selected ? selected.map((s) => s.value) : [])
          }}
          placeholder="Search and select genres..."
          styles={selectStyles}
          className="text-sm"
          classNamePrefix="react-select"
          isSearchable
          isClearable={false}
          formatOptionLabel={(option: any) => <div className="text-sm">{option.label}</div>}
        />
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
