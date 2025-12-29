"""API routes for audio analysis service."""

import os
import time
import tempfile
from pathlib import Path
from typing import Optional

import httpx
from fastapi import APIRouter, HTTPException, UploadFile, File, Form, status

from ..config import get_settings
from ..logging_config import get_logger
from ..analysis.analyzer import get_analyzer
from ..analysis.tagger import get_tagger
from .schemas import (
    AnalysisRequest,
    AnalysisResponse,
    HealthResponse,
    ErrorResponse,
    KeyResult,
    BPMResult,
    TagResult,
    TagItem,
)

logger = get_logger(__name__)

router = APIRouter()

# Track service start time for uptime
_start_time = time.time()


@router.get("/health", response_model=HealthResponse)
async def health_check():
    """Health check endpoint."""
    import essentia

    return HealthResponse(
        status="healthy",
        essentia_version=essentia.__version__,
        uptime_seconds=time.time() - _start_time,
    )


@router.post(
    "/analyze",
    response_model=AnalysisResponse,
    responses={
        422: {"model": ErrorResponse, "description": "Invalid input"},
        503: {"model": ErrorResponse, "description": "Analysis failed"},
    },
)
async def analyze_audio(
    audio_url: Optional[str] = Form(None),
    audio_file: Optional[UploadFile] = File(None),
    detect_key: bool = Form(True),
    detect_bpm: bool = Form(True),
    detect_tags: bool = Form(False),
    tag_top_n: int = Form(10),
):
    """Analyze audio file for key, BPM, and tag detection.

    Accepts either:
    - audio_url: URL to download the audio file from
    - audio_file: Uploaded audio file

    Returns key detection (if requested) with:
    - Standard notation (e.g., "A minor")
    - Camelot wheel notation (e.g., "8A")
    - Confidence score

    Returns BPM detection (if requested) with:
    - BPM value
    - Confidence score
    - Beat positions (first 100)

    Returns audio tags (if requested) with:
    - Genre classifications (MTG-Jamendo)
    - Mood/theme classifications
    - Instrument detections
    - Vocal/instrumental classification
    - Danceability score
    - Arousal/valence (energy/mood)
    """
    settings = get_settings()
    start_time = time.time()

    # Validate input
    if not audio_url and not audio_file:
        raise HTTPException(
            status_code=status.HTTP_422_UNPROCESSABLE_ENTITY,
            detail={
                "error": "missing_input",
                "message": "Either audio_url or audio_file must be provided",
            },
        )

    # Create temp directory if needed
    temp_dir = Path(settings.temp_dir)
    temp_dir.mkdir(parents=True, exist_ok=True)

    temp_path: Optional[Path] = None

    try:
        # Get audio file path (download or save upload)
        if audio_url:
            temp_path = await _download_audio(audio_url, temp_dir)
        elif audio_file:
            temp_path = await _save_upload(audio_file, temp_dir)

        if not temp_path or not temp_path.exists():
            raise HTTPException(
                status_code=status.HTTP_422_UNPROCESSABLE_ENTITY,
                detail={
                    "error": "file_error",
                    "message": "Failed to obtain audio file",
                },
            )

        # Run key/BPM analysis
        analyzer = get_analyzer(sample_rate=settings.sample_rate)
        result = analyzer.analyze(
            str(temp_path),
            detect_key=detect_key,
            detect_bpm=detect_bpm,
        )

        # Run tagging if requested
        tags_result = None
        if detect_tags:
            try:
                tagger = get_tagger()
                tag_data = tagger.tag(str(temp_path), top_n=tag_top_n)

                # Convert to TagResult schema
                tags_result = TagResult(
                    top_tags=[TagItem(**t) for t in tag_data.get("top_tags", [])] if tag_data.get("top_tags") else None,
                    genres=tag_data.get("genres"),
                    moods=tag_data.get("moods"),
                    instruments=tag_data.get("instruments"),
                    has_vocals=tag_data.get("has_vocals"),
                    vocal_confidence=tag_data.get("vocal_confidence"),
                    is_danceable=tag_data.get("is_danceable"),
                    danceability_confidence=tag_data.get("danceability_confidence"),
                    arousal=tag_data.get("arousal"),
                    valence=tag_data.get("valence"),
                )
                logger.info(
                    "tagging_complete",
                    genres_count=len(tags_result.genres or []),
                    moods_count=len(tags_result.moods or []),
                    has_vocals=tags_result.has_vocals,
                )
            except Exception as e:
                logger.warning("tagging_failed", error=str(e))

        # Build response
        analysis_time_ms = int((time.time() - start_time) * 1000)

        response = AnalysisResponse(
            key=KeyResult(**result["key"]) if result.get("key") else None,
            bpm=BPMResult(**result["bpm"]) if result.get("bpm") else None,
            tags=tags_result,
            duration=result["duration"],
            analysis_time_ms=analysis_time_ms,
        )

        logger.info(
            "analysis_complete",
            key=response.key.value if response.key else None,
            bpm=response.bpm.value if response.bpm else None,
            has_tags=response.tags is not None,
            analysis_time_ms=analysis_time_ms,
        )

        return response

    except ValueError as e:
        # Audio too short or invalid
        raise HTTPException(
            status_code=status.HTTP_422_UNPROCESSABLE_ENTITY,
            detail={
                "error": "invalid_audio",
                "message": str(e),
            },
        )

    except FileNotFoundError as e:
        raise HTTPException(
            status_code=status.HTTP_422_UNPROCESSABLE_ENTITY,
            detail={
                "error": "file_not_found",
                "message": str(e),
            },
        )

    except Exception as e:
        logger.exception("analysis_failed", error=str(e))
        raise HTTPException(
            status_code=status.HTTP_503_SERVICE_UNAVAILABLE,
            detail={
                "error": "analysis_failed",
                "message": f"Failed to analyze audio: {str(e)}",
            },
        )

    finally:
        # Cleanup temp file
        if settings.cleanup_temp_files and temp_path and temp_path.exists():
            try:
                temp_path.unlink()
                logger.debug("temp_file_cleaned", path=str(temp_path))
            except Exception as e:
                logger.warning("temp_file_cleanup_failed", path=str(temp_path), error=str(e))


@router.post(
    "/analyze/json",
    response_model=AnalysisResponse,
    responses={
        422: {"model": ErrorResponse, "description": "Invalid input"},
        503: {"model": ErrorResponse, "description": "Analysis failed"},
    },
)
async def analyze_audio_json(request: AnalysisRequest):
    """Analyze audio from URL (JSON request body).

    Alternative endpoint that accepts JSON body instead of form data.
    Only supports audio_url, not file uploads.
    """
    if not request.audio_url:
        raise HTTPException(
            status_code=status.HTTP_422_UNPROCESSABLE_ENTITY,
            detail={
                "error": "missing_url",
                "message": "audio_url is required for JSON endpoint",
            },
        )

    # Delegate to main analyze function
    return await analyze_audio(
        audio_url=request.audio_url,
        audio_file=None,
        detect_key=request.detect_key,
        detect_bpm=request.detect_bpm,
        detect_tags=request.detect_tags,
        tag_top_n=request.tag_top_n,
    )


async def _download_audio(url: str, temp_dir: Path) -> Path:
    """Download audio from URL to temp file.

    Args:
        url: URL to download from
        temp_dir: Directory to save temp file

    Returns:
        Path to downloaded file

    Raises:
        HTTPException: If download fails
    """
    settings = get_settings()
    logger.info("downloading_audio", url=url)

    try:
        async with httpx.AsyncClient(timeout=settings.analysis_timeout) as client:
            response = await client.get(url, follow_redirects=True)
            response.raise_for_status()

            # Determine file extension from content type or URL
            content_type = response.headers.get("content-type", "")
            ext = _get_extension(content_type, url)

            # Create temp file
            fd, temp_path = tempfile.mkstemp(suffix=ext, dir=str(temp_dir))
            with os.fdopen(fd, "wb") as f:
                f.write(response.content)

            logger.info("audio_downloaded", bytes=len(response.content), path=temp_path)
            return Path(temp_path)

    except httpx.HTTPError as e:
        logger.error("audio_download_failed", error=str(e))
        raise HTTPException(
            status_code=status.HTTP_422_UNPROCESSABLE_ENTITY,
            detail={
                "error": "download_failed",
                "message": f"Failed to download audio from URL: {str(e)}",
            },
        )


async def _save_upload(upload: UploadFile, temp_dir: Path) -> Path:
    """Save uploaded file to temp location.

    Args:
        upload: Uploaded file
        temp_dir: Directory to save temp file

    Returns:
        Path to saved file
    """
    # Get extension from filename
    ext = Path(upload.filename or "audio.wav").suffix or ".wav"

    # Create temp file
    fd, temp_path = tempfile.mkstemp(suffix=ext, dir=str(temp_dir))
    with os.fdopen(fd, "wb") as f:
        content = await upload.read()
        f.write(content)

    logger.info("upload_saved", bytes=len(content), path=temp_path)
    return Path(temp_path)


def _get_extension(content_type: str, url: str) -> str:
    """Determine file extension from content type or URL.

    Args:
        content_type: HTTP Content-Type header
        url: Original URL

    Returns:
        File extension including dot (e.g., ".mp3")
    """
    # Try content type first
    content_type_map = {
        "audio/mpeg": ".mp3",
        "audio/mp3": ".mp3",
        "audio/wav": ".wav",
        "audio/x-wav": ".wav",
        "audio/wave": ".wav",
        "audio/aiff": ".aiff",
        "audio/x-aiff": ".aiff",
        "audio/mp4": ".m4a",
        "audio/x-m4a": ".m4a",
        "audio/flac": ".flac",
        "audio/ogg": ".ogg",
    }

    for mime, ext in content_type_map.items():
        if mime in content_type.lower():
            return ext

    # Try URL extension
    url_path = url.split("?")[0]  # Remove query params
    url_ext = Path(url_path).suffix.lower()
    if url_ext in [".mp3", ".wav", ".aiff", ".m4a", ".flac", ".ogg"]:
        return url_ext

    # Default to mp3
    return ".mp3"
