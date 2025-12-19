#pragma once

#include <cstddef>

// ==============================================================================
/**
 * Constants - Centralized magic numbers for the Sidechain plugin
 *
 * This header consolidates configuration values that were previously
 * scattered throughout the codebase. Groupings:
 *   - Endpoints: API URLs for different environments
 *   - Api: Network timeouts, retry limits
 *   - Audio: Recording limits, BPM ranges, cache sizes
 *   - UI: Component dimensions, corner radii, avatar sizes
 *   - Cache: Image and audio cache limits
 *
 * Usage:
 *   #include "util/Constants.h"
 *
 *   // API URLs
 *   juce::URL url(Constants::Endpoints::DEV_BASE_URL + "/api/v1/auth/login");
 *
 *   // API timeouts
 *   .withConnectionTimeoutMs(Constants::Api::DEFAULT_TIMEOUT_MS)
 *
 *   // UI dimensions
 *   setSize(width, Constants::Ui::HEADER_HEIGHT)
 */
namespace Constants {
// ==========================================================================
// API Endpoints
// ==========================================================================
namespace Endpoints {
// Development server (local)
constexpr const char *DEV_BASE_URL = "http://localhost:8787";

// Production server
constexpr const char *PROD_BASE_URL = "https://api.sidechain.app";

// WebSocket endpoints
constexpr const char *DEV_WS_HOST = "localhost";
constexpr int DEV_WS_PORT = 8787;
constexpr const char *PROD_WS_HOST = "api.sidechain.app";
constexpr int PROD_WS_PORT = 443;
constexpr const char *WS_PATH = "/api/v1/ws";

// API version prefix
constexpr const char *API_VERSION = "/api/v1";

// Auth endpoints
constexpr const char *AUTH_LOGIN = "/api/v1/auth/login";
constexpr const char *AUTH_REGISTER = "/api/v1/auth/register";
constexpr const char *AUTH_OAUTH_POLL = "/api/v1/auth/oauth/poll";

// User endpoints
constexpr const char *USERS_PROFILE_PICTURE = "/api/v1/users/upload-profile-picture";
} // namespace Endpoints

// ==========================================================================
// Error Messages
// ==========================================================================
namespace Errors {
// Authentication errors
constexpr const char *NOT_AUTHENTICATED = "Not authenticated";
constexpr const char *AUTH_REQUIRED = "Authentication required";

// Network errors
constexpr const char *NETWORK_ERROR = "Network error occurred";
constexpr const char *REQUEST_FAILED = "Request failed";
constexpr const char *TIMEOUT = "Request timed out";

// Validation errors
constexpr const char *INVALID_INPUT = "\xe2\x9d\x8c Invalid input";
constexpr const char *MISSING_REQUIRED_FIELD = "\xe2\x9b\x94 Missing required field";
} // namespace Errors
// ==========================================================================
// API Configuration
// ==========================================================================
namespace Api {
// Default HTTP request timeout (30 seconds)
constexpr int DEFAULT_TIMEOUT_MS = 30000;

// Image download timeout (shorter for better UX)
constexpr int IMAGE_TIMEOUT_MS = 10000;

// Quick operations (profile pic in header)
constexpr int QUICK_TIMEOUT_MS = 5000;

// Large upload timeout (audio files)
constexpr int UPLOAD_TIMEOUT_MS = 60000;

// Maximum upload size in bytes (50MB)
constexpr size_t MAX_UPLOAD_SIZE_BYTES = 50 * 1024 * 1024;

// Maximum retries for failed requests
constexpr int MAX_RETRIES = 3;

// Retry delay base (exponential backoff)
constexpr int RETRY_DELAY_BASE_MS = 1000;

// WebSocket connect timeout
constexpr int WEBSOCKET_TIMEOUT_MS = 10000;

// Number of redirects to follow
constexpr int MAX_REDIRECTS = 5;
} // namespace Api

// ==========================================================================
// Audio Configuration
// ==========================================================================
namespace Audio {
// Maximum recording duration in seconds
constexpr double MAX_RECORDING_SECONDS = 60.0;

// Default BPM when none detected
constexpr double DEFAULT_BPM = 120.0;

// Valid BPM range
constexpr double MIN_BPM = 20.0;
constexpr double MAX_BPM = 300.0;

// Target loudness for normalization (LUFS)
constexpr float TARGET_LUFS = -14.0f;

// Audio player cache size in bytes (50MB)
constexpr size_t AUDIO_CACHE_SIZE_BYTES = 50 * 1024 * 1024;

// Default sample rate
constexpr double DEFAULT_SAMPLE_RATE = 44100.0;

// Number of channels for recording
constexpr int RECORDING_CHANNELS = 2;
} // namespace Audio

// ==========================================================================
// UI Dimensions
// ==========================================================================
namespace Ui {
// Header component height
constexpr int HEADER_HEIGHT = 60;

// Feed post card height
constexpr int POST_CARD_HEIGHT = 120;

// Comment row heights
constexpr int COMMENT_ROW_HEIGHT = 70;
constexpr int REPLY_ROW_HEIGHT = 60;

// Follower/Following list row height
constexpr int USER_ROW_HEIGHT = 70;

// Avatar sizes
constexpr int AVATAR_SIZE_LARGE = 50;
constexpr int AVATAR_SIZE_MEDIUM = 40;
constexpr int AVATAR_SIZE_SMALL = 32;

// Standard corner radii
constexpr float CORNER_RADIUS_LARGE = 12.0f;
constexpr float CORNER_RADIUS_MEDIUM = 8.0f;
constexpr float CORNER_RADIUS_SMALL = 4.0f;
constexpr float CORNER_RADIUS_PILL = 14.0f; // For pill-shaped buttons

// Badge heights
constexpr int BADGE_HEIGHT = 18;
constexpr int GENRE_CHIP_HEIGHT = 24;

// Standard spacing
constexpr int SPACING_XS = 4;
constexpr int SPACING_SM = 8;
constexpr int SPACING_MD = 12;
constexpr int SPACING_LG = 16;
constexpr int SPACING_XL = 24;

// Default border width
constexpr float BORDER_WIDTH = 1.0f;

// Default font sizes
constexpr float FONT_SIZE_XS = 10.0f;
constexpr float FONT_SIZE_SM = 12.0f;
constexpr float FONT_SIZE_MD = 14.0f;
constexpr float FONT_SIZE_LG = 16.0f;
constexpr float FONT_SIZE_XL = 20.0f;
constexpr float FONT_SIZE_TITLE = 24.0f;
} // namespace Ui

// ==========================================================================
// Cache Configuration
// ==========================================================================
namespace Cache {
// Maximum images to keep in LRU cache
constexpr size_t IMAGE_CACHE_MAX_ITEMS = 100;

// Audio preview cache item limit
constexpr size_t AUDIO_CACHE_MAX_ITEMS = 10;

// Feed data cache duration (milliseconds)
constexpr int FEED_CACHE_DURATION_MS = 60000; // 1 minute

// Profile cache duration (milliseconds)
constexpr int PROFILE_CACHE_DURATION_MS = 300000; // 5 minutes
} // namespace Cache

// ==========================================================================
// Pagination
// ==========================================================================
namespace Pagination {
// Default page size for feed
constexpr int FEED_PAGE_SIZE = 20;

// Default page size for comments
constexpr int COMMENTS_PAGE_SIZE = 20;

// Default page size for followers/following lists
constexpr int FOLLOW_LIST_PAGE_SIZE = 20;

// Default page size for user search
constexpr int SEARCH_PAGE_SIZE = 10;
} // namespace Pagination

// ==========================================================================
// Animation & Timing
// ==========================================================================
namespace Timing {
// Debounce delay for search input
constexpr int SEARCH_DEBOUNCE_MS = 300;

// Auto-refresh interval for feed
constexpr int FEED_REFRESH_INTERVAL_MS = 60000; // 1 minute

// Timer interval for progress updates
constexpr int PROGRESS_UPDATE_INTERVAL_MS = 100;

// Tooltip show delay
constexpr int TOOLTIP_DELAY_MS = 500;

// Animation duration
constexpr int ANIMATION_DURATION_MS = 200;
} // namespace Timing

// ==========================================================================
// Validation Limits
// ==========================================================================
namespace Limits {
// Username limits
constexpr int USERNAME_MIN_LENGTH = 3;
constexpr int USERNAME_MAX_LENGTH = 30;

// Display name limits
constexpr int DISPLAY_NAME_MAX_LENGTH = 50;

// Bio limits
constexpr int BIO_MAX_LENGTH = 500;

// Comment limits
constexpr int COMMENT_MAX_LENGTH = 1000;

// Post title limits
constexpr int POST_TITLE_MAX_LENGTH = 100;

// Post description limits
constexpr int POST_DESCRIPTION_MAX_LENGTH = 500;
} // namespace Limits

} // namespace Constants
