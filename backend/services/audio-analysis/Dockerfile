# Audio Analysis Service Dockerfile
# Uses Python with Essentia for key, BPM, and audio tagging

FROM python:3.11-slim

# Install system dependencies required by Essentia and uv
RUN apt-get update && apt-get install -y --no-install-recommends \
    ffmpeg \
    libfftw3-dev \
    libyaml-dev \
    libsamplerate0-dev \
    libavcodec-dev \
    libavformat-dev \
    libavutil-dev \
    libswresample-dev \
    libtag1-dev \
    libchromaprint-dev \
    curl \
    wget \
    && rm -rf /var/lib/apt/lists/*

# Install uv
COPY --from=ghcr.io/astral-sh/uv:latest /uv /uvx /bin/

# Set working directory
WORKDIR /app

# Copy project files for dependency installation
COPY pyproject.toml .

# Create virtual environment and install dependencies
ENV UV_SYSTEM_PYTHON=1
RUN uv pip install -e . --no-cache

# Download Essentia TF models (~200MB total)
RUN mkdir -p /app/models && \
    # MagnaTagATune (50 tags)
    wget -q -O /app/models/msd-musicnn-1.pb \
      https://essentia.upf.edu/models/autotagging/msd/msd-musicnn-1.pb && \
    wget -q -O /app/models/msd-musicnn-1.json \
      https://essentia.upf.edu/models/autotagging/msd/msd-musicnn-1.json && \
    # MTG-Jamendo Genre (87 genres)
    wget -q -O /app/models/mtg_jamendo_genre-discogs-effnet-1.pb \
      https://essentia.upf.edu/models/classification/mtg_jamendo/mtg_jamendo_genre-discogs-effnet-1.pb && \
    wget -q -O /app/models/mtg_jamendo_genre-discogs-effnet-1.json \
      https://essentia.upf.edu/models/classification/mtg_jamendo/mtg_jamendo_genre-discogs-effnet-1.json && \
    # MTG-Jamendo Mood/Theme (56 moods)
    wget -q -O /app/models/mtg_jamendo_moodtheme-discogs-effnet-1.pb \
      https://essentia.upf.edu/models/classification/mtg_jamendo/mtg_jamendo_moodtheme-discogs-effnet-1.pb && \
    wget -q -O /app/models/mtg_jamendo_moodtheme-discogs-effnet-1.json \
      https://essentia.upf.edu/models/classification/mtg_jamendo/mtg_jamendo_moodtheme-discogs-effnet-1.json && \
    # MTG-Jamendo Instruments (40 instruments)
    wget -q -O /app/models/mtg_jamendo_instrument-discogs-effnet-1.pb \
      https://essentia.upf.edu/models/classification/mtg_jamendo/mtg_jamendo_instrument-discogs-effnet-1.pb && \
    wget -q -O /app/models/mtg_jamendo_instrument-discogs-effnet-1.json \
      https://essentia.upf.edu/models/classification/mtg_jamendo/mtg_jamendo_instrument-discogs-effnet-1.json && \
    # Voice/Instrumental detection
    wget -q -O /app/models/voice_instrumental-discogs-effnet-1.pb \
      https://essentia.upf.edu/models/classification/voice_instrumental/voice_instrumental-discogs-effnet-1.pb && \
    wget -q -O /app/models/voice_instrumental-discogs-effnet-1.json \
      https://essentia.upf.edu/models/classification/voice_instrumental/voice_instrumental-discogs-effnet-1.json && \
    # Danceability
    wget -q -O /app/models/danceability-discogs-effnet-1.pb \
      https://essentia.upf.edu/models/classification/danceability/danceability-discogs-effnet-1.pb && \
    wget -q -O /app/models/danceability-discogs-effnet-1.json \
      https://essentia.upf.edu/models/classification/danceability/danceability-discogs-effnet-1.json && \
    # Arousal/Valence (emomusic)
    wget -q -O /app/models/emomusic-msd-musicnn-2.pb \
      https://essentia.upf.edu/models/classification/emomusic/emomusic-msd-musicnn-2.pb && \
    wget -q -O /app/models/emomusic-msd-musicnn-2.json \
      https://essentia.upf.edu/models/classification/emomusic/emomusic-msd-musicnn-2.json

# Copy application code
COPY app/ ./app/

# Create non-root user for security
RUN useradd -m -u 1000 appuser && \
    mkdir -p /tmp/audio-analysis && \
    chown -R appuser:appuser /app /tmp/audio-analysis

USER appuser

# Expose port
EXPOSE 8090

# Health check
HEALTHCHECK --interval=30s --timeout=10s --start-period=15s --retries=3 \
    CMD curl -f http://localhost:8090/health || exit 1

# Run the application
CMD ["uvicorn", "app.main:app", "--host", "0.0.0.0", "--port", "8090"]
