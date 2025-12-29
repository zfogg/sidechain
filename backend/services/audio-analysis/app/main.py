"""FastAPI application for audio analysis service."""

import time
from contextlib import asynccontextmanager
from typing import Callable

from fastapi import FastAPI, Request, Response
from fastapi.middleware.cors import CORSMiddleware
from starlette.middleware.base import BaseHTTPMiddleware

from .config import get_settings
from .logging_config import configure_logging, get_logger, set_request_id
from .api.routes import router


@asynccontextmanager
async def lifespan(app: FastAPI):
    """Application lifespan manager."""
    settings = get_settings()

    # Configure structured logging
    # Use JSON logs in production (when not in debug mode)
    json_logs = settings.log_level.upper() != "DEBUG"
    configure_logging(
        log_level=settings.log_level,
        json_logs=json_logs,
        log_to_stdout=True,
    )

    logger = get_logger(__name__)

    # Startup
    logger.info(
        "starting_audio_analysis_service",
        sample_rate=settings.sample_rate,
        max_duration=settings.max_audio_duration,
        timeout=settings.analysis_timeout,
        temp_dir=settings.temp_dir,
    )

    # Pre-initialize analyzer to warm up Essentia
    from .analysis.analyzer import get_analyzer

    analyzer = get_analyzer(sample_rate=settings.sample_rate)
    logger.info("essentia_analyzer_initialized")

    # Pre-initialize tagger (optional - can be lazy loaded)
    try:
        from .analysis.tagger import get_tagger

        tagger = get_tagger(models_dir=settings.models_dir)
        logger.info("audio_tagger_initialized")
    except Exception as e:
        logger.warning("audio_tagger_initialization_failed", error=str(e))

    yield

    # Shutdown
    logger.info("shutting_down_audio_analysis_service")


class RequestLoggingMiddleware(BaseHTTPMiddleware):
    """Middleware for request logging with request IDs and timing."""

    async def dispatch(self, request: Request, call_next: Callable) -> Response:
        # Generate request ID
        request_id = request.headers.get("X-Request-ID")
        request_id = set_request_id(request_id)

        logger = get_logger(__name__)
        start_time = time.perf_counter()

        # Log request
        logger.info(
            "request_started",
            method=request.method,
            path=request.url.path,
            query=str(request.url.query) if request.url.query else None,
            client_host=request.client.host if request.client else None,
        )

        try:
            response = await call_next(request)
            process_time = (time.perf_counter() - start_time) * 1000

            # Log response
            logger.info(
                "request_completed",
                method=request.method,
                path=request.url.path,
                status_code=response.status_code,
                duration_ms=round(process_time, 2),
            )

            # Add request ID to response headers
            response.headers["X-Request-ID"] = request_id

            return response

        except Exception as e:
            process_time = (time.perf_counter() - start_time) * 1000
            logger.error(
                "request_failed",
                method=request.method,
                path=request.url.path,
                error=str(e),
                error_type=type(e).__name__,
                duration_ms=round(process_time, 2),
            )
            raise


# Create FastAPI application
app = FastAPI(
    title="Sidechain Audio Analysis Service",
    description="Audio analysis microservice for key, BPM, and tag detection using Essentia",
    version="0.1.0",
    lifespan=lifespan,
)

# Add request logging middleware (before CORS)
app.add_middleware(RequestLoggingMiddleware)

# Configure CORS
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],  # In production, restrict to specific origins
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# Include API routes
app.include_router(router, prefix="/api/v1", tags=["analysis"])

# Also mount at root for convenience
app.include_router(router, tags=["analysis"])


@app.get("/")
async def root():
    """Root endpoint with service info."""
    return {
        "service": "audio-analysis",
        "version": "0.1.0",
        "endpoints": {
            "health": "/health",
            "analyze": "/api/v1/analyze",
            "analyze_json": "/api/v1/analyze/json",
        },
    }


if __name__ == "__main__":
    import uvicorn

    settings = get_settings()
    uvicorn.run(
        "app.main:app",
        host=settings.host,
        port=settings.port,
        log_level=settings.log_level.lower(),
        reload=True,
    )
