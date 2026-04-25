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
COPY pyproject.toml uv.lock README.md ./

# Create virtual environment and install dependencies using lock file
# This ensures reproducible builds with exact versions
ENV UV_SYSTEM_PYTHON=1
ENV UV_FROZEN=1
RUN uv sync --no-dev --no-editable

# Copy application code
COPY app/ ./app/

# Download Essentia models during build (when network is available)
RUN mkdir -p /app/models && \
    BASE="https://essentia.upf.edu/models" && \
    MODEL_FILES=( \
        "discogs-effnet-bs64-1.pb:feature-extractors/discogs-effnet/discogs-effnet-bs64-1.pb" \
        "msd-musicnn-1.pb:autotagging/msd/msd-musicnn-1.pb" \
        "msd-musicnn-1.json:autotagging/msd/msd-musicnn-1.json" \
        "mtg_jamendo_genre-discogs-effnet-1.pb:classification-heads/mtg_jamendo_genre/mtg_jamendo_genre-discogs-effnet-1.pb" \
        "mtg_jamendo_genre-discogs-effnet-1.json:classification-heads/mtg_jamendo_genre/mtg_jamendo_genre-discogs-effnet-1.json" \
        "mtg_jamendo_moodtheme-discogs-effnet-1.pb:classification-heads/mtg_jamendo_moodtheme/mtg_jamendo_moodtheme-discogs-effnet-1.pb" \
        "mtg_jamendo_moodtheme-discogs-effnet-1.json:classification-heads/mtg_jamendo_moodtheme/mtg_jamendo_moodtheme-discogs-effnet-1.json" \
        "mtg_jamendo_instrument-discogs-effnet-1.pb:classification-heads/mtg_jamendo_instrument/mtg_jamendo_instrument-discogs-effnet-1.pb" \
        "mtg_jamendo_instrument-discogs-effnet-1.json:classification-heads/mtg_jamendo_instrument/mtg_jamendo_instrument-discogs-effnet-1.json" \
        "voice_instrumental-discogs-effnet-1.pb:classification-heads/voice_instrumental/voice_instrumental-discogs-effnet-1.pb" \
        "voice_instrumental-discogs-effnet-1.json:classification-heads/voice_instrumental/voice_instrumental-discogs-effnet-1.json" \
        "danceability-discogs-effnet-1.pb:classification-heads/danceability/danceability-discogs-effnet-1.pb" \
        "danceability-discogs-effnet-1.json:classification-heads/danceability/danceability-discogs-effnet-1.json" \
        "emomusic-msd-musicnn-2.pb:classification-heads/emomusic/emomusic-msd-musicnn-2.pb" \
        "emomusic-msd-musicnn-2.json:classification-heads/emomusic/emomusic-msd-musicnn-2.json" \
    ) && \
    for model_pair in "${MODEL_FILES[@]}"; do \
        IFS=':' read -r filename path <<< "$model_pair"; \
        echo "Downloading $filename..."; \
        wget --tries=3 --timeout=60 --waitretry=5 -O "/app/models/$filename" "$BASE/$path" || exit 1; \
    done && \
    echo "All models downloaded successfully."

# Create non-root user for security
RUN useradd -m -u 1000 appuser && \
    mkdir -p /tmp/audio-analysis && \
    chown -R appuser:appuser /app /tmp/audio-analysis

USER appuser

# Expose port
EXPOSE 8090

# Health check
HEALTHCHECK --interval=30s --timeout=10s --start-period=60s --retries=3 \
    CMD curl -f http://localhost:8090/health || exit 1

CMD ["/app/.venv/bin/uvicorn", "app.main:app", "--host", "0.0.0.0", "--port", "8090"]
