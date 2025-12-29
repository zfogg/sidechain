"""Audio tagging using Essentia TensorFlow models."""

import json
import logging
from pathlib import Path
from typing import Optional, Dict, Any, List

import numpy as np
import essentia.standard as es

logger = logging.getLogger(__name__)


class AudioTagger:
    """Analyzes audio files for genre, mood, instruments, and other tags using TensorFlow models."""

    def __init__(self, models_dir: str = "/app/models", sample_rate: int = 16000):
        """Initialize the tagger with pre-trained models.

        Args:
            models_dir: Directory containing the .pb and .json model files
            sample_rate: Sample rate for audio loading (16000 Hz for Essentia TF models)
        """
        self.models_dir = Path(models_dir)
        self.sample_rate = sample_rate
        self._models_loaded = False

        # Model metadata (class labels)
        self._metadata: Dict[str, Any] = {}

        # Lazy-loaded model instances
        self._msd_model = None
        self._genre_model = None
        self._mood_model = None
        self._instrument_model = None
        self._voice_model = None
        self._danceability_model = None
        self._emomusic_model = None

        # Embedding extractors for EffNet-based models
        self._discogs_embedder = None
        self._musicnn_embedder = None

        logger.info(f"AudioTagger initialized with models_dir={models_dir}")

    def _load_metadata(self, model_name: str) -> Dict[str, Any]:
        """Load model metadata (class labels) from JSON file."""
        if model_name in self._metadata:
            return self._metadata[model_name]

        json_path = self.models_dir / f"{model_name}.json"
        if not json_path.exists():
            logger.warning(f"Model metadata not found: {json_path}")
            return {}

        with open(json_path) as f:
            self._metadata[model_name] = json.load(f)
        return self._metadata[model_name]

    def _ensure_models_loaded(self):
        """Lazy-load all TensorFlow models."""
        if self._models_loaded:
            return

        logger.info("Loading Essentia TensorFlow models...")

        # Load embedding extractors
        try:
            discogs_pb = self.models_dir / "discogs-effnet-bs64-1.pb"
            if discogs_pb.exists():
                self._discogs_embedder = es.TensorflowPredictEffnetDiscogs(
                    graphFilename=str(discogs_pb),
                    output="PartitionedCall:1"
                )
            else:
                # Use first available effnet model for embeddings
                self._discogs_embedder = es.TensorflowPredictEffnetDiscogs(
                    graphFilename=str(self.models_dir / "mtg_jamendo_genre-discogs-effnet-1.pb"),
                    output="PartitionedCall:1"
                )
        except Exception as e:
            logger.warning(f"Failed to load EffNet embedder: {e}")

        try:
            self._musicnn_embedder = es.TensorflowPredictMusiCNN(
                graphFilename=str(self.models_dir / "msd-musicnn-1.pb"),
                output="model/dense/BiasAdd"
            )
        except Exception as e:
            logger.warning(f"Failed to load MusiCNN embedder: {e}")

        # Load classification models
        self._load_model("msd-musicnn-1", "_msd_model", es.TensorflowPredict2D)
        self._load_model("mtg_jamendo_genre-discogs-effnet-1", "_genre_model", es.TensorflowPredict2D)
        self._load_model("mtg_jamendo_moodtheme-discogs-effnet-1", "_mood_model", es.TensorflowPredict2D)
        self._load_model("mtg_jamendo_instrument-discogs-effnet-1", "_instrument_model", es.TensorflowPredict2D)
        self._load_model("voice_instrumental-discogs-effnet-1", "_voice_model", es.TensorflowPredict2D)
        self._load_model("danceability-discogs-effnet-1", "_danceability_model", es.TensorflowPredict2D)
        self._load_model("emomusic-msd-musicnn-2", "_emomusic_model", es.TensorflowPredict2D)

        self._models_loaded = True
        logger.info("All Essentia TensorFlow models loaded successfully")

    def _load_model(self, model_name: str, attr_name: str, model_class):
        """Load a single TensorFlow model."""
        pb_path = self.models_dir / f"{model_name}.pb"
        if not pb_path.exists():
            logger.warning(f"Model not found: {pb_path}")
            return

        try:
            setattr(self, attr_name, model_class(graphFilename=str(pb_path)))
            self._load_metadata(model_name)
            logger.debug(f"Loaded model: {model_name}")
        except Exception as e:
            logger.error(f"Failed to load model {model_name}: {e}")

    def tag(
        self,
        audio_path: str,
        top_n: int = 10,
        detect_genres: bool = True,
        detect_moods: bool = True,
        detect_instruments: bool = True,
        detect_vocals: bool = True,
        detect_danceability: bool = True,
        detect_arousal_valence: bool = True,
    ) -> Dict[str, Any]:
        """Analyze audio file for tags, genres, moods, and other classifications.

        Args:
            audio_path: Path to the audio file
            top_n: Number of top tags to return for each category
            detect_genres: Whether to detect genre classifications
            detect_moods: Whether to detect mood/theme classifications
            detect_instruments: Whether to detect instrument classifications
            detect_vocals: Whether to detect voice/instrumental classification
            detect_danceability: Whether to detect danceability score
            detect_arousal_valence: Whether to detect arousal/valence (energy/mood)

        Returns:
            Dictionary containing tag analysis results
        """
        self._ensure_models_loaded()

        path = Path(audio_path)
        if not path.exists():
            raise FileNotFoundError(f"Audio file not found: {audio_path}")

        logger.info(f"Tagging audio: {audio_path}")

        # Load audio at 16kHz for TensorFlow models
        audio = es.MonoLoader(filename=str(path), sampleRate=self.sample_rate)()

        result: Dict[str, Any] = {}

        # Get embeddings
        effnet_embeddings = None
        musicnn_embeddings = None

        if self._discogs_embedder:
            try:
                effnet_embeddings = self._discogs_embedder(audio)
            except Exception as e:
                logger.warning(f"EffNet embedding extraction failed: {e}")

        if self._musicnn_embedder:
            try:
                musicnn_embeddings = self._musicnn_embedder(audio)
            except Exception as e:
                logger.warning(f"MusiCNN embedding extraction failed: {e}")

        # MagnaTagATune (top general tags)
        if musicnn_embeddings is not None:
            result["top_tags"] = self._get_top_tags(
                musicnn_embeddings, "msd-musicnn-1", top_n
            )

        # Genre classification
        if detect_genres and effnet_embeddings is not None:
            result["genres"] = self._classify(
                effnet_embeddings, "_genre_model", "mtg_jamendo_genre-discogs-effnet-1", top_n
            )

        # Mood/Theme classification
        if detect_moods and effnet_embeddings is not None:
            result["moods"] = self._classify(
                effnet_embeddings, "_mood_model", "mtg_jamendo_moodtheme-discogs-effnet-1", top_n
            )

        # Instrument classification
        if detect_instruments and effnet_embeddings is not None:
            result["instruments"] = self._classify(
                effnet_embeddings, "_instrument_model", "mtg_jamendo_instrument-discogs-effnet-1", top_n
            )

        # Voice/Instrumental detection
        if detect_vocals and effnet_embeddings is not None:
            vocal_result = self._classify_binary(
                effnet_embeddings, "_voice_model", "voice_instrumental-discogs-effnet-1"
            )
            if vocal_result is not None:
                result["has_vocals"] = vocal_result.get("has_vocals", False)
                result["vocal_confidence"] = vocal_result.get("confidence", 0.0)

        # Danceability detection
        if detect_danceability and effnet_embeddings is not None:
            dance_result = self._classify_binary(
                effnet_embeddings, "_danceability_model", "danceability-discogs-effnet-1"
            )
            if dance_result is not None:
                result["is_danceable"] = dance_result.get("is_danceable", False)
                result["danceability_confidence"] = dance_result.get("confidence", 0.0)

        # Arousal/Valence (energy/mood dimensions)
        if detect_arousal_valence and musicnn_embeddings is not None:
            av_result = self._get_arousal_valence(musicnn_embeddings)
            if av_result:
                result["arousal"] = av_result.get("arousal", 0.5)
                result["valence"] = av_result.get("valence", 0.5)

        return result

    def _get_top_tags(
        self, embeddings: np.ndarray, model_name: str, top_n: int
    ) -> List[Dict[str, Any]]:
        """Get top tags from MusiCNN embeddings."""
        metadata = self._load_metadata(model_name)
        classes = metadata.get("classes", [])

        if len(embeddings) == 0 or len(classes) == 0:
            return []

        # Average across time dimension
        mean_activations = np.mean(embeddings, axis=0)

        # Get top N indices
        top_indices = np.argsort(mean_activations)[::-1][:top_n]

        return [
            {
                "name": classes[i] if i < len(classes) else f"tag_{i}",
                "confidence": float(mean_activations[i])
            }
            for i in top_indices
            if mean_activations[i] > 0.1  # Filter low confidence
        ]

    def _classify(
        self, embeddings: np.ndarray, model_attr: str, model_name: str, top_n: int
    ) -> List[str]:
        """Classify using a TensorFlow model and return top class names."""
        model = getattr(self, model_attr)
        if model is None:
            return []

        metadata = self._load_metadata(model_name)
        classes = metadata.get("classes", [])

        try:
            predictions = model(embeddings)
            mean_predictions = np.mean(predictions, axis=0)
            top_indices = np.argsort(mean_predictions)[::-1][:top_n]

            return [
                classes[i] for i in top_indices
                if i < len(classes) and mean_predictions[i] > 0.1
            ]
        except Exception as e:
            logger.warning(f"Classification failed for {model_name}: {e}")
            return []

    def _classify_binary(
        self, embeddings: np.ndarray, model_attr: str, model_name: str
    ) -> Optional[Dict[str, Any]]:
        """Binary classification (voice/instrumental, danceability)."""
        model = getattr(self, model_attr)
        if model is None:
            return None

        metadata = self._load_metadata(model_name)
        classes = metadata.get("classes", ["negative", "positive"])

        try:
            predictions = model(embeddings)
            mean_predictions = np.mean(predictions, axis=0)

            # Binary classification - higher value for positive class
            positive_idx = 1 if len(classes) > 1 else 0
            confidence = float(mean_predictions[positive_idx])
            is_positive = confidence > 0.5

            # Map to specific field names based on model
            if "voice" in model_name:
                return {"has_vocals": is_positive, "confidence": confidence}
            elif "danceability" in model_name:
                return {"is_danceable": is_positive, "confidence": confidence}
            else:
                return {"is_positive": is_positive, "confidence": confidence}
        except Exception as e:
            logger.warning(f"Binary classification failed for {model_name}: {e}")
            return None

    def _get_arousal_valence(self, embeddings: np.ndarray) -> Optional[Dict[str, float]]:
        """Get arousal (energy) and valence (mood positivity) scores."""
        if self._emomusic_model is None:
            return None

        try:
            predictions = self._emomusic_model(embeddings)
            mean_predictions = np.mean(predictions, axis=0)

            # Emomusic model outputs arousal and valence as two dimensions
            # Arousal: 0 = calm, 1 = energetic
            # Valence: 0 = negative/sad, 1 = positive/happy
            arousal = float(mean_predictions[0]) if len(mean_predictions) > 0 else 0.5
            valence = float(mean_predictions[1]) if len(mean_predictions) > 1 else 0.5

            # Clamp to [0, 1] range
            arousal = max(0.0, min(1.0, arousal))
            valence = max(0.0, min(1.0, valence))

            return {"arousal": arousal, "valence": valence}
        except Exception as e:
            logger.warning(f"Arousal/valence extraction failed: {e}")
            return None


# Global tagger instance (lazy initialized)
_tagger: Optional[AudioTagger] = None


def get_tagger(models_dir: str = "/app/models") -> AudioTagger:
    """Get or create the global tagger instance.

    Args:
        models_dir: Directory containing model files

    Returns:
        AudioTagger instance
    """
    global _tagger
    if _tagger is None:
        _tagger = AudioTagger(models_dir=models_dir)
    return _tagger
