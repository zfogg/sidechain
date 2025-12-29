"""Pydantic models for API request/response schemas."""

from typing import Optional, List
from pydantic import BaseModel, Field


class KeyResult(BaseModel):
    """Musical key detection result."""

    value: str = Field(..., description="Key in standard notation (e.g., 'A minor')")
    camelot: str = Field(..., description="Camelot wheel notation (e.g., '8A')")
    confidence: float = Field(..., ge=0.0, le=1.0, description="Detection confidence")
    scale: str = Field(..., description="Scale type: 'major' or 'minor'")
    root: str = Field(..., description="Root note (e.g., 'A', 'F#')")


class BPMResult(BaseModel):
    """BPM/tempo detection result."""

    value: float = Field(..., gt=0, description="Detected BPM")
    confidence: float = Field(..., ge=0.0, le=1.0, description="Detection confidence")
    beats: Optional[List[float]] = Field(
        None, description="Beat positions in seconds (limited to first 100)"
    )


class TagItem(BaseModel):
    """Single tag with confidence score."""

    name: str = Field(..., description="Tag name")
    confidence: float = Field(..., ge=0.0, le=1.0, description="Tag confidence score")


class TagResult(BaseModel):
    """Audio tagging result from TensorFlow models."""

    top_tags: Optional[List[TagItem]] = Field(
        None, description="Top general audio tags (MagnaTagATune)"
    )
    genres: Optional[List[str]] = Field(
        None, description="Detected genres (MTG-Jamendo, up to 87 categories)"
    )
    moods: Optional[List[str]] = Field(
        None, description="Detected moods/themes (MTG-Jamendo, up to 56 categories)"
    )
    instruments: Optional[List[str]] = Field(
        None, description="Detected instruments (MTG-Jamendo, up to 40 categories)"
    )
    has_vocals: Optional[bool] = Field(
        None, description="Whether audio contains vocals"
    )
    vocal_confidence: Optional[float] = Field(
        None, ge=0.0, le=1.0, description="Confidence for vocal detection"
    )
    is_danceable: Optional[bool] = Field(
        None, description="Whether audio is danceable"
    )
    danceability_confidence: Optional[float] = Field(
        None, ge=0.0, le=1.0, description="Confidence for danceability detection"
    )
    arousal: Optional[float] = Field(
        None, ge=0.0, le=1.0, description="Energy level (0=calm, 1=energetic)"
    )
    valence: Optional[float] = Field(
        None, ge=0.0, le=1.0, description="Mood positivity (0=negative, 1=positive)"
    )


class AnalysisRequest(BaseModel):
    """Request body for audio analysis."""

    audio_url: Optional[str] = Field(None, description="URL to audio file (S3/CDN)")
    detect_key: bool = Field(True, description="Whether to detect musical key")
    detect_bpm: bool = Field(True, description="Whether to detect BPM/tempo")
    detect_tags: bool = Field(False, description="Whether to detect audio tags (genres, moods, etc.)")
    tag_top_n: int = Field(10, ge=1, le=50, description="Number of top tags to return per category")


class AnalysisResponse(BaseModel):
    """Response body for audio analysis."""

    key: Optional[KeyResult] = Field(None, description="Key detection result")
    bpm: Optional[BPMResult] = Field(None, description="BPM detection result")
    tags: Optional[TagResult] = Field(None, description="Audio tagging result")
    duration: float = Field(..., description="Audio duration in seconds")
    analysis_time_ms: int = Field(..., description="Processing time in milliseconds")


class HealthResponse(BaseModel):
    """Health check response."""

    status: str = Field(..., description="Service status")
    essentia_version: str = Field(..., description="Essentia library version")
    uptime_seconds: float = Field(..., description="Service uptime in seconds")


class ErrorResponse(BaseModel):
    """Error response."""

    error: str = Field(..., description="Error code")
    message: str = Field(..., description="Human-readable error message")
