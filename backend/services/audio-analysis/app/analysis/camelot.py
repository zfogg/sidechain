"""Camelot wheel notation conversion utilities."""

# Camelot wheel mapping for DJ mixing compatibility
# Minor keys use 'A' suffix, Major keys use 'B' suffix
CAMELOT_MAP = {
    # Minor keys (A)
    ("A", "minor"): "8A",
    ("E", "minor"): "9A",
    ("B", "minor"): "10A",
    ("F#", "minor"): "11A",
    ("Gb", "minor"): "11A",
    ("C#", "minor"): "12A",
    ("Db", "minor"): "12A",
    ("G#", "minor"): "1A",
    ("Ab", "minor"): "1A",
    ("D#", "minor"): "2A",
    ("Eb", "minor"): "2A",
    ("A#", "minor"): "3A",
    ("Bb", "minor"): "3A",
    ("F", "minor"): "4A",
    ("C", "minor"): "5A",
    ("G", "minor"): "6A",
    ("D", "minor"): "7A",
    # Major keys (B)
    ("C", "major"): "8B",
    ("G", "major"): "9B",
    ("D", "major"): "10B",
    ("A", "major"): "11B",
    ("E", "major"): "12B",
    ("B", "major"): "1B",
    ("F#", "major"): "2B",
    ("Gb", "major"): "2B",
    ("C#", "major"): "3B",
    ("Db", "major"): "3B",
    ("G#", "major"): "4B",
    ("Ab", "major"): "4B",
    ("D#", "major"): "5B",
    ("Eb", "major"): "5B",
    ("A#", "major"): "6B",
    ("Bb", "major"): "6B",
    ("F", "major"): "7B",
}


def to_camelot(key: str, scale: str) -> str:
    """Convert a key and scale to Camelot wheel notation.

    Args:
        key: Root note (e.g., 'A', 'F#', 'Bb')
        scale: 'major' or 'minor'

    Returns:
        Camelot notation (e.g., '8A', '11B') or empty string if not found
    """
    return CAMELOT_MAP.get((key, scale.lower()), "")


def normalize_key_name(key: str) -> str:
    """Normalize key name from Essentia output.

    Essentia may return keys like 'A' or 'F#' - this ensures consistent naming.

    Args:
        key: Raw key name from Essentia

    Returns:
        Normalized key name
    """
    # Essentia returns clean key names, but ensure consistency
    key = key.strip()

    # Handle enharmonic equivalents (prefer sharps)
    enharmonic_map = {
        "Db": "C#",
        "Eb": "D#",
        "Gb": "F#",
        "Ab": "G#",
        "Bb": "A#",
    }

    return enharmonic_map.get(key, key)
