"""FastAPI application for audio analysis service."""

import logging
from contextlib import asynccontextmanager

from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware

from .config import get_settings
from .api.routes import router

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s - %(name)s - %(levelname)s - %(message)s",
)
logger = logging.getLogger(__name__)


@asynccontextmanager
async def lifespan(app: FastAPI):
    """Application lifespan manager."""
    settings = get_settings()

    # Startup
    logger.info("Starting audio analysis service...")
    logger.info(f"Configuration: sample_rate={settings.sample_rate}, "
                f"max_duration={settings.max_audio_duration}s, "
                f"timeout={settings.analysis_timeout}s")

    # Pre-initialize analyzer to warm up Essentia
    from .analysis.analyzer import get_analyzer
    analyzer = get_analyzer(sample_rate=settings.sample_rate)
    logger.info("Essentia analyzer initialized")

    yield

    # Shutdown
    logger.info("Shutting down audio analysis service...")


# Create FastAPI application
app = FastAPI(
    title="Sidechain Audio Analysis Service",
    description="Audio analysis microservice for key and BPM detection using Essentia",
    version="0.1.0",
    lifespan=lifespan,
)

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
        log_level=settings.log_level,
        reload=True,
    )
