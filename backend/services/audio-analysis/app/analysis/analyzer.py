"""Audio analysis using Essentia library for key and BPM detection."""

import logging
from typing import Optional, Dict, Any
from pathlib import Path

import essentia
import essentia.standard as es

from .camelot import to_camelot, normalize_key_name

logger = logging.getLogger(__name__)


class AudioAnalyzer:
    """Analyzes audio files for musical key and BPM using Essentia."""

    def __init__(self, sample_rate: int = 44100):
        """Initialize the analyzer.

        Args:
            sample_rate: Sample rate for audio loading (default 44100 Hz)
        """
        self.sample_rate = sample_rate

        # Initialize Essentia algorithms
        self.key_extractor = es.KeyExtractor()
        self.rhythm_extractor = es.RhythmExtractor2013(method="multifeature")

        logger.info(
            f"AudioAnalyzer initialized with sample_rate={sample_rate}, "
            f"essentia_version={essentia.__version__}"
        )

    def analyze(
        self,
        audio_path: str,
        detect_key: bool = True,
        detect_bpm: bool = True,
    ) -> Dict[str, Any]:
        """Analyze an audio file for key and BPM.

        Args:
            audio_path: Path to the audio file
            detect_key: Whether to detect musical key
            detect_bpm: Whether to detect BPM/tempo

        Returns:
            Dictionary containing analysis results

        Raises:
            FileNotFoundError: If audio file doesn't exist
            ValueError: If audio is too short for analysis
            RuntimeError: If analysis fails
        """
        path = Path(audio_path)
        if not path.exists():
            raise FileNotFoundError(f"Audio file not found: {audio_path}")

        logger.info(f"Loading audio from: {audio_path}")

        # Load audio as mono
        loader = es.MonoLoader(filename=str(path), sampleRate=self.sample_rate)
        audio = loader()

        # Calculate duration
        duration = len(audio) / self.sample_rate
        logger.info(f"Audio loaded: {duration:.2f} seconds, {len(audio)} samples")

        # Minimum duration check (need at least 3 seconds for reliable analysis)
        if duration < 3.0:
            raise ValueError(
                f"Audio too short for analysis: {duration:.2f}s (minimum 3s required)"
            )

        result: Dict[str, Any] = {"duration": duration}

        # Key detection
        if detect_key:
            try:
                key_result = self._detect_key(audio)
                result["key"] = key_result
                logger.info(
                    f"Key detected: {key_result['value']} "
                    f"(confidence: {key_result['confidence']:.2f})"
                )
            except Exception as e:
                logger.error(f"Key detection failed: {e}")
                result["key"] = None

        # BPM detection
        if detect_bpm:
            try:
                bpm_result = self._detect_bpm(audio)
                result["bpm"] = bpm_result
                logger.info(
                    f"BPM detected: {bpm_result['value']:.1f} "
                    f"(confidence: {bpm_result['confidence']:.2f})"
                )
            except Exception as e:
                logger.error(f"BPM detection failed: {e}")
                result["bpm"] = None

        return result

    def _detect_key(self, audio) -> Dict[str, Any]:
        """Detect musical key from audio samples.

        Args:
            audio: Mono audio samples (numpy array)

        Returns:
            Dictionary with key detection results
        """
        # Run key extraction
        key, scale, strength = self.key_extractor(audio)

        # Normalize key name
        key = normalize_key_name(key)

        # Get Camelot notation
        camelot = to_camelot(key, scale)

        return {
            "value": f"{key} {scale}",
            "camelot": camelot,
            "confidence": float(strength),
            "scale": scale,
            "root": key,
        }

    def _detect_bpm(self, audio) -> Dict[str, Any]:
        """Detect BPM/tempo from audio samples.

        Args:
            audio: Mono audio samples (numpy array)

        Returns:
            Dictionary with BPM detection results
        """
        # Run rhythm extraction
        # Returns: bpm, beats, beats_confidence, _, beats_loudness
        bpm, beats, beats_confidence, _, _ = self.rhythm_extractor(audio)

        # Limit beats array to first 100 to avoid huge responses
        beats_list = [float(b) for b in beats[:100]] if len(beats) > 0 else []

        return {
            "value": float(bpm),
            "confidence": float(beats_confidence),
            "beats": beats_list if beats_list else None,
        }

    def analyze_file(
        self,
        audio_path: str,
        detect_key: bool = True,
        detect_bpm: bool = True,
    ) -> Dict[str, Any]:
        """Convenience method - alias for analyze().

        Args:
            audio_path: Path to the audio file
            detect_key: Whether to detect musical key
            detect_bpm: Whether to detect BPM/tempo

        Returns:
            Dictionary containing analysis results
        """
        return self.analyze(audio_path, detect_key, detect_bpm)


# Global analyzer instance (lazy initialized)
_analyzer: Optional[AudioAnalyzer] = None


def get_analyzer(sample_rate: int = 44100) -> AudioAnalyzer:
    """Get or create the global analyzer instance.

    Args:
        sample_rate: Sample rate for audio loading

    Returns:
        AudioAnalyzer instance
    """
    global _analyzer
    if _analyzer is None:
        _analyzer = AudioAnalyzer(sample_rate=sample_rate)
    return _analyzer
