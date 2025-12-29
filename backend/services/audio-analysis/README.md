# Audio Analysis Service

Audio analysis microservice for Sidechain using [Essentia](https://essentia.upf.edu/) for key, BPM, and tag detection.

## Features

- **Key Detection**: Musical key with Camelot wheel notation
- **BPM Detection**: Tempo with beat positions
- **Audio Tagging**: Genre, mood, instruments using Essentia TensorFlow models

## API Endpoints

### `GET /health`
Health check endpoint.

### `POST /api/v1/analyze`
Analyze audio file for key, BPM, and tags.

**Form parameters:**
- `audio_url` (optional): URL to download audio from
- `audio_file` (optional): Uploaded audio file
- `detect_key` (bool, default: true): Enable key detection
- `detect_bpm` (bool, default: true): Enable BPM detection
- `detect_tags` (bool, default: false): Enable audio tagging

### `POST /api/v1/analyze/json`
Same as above but accepts JSON request body.

## Running Locally

```bash
# Install dependencies
uv sync

# Run development server
uv run uvicorn app.main:app --reload --port 8090
```

## Docker

```bash
# Build image (requires x86_64 platform for Essentia)
docker build --platform linux/amd64 -t audio-analysis .

# Run container
docker run -p 8090:8090 audio-analysis
```

## Models

The service uses pre-trained Essentia TensorFlow models:
- MagnaTagATune (msd-musicnn) - General audio tags
- MTG-Jamendo Genre - Genre classification
- MTG-Jamendo Mood/Theme - Mood detection
- MTG-Jamendo Instruments - Instrument detection
- Voice/Instrumental - Vocal detection
- Danceability - Danceability score
- Emomusic - Arousal/Valence dimensions
