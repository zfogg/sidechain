# Waveform System Implementation

## Overview
This document tracks the implementation of the real waveform system that generates PNG waveforms, stores them in S3, and displays them in the plugin.

## ✅ Completed

### Frontend (JUCE Plugin)
- [x] **WaveformImageView Component** (`plugin/src/ui/common/WaveformImageView.h/cpp`)
  - Downloads waveform PNGs from URLs
  - Caches images in memory
  - Shows loading state
  - Handles download failures gracefully

### Backend (Go)
- [x] **Waveform Generator** (`backend/internal/waveform/generator.go`)
  - Generates PNG waveforms from WAV audio
  - Configurable dimensions (default: 1200x200)
  - Uses RMS algorithm for clean waveform visualization
  - Colors match plugin theme (background: #26262C, foreground: #B4E4FF)

- [x] **S3 Storage** (`backend/internal/waveform/storage.go`)
  - Uploads waveforms to S3
  - Returns CDN URLs
  - Supports deletion
  - Caching headers for performance

- [x] **Database Models**
  - Added `waveform_url` to `AudioPost` struct
  - Added `waveform_url` to `Story` struct
  - GORM will auto-migrate on next server start

### Backend Integration ✅
- [x] **Go dependencies installed**:
  - `github.com/go-audio/audio v1.0.0`
  - `github.com/go-audio/wav v1.1.0`
  - `github.com/aws/aws-sdk-go v1.55.8`

- [x] **CreateStory handler updated** (`backend/internal/handlers/stories.go`):
  - Generates waveform PNG after audio upload
  - Uploads to S3 and stores waveform_url in Story model
  - Returns waveform_url in API response
  - Handles errors gracefully (logs warning, doesn't fail upload)

- [x] **Backfill script created** (`backend/cmd/backfill-waveforms/main.go`):
  - Backfills existing posts and stories without waveforms
  - Downloads audio from CDN, generates waveform, uploads to S3
  - Progress tracking and error reporting
  - Commands: `all`, `posts`, `stories`, `dry-run`

- [x] **Handlers struct updated**:
  - Added waveformGenerator and waveformStorage fields
  - Added SetWaveformTools() method for dependency injection

- [x] **Environment variables** (already configured in `.env`):
  ```bash
  AWS_BUCKET=sidechain-dev-media  # Same bucket as audio uploads
  AWS_REGION=us-west-2            # Same region as audio uploads
  CDN_URL=https://cdn.sidechain.live  # optional
  ```

## 🔄 In Progress / TODO

### Backend Integration
- [ ] Update `UploadAudio` handler to:
  1. Generate waveform from uploaded WAV
  2. Upload waveform PNG to S3
  3. Store `waveform_url` in database
  4. Return `waveform_url` in API response

### Frontend Integration
- [x] **Updated FeedPost model** (`plugin/src/models/FeedPost.h/cpp`):
  - Added `waveformUrl` field
  - Parses from JSON in `fromJson()`
  - Serializes to JSON in `toJson()`

- [x] **Updated Story model** (`plugin/src/models/Story.h/cpp`):
  - Added `waveformUrl` field
  - Parses from JSON in `fromJSON()`

- [x] **Integrated WaveformImageView into PostCard** (`plugin/src/ui/feed/PostCard.h/cpp`):
  - Added WaveformImageView as child component
  - Loads waveform from `post.waveformUrl` when available
  - Falls back to fake waveforms for legacy posts
  - Added `setNetworkClient()` method for dependency injection
  - Positions waveform correctly in resized()

- [ ] Replace fake waveforms in `StoryViewer.cpp`
  - Add `WaveformImageView` member
  - Load from `story->waveformUrl`

- [ ] Replace fake waveforms in `StoryRecording.cpp`
  - Show placeholder during recording
  - Load real waveform in preview state

### Testing
- [ ] Test waveform generation with various audio formats
- [ ] Test S3 upload and retrieval
- [ ] Test CDN caching
- [ ] Test plugin display of waveforms
- [ ] Test error handling (network failures, etc.)

## File Locations

### Frontend (C++)
```
plugin/src/ui/common/WaveformImageView.h
plugin/src/ui/common/WaveformImageView.cpp
plugin/src/ui/feed/PostCard.cpp (needs integration)
plugin/src/ui/stories/StoryViewer.cpp (needs integration)
plugin/src/ui/stories/StoryRecording.cpp (needs integration)
plugin/src/models/FeedPost.h (needs waveformUrl field)
plugin/src/models/Story.h (needs waveformUrl field)
```

### Backend (Go)
```
backend/internal/waveform/generator.go
backend/internal/waveform/storage.go
backend/internal/models/user.go (✅ updated)
backend/internal/handlers/audio.go (needs integration)
backend/internal/handlers/stories.go (needs integration)
```

## Usage

### Running the Backfill Script

The backfill script generates waveforms for existing posts and stories in your database.

**No configuration needed!** It uses your existing `.env` file:

```bash
# Dry run (see what needs backfilling)
go run cmd/backfill-waveforms/main.go dry-run

# Backfill everything
go run cmd/backfill-waveforms/main.go all

# Backfill just posts or just stories
go run cmd/backfill-waveforms/main.go posts
go run cmd/backfill-waveforms/main.go stories
```

**Environment variables used** (from your existing `.env`):
```bash
AWS_BUCKET=sidechain-dev-media  # Your existing S3 bucket
AWS_REGION=us-west-2            # Your existing region
CDN_URL=...                     # Optional - your CDN base URL
```

### Setting up Waveform Generation in Server

In your `cmd/server/main.go`, initialize waveform tools and inject them:

```go
import "github.com/zfogg/sidechain/backend/internal/waveform"

// After creating handlers
generator := waveform.NewGenerator()
storage, err := waveform.NewStorage(
    os.Getenv("AWS_REGION"),  // us-west-2
    os.Getenv("AWS_BUCKET"),  // sidechain-dev-media (same bucket as audio)
    os.Getenv("CDN_URL"),     // optional
)
if err != nil {
    log.Fatalf("Failed to initialize waveform storage: %v", err)
}

handlers.SetWaveformTools(generator, storage)
```

## Next Steps

1. ✅ ~~Install Go dependencies~~

2. ✅ ~~Update story upload handler to generate and upload waveforms~~

3. **Update audio upload handler** to generate and upload waveforms

4. ✅ ~~Add waveformUrl to plugin models (FeedPost, Story)~~

5. ✅ ~~Integrate WaveformImageView in PostCard~~

6. **Integrate WaveformImageView in StoryViewer, StoryRecording**

7. **Remove all fake waveform generation code**

8. **Test end-to-end** with real uploads

9. **Run backfill script** on existing database

## Architecture

```
┌─────────────────┐
│  Upload Audio   │
└────────┬────────┘
         │
         ├─> Generate Waveform PNG (1200x200)
         ├─> Upload to S3 (s3://bucket/waveforms/user_id/post_id_timestamp.png)
         ├─> Get CDN URL (https://cdn.sidechain.live/waveforms/...)
         ├─> Store in DB (audio_posts.waveform_url)
         └─> Return in API response
                  │
                  v
         ┌────────────────┐
         │  Plugin Feed   │
         └────────┬───────┘
                  │
                  ├─> Parse waveformUrl from JSON
                  ├─> WaveformImageView.loadFromUrl(url)
                  ├─> Download PNG from CDN
                  └─> Display in PostCard/StoryViewer
```

## Performance Considerations

1. **Waveform Generation**: ~100-500ms for 60-second audio
2. **S3 Upload**: ~200-1000ms depending on network
3. **CDN Delivery**: ~50-200ms first request, <50ms cached
4. **Plugin Download**: Asynchronous, non-blocking UI
5. **Memory**: PNG files are ~50-200KB each

## Open Questions

- [ ] Should we generate multiple sizes (thumbnail, full)?
- [ ] Should we use progressive JPG instead of PNG for smaller file size?
- [ ] Should we pre-generate waveforms for existing posts?
- [ ] Should we cache waveforms locally in the plugin?
