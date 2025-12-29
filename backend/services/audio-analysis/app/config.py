"""Configuration settings for the audio analysis service."""

from pydantic_settings import BaseSettings
from functools import lru_cache


class Settings(BaseSettings):
    """Application settings loaded from environment variables."""

    # Server settings
    host: str = "0.0.0.0"
    port: int = 8090
    log_level: str = "info"

    # Analysis settings
    max_audio_duration: int = 300  # Maximum audio duration in seconds (5 minutes)
    analysis_timeout: int = 60  # Timeout for analysis in seconds
    sample_rate: int = 44100  # Sample rate for analysis

    # Temp file settings
    temp_dir: str = "/tmp/audio-analysis"
    cleanup_temp_files: bool = True

    class Config:
        env_prefix = ""
        case_sensitive = False


@lru_cache()
def get_settings() -> Settings:
    """Get cached settings instance."""
    return Settings()
