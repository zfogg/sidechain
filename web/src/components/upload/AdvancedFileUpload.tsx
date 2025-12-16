import { useState } from 'react'
import { Button } from '@/components/ui/button'
import {
  Tabs,
  TabsContent,
  TabsList,
  TabsTrigger,
} from '@/components/ui/tabs'

interface AdvancedFileUploadProps {
  onMidiSelect?: (file: File) => void
  onProjectSelect?: (file: File) => void
  selectedMidiFile?: File | null
  selectedProjectFile?: File | null
}

/**
 * AdvancedFileUpload - Upload MIDI and project files
 *
 * Features:
 * - MIDI file upload (analyze metadata)
 * - Project file upload (.als, .flp, .logic)
 * - File preview and metadata display
 */
export function AdvancedFileUpload({
  onMidiSelect,
  onProjectSelect,
  selectedMidiFile,
  selectedProjectFile,
}: AdvancedFileUploadProps) {
  const [midiError, setMidiError] = useState('')
  const [projectError, setProjectError] = useState('')

  const handleMidiUpload = (file: File) => {
    // Validate MIDI file
    const validTypes = ['audio/midi', 'audio/x-midi']
    if (!validTypes.includes(file.type) && !file.name.endsWith('.mid') && !file.name.endsWith('.midi')) {
      setMidiError('Invalid file type. Please upload a MIDI file (.mid or .midi)')
      return
    }

    if (file.size > 10 * 1024 * 1024) {
      setMidiError('File is too large. Max size is 10MB.')
      return
    }

    setMidiError('')
    onMidiSelect?.(file)
  }

  const handleProjectUpload = (file: File) => {
    // Validate project file
    const validExtensions = ['.als', '.flp', '.logic', '.logicx', '.sesx', '.cpr', '.rrwx']
    const fileName = file.name.toLowerCase()
    const hasValidExtension = validExtensions.some((ext) => fileName.endsWith(ext))

    if (!hasValidExtension) {
      setProjectError(
        'Invalid file type. Supported: Ableton (.als), FL Studio (.flp), Logic (.logic/.logicx), Studio One (.sesx), Cubase (.cpr), Reaper (.rrwx)'
      )
      return
    }

    if (file.size > 500 * 1024 * 1024) {
      setProjectError('File is too large. Max size is 500MB.')
      return
    }

    setProjectError('')
    onProjectSelect?.(file)
  }

  const handleMidiDrop = (e: React.DragEvent<HTMLDivElement>) => {
    e.preventDefault()
    const files = e.dataTransfer.files
    if (files.length > 0) {
      handleMidiUpload(files[0])
    }
  }

  const handleProjectDrop = (e: React.DragEvent<HTMLDivElement>) => {
    e.preventDefault()
    const files = e.dataTransfer.files
    if (files.length > 0) {
      handleProjectUpload(files[0])
    }
  }

  return (
    <div className="space-y-4">
      <div className="bg-bg-secondary/50 border border-border rounded-lg p-4">
        <h3 className="font-semibold text-foreground mb-3">Additional Files (Optional)</h3>

        <Tabs defaultValue="midi" className="w-full">
          <TabsList className="grid w-full grid-cols-2">
            <TabsTrigger value="midi">🎹 MIDI Data</TabsTrigger>
            <TabsTrigger value="project">📁 Project File</TabsTrigger>
          </TabsList>

          {/* MIDI Upload Tab */}
          <TabsContent value="midi" className="space-y-3">
            <div
              onDrop={handleMidiDrop}
              onDragOver={(e) => e.preventDefault()}
              className="border-2 border-dashed border-border rounded-lg p-6 text-center hover:border-coral-pink/50 transition-colors"
            >
              <svg
                className="w-8 h-8 mx-auto text-muted-foreground mb-2"
                fill="none"
                stroke="currentColor"
                viewBox="0 0 24 24"
              >
                <path
                  strokeLinecap="round"
                  strokeLinejoin="round"
                  strokeWidth={2}
                  d="M12 4v16m8-8H4"
                />
              </svg>
              <p className="text-sm text-muted-foreground mb-3">
                Drag and drop MIDI file or click to browse
              </p>
              <input
                type="file"
                accept=".mid,.midi"
                onChange={(e) => e.target.files && handleMidiUpload(e.target.files[0])}
                className="hidden"
                id="midi-input"
              />
              <label htmlFor="midi-input" className="inline-block">
                <span className="inline-flex items-center justify-center rounded-md border border-input bg-background px-3 py-2 text-sm font-medium ring-offset-background transition-colors hover:bg-accent hover:text-accent-foreground cursor-pointer">
                  Choose MIDI File
                </span>
              </label>
            </div>

            {midiError && (
              <p className="text-sm text-red-500">{midiError}</p>
            )}

            {selectedMidiFile && (
              <div className="p-3 bg-coral-pink/10 border border-coral-pink/20 rounded">
                <p className="text-sm font-medium text-foreground">
                  ✓ MIDI file loaded: {selectedMidiFile.name}
                </p>
                <p className="text-xs text-muted-foreground mt-1">
                  {(selectedMidiFile.size / 1024).toFixed(2)} KB
                </p>
              </div>
            )}

            <p className="text-xs text-muted-foreground">
              Upload the MIDI/note data from your production. This helps other producers remix your work.
            </p>
          </TabsContent>

          {/* Project File Upload Tab */}
          <TabsContent value="project" className="space-y-3">
            <div
              onDrop={handleProjectDrop}
              onDragOver={(e) => e.preventDefault()}
              className="border-2 border-dashed border-border rounded-lg p-6 text-center hover:border-coral-pink/50 transition-colors"
            >
              <svg
                className="w-8 h-8 mx-auto text-muted-foreground mb-2"
                fill="none"
                stroke="currentColor"
                viewBox="0 0 24 24"
              >
                <path
                  strokeLinecap="round"
                  strokeLinejoin="round"
                  strokeWidth={2}
                  d="M9 12h6m-6 4h6m2 5H7a2 2 0 01-2-2V5a2 2 0 012-2h5.586a1 1 0 01.707.293l5.414 5.414a1 1 0 01.293.707V19a2 2 0 01-2 2z"
                />
              </svg>
              <p className="text-sm text-muted-foreground mb-3">
                Drag and drop project file or click to browse
              </p>
              <input
                type="file"
                accept=".als,.flp,.logic,.logicx,.sesx,.cpr,.rrwx"
                onChange={(e) => e.target.files && handleProjectUpload(e.target.files[0])}
                className="hidden"
                id="project-input"
              />
              <label htmlFor="project-input" className="inline-block">
                <span className="inline-flex items-center justify-center rounded-md border border-input bg-background px-3 py-2 text-sm font-medium ring-offset-background transition-colors hover:bg-accent hover:text-accent-foreground cursor-pointer">
                  Choose Project File
                </span>
              </label>
            </div>

            {projectError && (
              <p className="text-sm text-red-500">{projectError}</p>
            )}

            {selectedProjectFile && (
              <div className="p-3 bg-coral-pink/10 border border-coral-pink/20 rounded">
                <p className="text-sm font-medium text-foreground">
                  ✓ Project file loaded: {selectedProjectFile.name}
                </p>
                <p className="text-xs text-muted-foreground mt-1">
                  {(selectedProjectFile.size / (1024 * 1024)).toFixed(2)} MB
                </p>
              </div>
            )}

            <p className="text-xs text-muted-foreground">
              Supported: Ableton Live, FL Studio, Logic Pro, Studio One, Cubase, Reaper
            </p>
          </TabsContent>
        </Tabs>
      </div>
    </div>
  )
}
