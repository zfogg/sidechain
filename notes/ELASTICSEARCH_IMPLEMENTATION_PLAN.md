# Elasticsearch Full-Stack Implementation Plan

**Status**: TODO
**Priority**: High (Core Infrastructure)
**Scope**: Backend (Go), Plugin (C++/JUCE), Web (React), CLI (Go)
**Timeline**: 2-3 weeks for complete implementation

> Activate the Elasticsearch infrastructure that's already built but unused. Implement professional, polished search across all surfaces of the application.

Current Limitations to fix:
  - ❌ No fuzzy matching (typos fail silently)
  - ❌ No stemming (searching "producing" won't find "produce")
  - ❌ No tokenization (searches entire fields as one token)
  - ❌ No autocomplete suggestions
  - ❌ No complex ranking algorithms
  - ❌ Will slow down when you hit ~100k posts or ~50k users
  - ❌ No full-text search capabilities

Immediate Benefits (Once Wired Up)

  | Feature                   | Current (PostgreSQL) | With Elasticsearch                    |
  |---------------------------|----------------------|---------------------------------------|
  | Typo Tolerance            | ❌ None              | ✅ Fuzzy matching (1-2 char distance) |
  | Stemming                  | ❌ No                | ✅ "producing" finds "produce"        |
  | Autocomplete              | ❌ No                | ✅ Prefix completion suggestions      |
  | Ranking                   | ✅ Follower count    | ✅ Relevance + engagement + recency   |
  | Performance (100k+ posts) | ⚠️ Slows down        | ✅ O(log n) with indexes              |
  | Complex Filters           | ⚠️ Limited           | ✅ Genre/BPM/Key/DAW simultaneously   |
  | Search Analytics          | ❌ Stubbed           | ✅ Query patterns, failed searches    |

---

## 📋 Phase 0: Backend Foundation (Core Wiring)

### 0.1 Initialize Search Client in Main
- [ ] **File**: `backend/cmd/server/main.go`
- [ ] Add Elasticsearch client initialization after database setup
- [ ] Create indices on server startup (users, posts, stories)
- [ ] Handle initialization errors gracefully (log but don't crash if ES unavailable)
- [ ] Add clean shutdown hook to close search client
- **Acceptance**: `make backend-dev` starts without ES errors

### 0.2 Add Search Client to Handlers Struct
- [ ] **File**: `backend/internal/handlers/handlers.go`
- [ ] Add `search *search.Client` field to `Handlers` struct
- [ ] Update `NewHandlers()` function to accept search client parameter
- [ ] Update all handler instantiation sites to pass search client
- **Acceptance**: Code compiles without errors

### 0.3 Initial Index Syncing on User Creation
- [ ] **File**: `backend/internal/handlers/auth.go` (user creation)
- [ ] After user signup/creation, call `h.search.IndexUser(ctx, user.ID, user)`
- [ ] Log errors but don't fail the request (search is non-critical)
- [ ] Test: Create new user, verify it appears in Elasticsearch
- **Acceptance**: Can search for newly created users

### 0.4 Initial Index Syncing on Post Creation
- [ ] **File**: `backend/internal/handlers/posts.go`
- [ ] After post creation, call `h.search.IndexPost(ctx, post.ID, post)`
- [ ] Extract post metadata: genre, BPM, key, DAW from audio metadata
- [ ] Log indexing errors but don't fail POST request
- **Acceptance**: New posts appear in search results

### 0.5 Initial Index Syncing on Story Creation
- [ ] **File**: `backend/internal/handlers/stories.go`
- [ ] Add story index to search client (if not already defined)
- [ ] Story mapping: id, user_id, username, created_at, expires_at, media_url
- [ ] After story creation, call `h.search.IndexStory(ctx, story.ID, story)`
- **Acceptance**: Stories can be searched

### 0.6 Add Index Syncing to Post Updates
- [ ] **File**: `backend/internal/handlers/posts.go` (update endpoints)
- [ ] When post metadata updates (like count, play count, comment count), update ES index
- [ ] Call `h.search.UpdatePost(ctx, postID, updatedFields)`
- [ ] Include updated engagement metrics in index
- **Acceptance**: Updated posts reflect changes in search results

### 0.7 Add Index Deletion on Entity Deletion
- [ ] **Files**: `auth.go`, `posts.go`, `stories.go` (delete handlers)
- [ ] When users are deleted: `h.search.DeleteUser(ctx, userID)`
- [ ] When posts are deleted: `h.search.DeletePost(ctx, postID)`
- [ ] When stories are deleted: `h.search.DeleteStory(ctx, storyID)`
- [ ] Handle "not found" gracefully (already in client code)
- **Acceptance**: Deleted items don't appear in search results

### 0.8 Add Migration Job for Existing Data
- [ ] **File**: `backend/cmd/migrate/main.go` (add new migration command)
- [ ] Create `index-elasticsearch` command to backfill indices from database
- [ ] Fetch all users, posts, stories from DB and index them
- [ ] Show progress bar (total count, current progress)
- [ ] Handle partial failures (log, continue, report summary)
- [ ] Command: `go run cmd/migrate/main.go index-elasticsearch`
- **Acceptance**: All existing data indexed without errors

---

## 🔍 Phase 1: Backend Search Endpoints

### 1.1 Replace User Search Endpoint
- [ ] **File**: `backend/internal/handlers/discovery.go` - `SearchUsers()`
- [ ] Current: PostgreSQL ILIKE matching
- [ ] Replace: Use `h.search.SearchUsers(ctx, query, limit, offset)`
- [ ] Keep same API contract (same request/response format)
- [ ] Add optional filters: genre, follower_count_min, joined_after
- [ ] Return search metadata: query time, total results, search matched fields
- [ ] Test: `make test-backend` - discovery tests pass
- **Acceptance**: `/api/v1/search/users?q=indie` returns ES results with same format

### 1.2 Replace Post Search Endpoint
- [ ] **File**: `backend/internal/handlers/discovery.go` - `SearchPosts()`
- [ ] Current: PostgreSQL query with JOINs
- [ ] Replace: Use `h.search.SearchPosts(ctx, params)`
- [ ] Support filters: genre (array), bpm_min/bpm_max, key, daw, created_after
- [ ] Implement multi-field search: post_title, username, genre tags
- [ ] Use function_score ranking:
  - Base relevance from text match
  - Like count (weight: 3.0, log1p)
  - Play count (weight: 1.0, log1p)
  - Comment count (weight: 2.0, log1p)
  - Recency (30-day exponential decay, weight: 0.5)
- [ ] Test: `make test-backend` - post search tests pass
- **Acceptance**: `/api/v1/search/posts?q=techno&genre=techno&bpm_min=120&bpm_max=130` returns ranked results

### 1.3 Add Story Search Endpoint
- [ ] **File**: `backend/internal/handlers/stories.go` - new `SearchStories()`
- [ ] Endpoint: `GET /api/v1/search/stories?q=query&username=producer&created_after=2025-12-10`
- [ ] Search fields: username, story text/description, created_at
- [ ] Filter by: username, creation date range, not expired
- [ ] Don't return expired stories (check expires_at)
- [ ] Sort by: recency (newest first), then relevance
- [ ] Test endpoint manually before integration
- **Acceptance**: Can search for stories with date range filter

### 1.4 Add Autocomplete Endpoint for Users
- [ ] **File**: `backend/internal/handlers/discovery.go` - new `SuggestUsers()`
- [ ] Endpoint: `GET /api/v1/search/users/suggest?q=indi`
- [ ] Uses ES completion suggester on `username.suggest` field
- [ ] Returns top 10 suggestions by default
- [ ] Response: `[{"username": "indie_producer", "follower_count": 234}]`
- [ ] Used for: autocomplete dropdown in search bars
- [ ] Test with various prefix inputs
- **Acceptance**: Typing "indi" suggests "indie_producer"

### 1.5 Add Autocomplete Endpoint for Genre
- [ ] **File**: `backend/internal/handlers/discovery.go` - new `SuggestGenres()`
- [ ] Endpoint: `GET /api/v1/search/genres/suggest?q=tech`
- [ ] Returns list of genres matching prefix: ["techno", "tech_house", "tech_pop"]
- [ ] Sorted by frequency in posts (most used first)
- [ ] Limit: 20 suggestions
- [ ] Implementation: Aggregate on genre field, order by _count desc
- **Acceptance**: Autocomplete dropdown suggests relevant genres

### 1.6 Add Advanced Search Endpoint
- [ ] **File**: `backend/internal/handlers/discovery.go` - new `AdvancedSearch()`
- [ ] Endpoint: `POST /api/v1/search/advanced` (complex query support)
- [ ] Request body:
  ```json
  {
    "query": "string to search",
    "type": "posts|users|stories|all",
    "filters": {
      "genres": ["techno", "house"],
      "bpm_min": 120,
      "bpm_max": 130,
      "daw": "ableton",
      "created_after": "2025-12-01",
      "engagement": {
        "min_likes": 5,
        "min_plays": 10
      }
    },
    "sort": "relevance|recency|popularity",
    "limit": 20,
    "offset": 0
  }
  ```
- [ ] Response: Unified results across entity types with metadata
- [ ] Return search execution stats (query time, total matched)
- [ ] Test: Complex multi-filter queries
- **Acceptance**: Can search across posts+users+stories with complex filters

### 1.7 Implement Search Analytics Tracking
- [ ] **File**: `backend/internal/search/client.go` - implement `TrackSearchQuery()`
- [ ] Current: Stub that prints to console
- [ ] Real implementation: Index to `search_analytics` index
- [ ] Track: query, result_count, filters_used, user_id, timestamp, search_type
- [ ] Add to all search endpoints (SearchUsers, SearchPosts, SearchStories, Advanced)
- [ ] Call after search completes (even if 0 results)
- [ ] Implement `TrackSearchAnalytics()` helper function
- **Acceptance**: Search queries logged to Elasticsearch analytics index

### 1.8 Add Search Error Recovery
- [ ] **File**: `backend/internal/handlers/discovery.go` (all search handlers)
- [ ] If Elasticsearch unavailable, fallback to PostgreSQL
- [ ] Log warning when fallback happens
- [ ] Return search results with `fallback: true` flag in response
- [ ] Don't expose ES errors to client
- [ ] Test by disabling ES container and verifying fallback
- **Acceptance**: Search still works if Elasticsearch is down

---

## 🎵 Phase 2: Plugin Search UI (C++/JUCE)

### 2.1 Create Search Component Infrastructure
- [ ] **File**: `plugin/src/ui/search/SearchComponent.h/cpp` (new)
- [ ] Inherit from `juce::Component`
- [ ] Layout: Search bar (top), results list (scrollable), filters sidebar (optional)
- [ ] Search bar: Text input with real-time suggestions
- [ ] Results list: Dynamic scrolling with lazy loading
- [ ] Filters: Genre checkboxes, BPM range slider, DAW selector
- **Acceptance**: Component renders without errors in plugin

### 2.2 Create Search Results Item Component
- [ ] **File**: `plugin/src/ui/search/SearchResultItem.h/cpp` (new)
- [ ] For users: Avatar, username, follower count, follow button
- [ ] For posts: Author avatar/username, waveform, genre, BPM, play/like buttons
- [ ] For stories: Author avatar, expiration timer, preview image, click to view
- [ ] Click handling: Navigate to post/user/story detail
- [ ] Hover effects: Highlight, subtle shadow
- **Acceptance**: All 3 entity types render correctly

### 2.3 Integrate Search Networking
- [ ] **File**: `plugin/src/network/NetworkClient.cpp` - add search methods
- [ ] Add methods:
  - `searchUsers(query, limit, offset)`
  - `searchPosts(query, filters, limit, offset)`
  - `searchStories(query, limit, offset)`
  - `suggestUsers(prefix)`
  - `suggestGenres(prefix)`
  - `advancedSearch(complexQuery)`
- [ ] All return proper error messages via callbacks
- [ ] Handle network timeouts gracefully
- **Acceptance**: All search endpoints callable from plugin

### 2.4 Add Autocomplete Dropdown
- [ ] **File**: `plugin/src/ui/search/AutocompleteDropdown.h/cpp` (new)
- [ ] Shows suggestions as user types in search bar
- [ ] Updates on each keystroke (debounced 300ms)
- [ ] Max 10 suggestions displayed
- [ ] Select suggestion: Populate search bar and execute search
- [ ] Arrow keys to navigate, Enter to select
- **Acceptance**: Typing "indi" shows autocomplete suggestions

### 2.5 Add Filter UI Controls
- [ ] **File**: `plugin/src/ui/search/SearchFiltersPanel.h/cpp` (new)
- [ ] Collapsible filters section
- [ ] Genre multi-select (checkboxes)
- [ ] BPM range slider (0-300 BPM)
- [ ] DAW selector (Ableton, FL Studio, Logic, etc.)
- [ ] Creation date range (last day/week/month/all)
- [ ] Engagement filter (min likes, min plays)
- [ ] "Apply Filters" button triggers new search
- **Acceptance**: Can set filters and see filtered results

### 2.6 Implement Search Pagination
- [ ] **File**: `plugin/src/ui/search/SearchComponent.cpp`
- [ ] "Load More" button at bottom of results
- [ ] Incremental loading: Load 20 results, then 20 more
- [ ] Preserve scroll position when loading more
- [ ] Loading indicator while fetching
- [ ] Handle end of results (no more button)
- **Acceptance**: Can scroll through 100+ results without lag

### 2.7 Wire Up Search Tab in Main UI
- [ ] **File**: `plugin/src/ui/MainTabbedComponent.h/cpp`
- [ ] Add "Search" tab between Feed and Profile (or in header)
- [ ] Tab icon: Magnifying glass
- [ ] Contains SearchComponent from 2.1
- [ ] Navigation: Click on result → view detail → back to search
- [ ] Maintain search state when switching tabs
- **Acceptance**: Can click Search tab and use all functionality

### 2.8 Add Recent Searches History
- [ ] **File**: `plugin/src/ui/search/SearchComponent.cpp`
- [ ] Store last 10 searches locally in plugin settings
- [ ] Show "Recent Searches" when search bar is empty
- [ ] Click to re-run search
- [ ] Clear history button
- [ ] Implementation: JSON array in plugin preferences
- **Acceptance**: Recent searches persist across plugin restarts

### 2.9 Add Search to Header Quick Search
- [ ] **File**: `plugin/src/ui/header/HeaderComponent.cpp`
- [ ] Small search icon in header (right side)
- [ ] Click → opens search modal/panel
- [ ] Search bar focused by default
- [ ] Quick filter toggles (Users / Posts / Stories)
- [ ] Results preview (3 top results)
- [ ] "View More Results" → Full search interface
- **Acceptance**: Can do quick search from anywhere in plugin

### 2.10 Add Search Event Analytics
- [ ] **File**: `plugin/src/ui/search/SearchComponent.cpp`
- [ ] Track: searches performed, results clicked, filters applied
- [ ] Send to backend analytics endpoint
- [ ] Include: query, result_count, user_id, timestamp, entity_type
- [ ] Use existing analytics pipeline
- **Acceptance**: Search usage logged to backend

---

## 🌐 Phase 3: Web Frontend Search (React)

### 3.1 Create Search Page Component
- [ ] **File**: `web/src/pages/SearchPage.tsx` (new)
- [ ] Route: `/search?q=query&type=all`
- [ ] Layout: Search bar (top), filters sidebar (left), results grid (right)
- [ ] Search bar: Text input with real-time suggestions
- [ ] Results displayed in grid (3 columns on desktop, responsive)
- **Acceptance**: Navigate to `/search?q=test` and see results

### 3.2 Create Search Results Display Components
- [ ] **File**: `web/src/components/search/UserResult.tsx` (new)
- [ ] Display: Avatar, username, follower count, follow button, bio preview
- [ ] Click: Navigate to user profile
- [ ] **File**: `web/src/components/search/PostResult.tsx` (new)
- [ ] Display: Waveform preview, artist name, genre, BPM, play/like count, play button
- [ ] Click: Navigate to post detail
- [ ] **File**: `web/src/components/search/StoryResult.tsx` (new)
- [ ] Display: Artist avatar, story preview image, "expires in X hours", play button
- [ ] Click: Open story viewer
- **Acceptance**: All 3 result types render correctly

### 3.3 Add Search Filters Component
- [ ] **File**: `web/src/components/search/SearchFilters.tsx` (new)
- [ ] Genre multi-select (tags or checkbox list)
- [ ] BPM range slider (double-handle slider)
- [ ] DAW selector (dropdown/multi-select)
- [ ] Date range picker (last 24h / week / month / all)
- [ ] Engagement threshold (min likes, min plays)
- [ ] "Clear Filters" button
- [ ] "Apply Filters" button
- [ ] Real-time URL sync: `/search?q=test&genre=techno&bpm_min=120`
- **Acceptance**: Filters update URL and results in real-time

### 3.4 Implement Search API Hook
- [ ] **File**: `web/src/hooks/useSearch.ts` (new)
- [ ] Hook: `useSearch(query, filters, page, sortBy)`
- [ ] Returns: `{results, loading, error, totalCount, hasMore}`
- [ ] Handles: Debouncing, caching, pagination
- [ ] Calls backend: `/api/v1/search/advanced` endpoint
- [ ] Supports: All entity types, complex filters
- **Acceptance**: Can use hook in any component

### 3.5 Add Search Autocomplete
- [ ] **File**: `web/src/components/search/SearchAutocomplete.tsx` (new)
- [ ] Dropdown suggestions as user types
- [ ] Three categories: Users, Posts (by title), Genres
- [ ] Show profile picture, engagement metrics for each
- [ ] Click suggestion: Navigate or auto-fill search
- [ ] Debounce: 300ms
- **Acceptance**: Typing shows relevant suggestions

### 3.6 Implement Search Pagination
- [ ] **File**: `web/src/components/search/SearchResults.tsx`
- [ ] Display results in infinite scroll or pagination buttons
- [ ] "Load More" button or auto-load on scroll
- [ ] Show: "Showing 1-20 of 234 results"
- [ ] Loading state while fetching
- [ ] **Acceptance**: Can paginate through 100+ results

### 3.7 Add Search to Navigation
- [ ] **File**: `web/src/components/Header.tsx`
- [ ] Search bar in header
- [ ] Click search icon or focus bar → navigate to `/search`
- [ ] Pre-populate with typed query
- [ ] Link to advanced search
- **Acceptance**: Can access search from any page

### 3.8 Create Advanced Search Page
- [ ] **File**: `web/src/pages/AdvancedSearchPage.tsx` (new)
- [ ] Route: `/search/advanced`
- [ ] Complex query builder UI
- [ ] Visual filters for all supported criteria
- [ ] Query preview (what ES query will run)
- [ ] Save searches (favorite queries)
- [ ] **Acceptance**: Can construct and execute complex queries

### 3.9 Add Search Analytics
- [ ] **File**: `web/src/hooks/useSearch.ts`
- [ ] Track: searches performed, results viewed, filters applied
- [ ] Send to backend: `/api/v1/analytics/search`
- [ ] Include: query, filters, result_count, user_id, timestamp
- [ ] **Acceptance**: Search usage visible in analytics

### 3.10 Implement Search Result Metadata Display
- [ ] Show search execution stats in UI
- [ ] Display: "Found 234 results in 0.4s"
- [ ] Highlight matched fields (bold matching text)
- [ ] Show relevance score (visual indicator: 1-5 stars)
- [ ] Display ranking reason tooltip (why this result ranked high)
- **Acceptance**: User sees detailed search result information

---

## 🖥️ Phase 4: CLI Admin Tool

### 4.1 Add Search Index Management Commands
- [ ] **File**: `backend/cmd/cli/main.go` (or existing admin CLI)
- [ ] Command: `sidechain-cli search status`
  - Shows: Index names, document counts, index size, health status
- [ ] Command: `sidechain-cli search reindex [users|posts|stories|all]`
  - Full reindex from database with progress bar
  - Delete old index, create new, backfill data
- [ ] Command: `sidechain-cli search optimize [users|posts|stories|all]`
  - Force merge segments, optimize for search speed
- **Acceptance**: Can manage indices from CLI

### 4.2 Add Search Analytics Query Commands
- [ ] **File**: `backend/cmd/cli/main.go`
- [ ] Command: `sidechain-cli search analytics top-queries --days=7`
  - Show top 20 searched queries in last N days
- [ ] Command: `sidechain-cli search analytics failed-searches --days=7`
  - Show searches that returned 0 results (user pain points)
- [ ] Command: `sidechain-cli search analytics trends --days=30`
  - Show search volume over time, trending terms
- [ ] Command: `sidechain-cli search analytics by-genre --days=7`
  - Show search activity by genre
- **Acceptance**: Can query search analytics from CLI

### 4.3 Add Search Relevance Tuning Commands
- [ ] **File**: `backend/cmd/cli/main.go`
- [ ] Command: `sidechain-cli search tune-weights --entity=posts`
  - Interactive CLI to adjust boost weights
  - Show current weights: like_count=3.0, play_count=1.0, etc.
  - User inputs new values, preview impact
  - Save new weights to index configuration
- [ ] Command: `sidechain-cli search analyze-relevance --query="techno"`
  - Show how ES scores each result
  - Break down score components (text relevance, engagement boost, recency, etc.)
  - Help debug ranking issues
- **Acceptance**: Can tune search relevance from CLI

### 4.4 Add Search Performance Monitoring
- [ ] **File**: `backend/cmd/cli/main.go`
- [ ] Command: `sidechain-cli search benchmark --queries=100`
  - Run 100 random searches, measure latency
  - Show: min, max, avg, p95, p99 latencies
  - Identify slow queries
- [ ] Command: `sidechain-cli search slowqueries --limit=10`
  - Show 10 slowest searches from analytics
  - Suggest optimizations (add index, adjust boost)
- **Acceptance**: Can monitor search performance

### 4.5 Add Index Backup/Restore Commands
- [ ] **File**: `backend/cmd/cli/main.go`
- [ ] Command: `sidechain-cli search backup --output=/backups/es-backup.tar.gz`
  - Backup all ES indices to file
- [ ] Command: `sidechain-cli search restore --input=/backups/es-backup.tar.gz`
  - Restore indices from backup file
  - Prompt for confirmation (destructive operation)
- **Acceptance**: Can backup/restore search indices

### 4.6 Add Search Query Testing Command
- [ ] **File**: `backend/cmd/cli/main.go`
- [ ] Command: `sidechain-cli search test-query --entity=posts --query="techno" --filters=genre:techno`
  - Execute search query and show full results + scores
  - Useful for debugging search issues
  - Show full ES query JSON for inspection
- **Acceptance**: Can test search queries interactively

---

## 🧪 Phase 5: Testing & Validation

### 5.1 Backend Unit Tests
- [ ] **File**: `backend/internal/search/client_test.go`
- [ ] Test `SearchUsers()` with various queries and filters
- [ ] Test `SearchPosts()` ranking algorithm (engagement boost)
- [ ] Test `SearchStories()` with date filtering and expiration
- [ ] Test `TrackSearchQuery()` analytics tracking
- [ ] Test fallback to PostgreSQL when ES unavailable
- [ ] Target: 90%+ code coverage for search client
- **Acceptance**: `make test-backend` passes all search tests

### 5.2 Integration Tests
- [ ] **File**: `backend/tests/integration/search_test.go` (new)
- [ ] Create test data: 10 users, 50 posts, 20 stories
- [ ] Test user creation → indexing → search
- [ ] Test post creation with metadata → search with filters
- [ ] Test engagement updates → ranking changes
- [ ] Test deletion → removed from search
- [ ] Test analytics tracking → can query analytics
- **Acceptance**: `make test-integration` passes all search tests

### 5.3 Plugin Unit Tests
- [ ] **File**: `plugin/tests/SearchComponentTest.cpp` (new)
- [ ] Test SearchComponent initialization
- [ ] Test autocomplete dropdown updates
- [ ] Test filter panel state management
- [ ] Test pagination logic
- [ ] Test result item rendering
- [ ] Target: 80%+ code coverage
- **Acceptance**: `make test-plugin-unit` includes search tests

### 5.4 Performance Tests
- [ ] **File**: `backend/tests/performance/search_benchmark_test.go` (new)
- [ ] Benchmark 1000 user search
- [ ] Benchmark 5000 post search with complex filters
- [ ] Benchmark autocomplete suggestions
- [ ] Benchmark analytics query aggregations
- [ ] Target latencies:
  - User search: < 100ms
  - Post search: < 200ms
  - Autocomplete: < 50ms
- **Acceptance**: All benchmarks meet targets

### 5.5 End-to-End Tests (Manual)
- [ ] Create test user account
- [ ] Create multiple posts (different genres, BPMs, engagement levels)
- [ ] Create stories
- [ ] Test all search flows:
  - Text search across all entities
  - Filter by genre, BPM, DAW
  - Autocomplete suggestions
  - Pagination
  - Analytics queries
- [ ] Verify ranking (high-engagement posts appear first)
- [ ] Verify recency boost (recent posts ranked higher)
- **Acceptance**: All manual tests pass

### 5.6 Search Result Relevance Review
- [ ] **Manual**: Execute searches and review results
- [ ] Verify: Most relevant results appear at top
- [ ] Check: Typo tolerance works (search "tencho" finds "techno")
- [ ] Verify: Engagement matters (popular posts rank high)
- [ ] Check: Date matters (recent + popular beats old + very popular)
- [ ] Adjust: Boost weights if needed based on observations
- **Acceptance**: Results feel relevant to users

---

## 📊 Phase 6: Monitoring & Analytics

### 6.1 Create Search Metrics Dashboard
- [ ] **File**: `backend/internal/admin/search_dashboard.go` (new)
- [ ] Endpoint: `GET /api/v1/admin/search-metrics`
- [ ] Returns:
  - Total searches (daily, weekly, monthly)
  - Average latency
  - Top searches (what users search for)
  - Failed searches (zero results)
  - Trending searches
  - Search by genre distribution
  - User engagement with search results
- [ ] Time range filters (1d/7d/30d/90d)
- **Acceptance**: Can view search metrics via API

### 6.2 Create Admin Web Dashboard
- [ ] **File**: `web/src/pages/admin/SearchDashboard.tsx` (new)
- [ ] Display metrics from 6.1 in charts
- [ ] Line chart: Search volume over time
- [ ] Bar chart: Top searches
- [ ] Pie chart: Searches by type (users/posts/stories)
- [ ] Table: Failed searches (user pain points)
- [ ] Route: `/admin/search-metrics` (admin only)
- **Acceptance**: Admin can view search dashboard

### 6.3 Add Elasticsearch Health Monitoring
- [ ] **File**: `backend/internal/health/health.go`
- [ ] Add health check for Elasticsearch
- [ ] Check: Cluster status, number of nodes, index status
- [ ] Endpoint: `GET /health` includes ES status
- [ ] Alert if: ES unavailable, indices unhealthy, disk full
- **Acceptance**: `/health` endpoint reports ES status

### 6.4 Add Search Performance Alerts
- [ ] **File**: `backend/internal/search/monitoring.go` (new)
- [ ] Monitor: Search latency, error rate
- [ ] Alert if:
  - Search latency > 500ms (performance degradation)
  - Search error rate > 1% (indexing/query issues)
  - ES cluster unhealthy
- [ ] Log alerts to system, send to ops team
- **Acceptance**: System alerts on search performance issues

### 6.5 Create Search Usage Analytics Report
- [ ] **File**: `backend/cmd/cli/main.go`
- [ ] Command: `sidechain-cli search report --days=30 --format=json`
- [ ] Generates report: Search trends, user behavior, popular queries
- [ ] Export as JSON/CSV for analysis
- [ ] Show insights: Which queries drive engagement, which fail
- **Acceptance**: Can generate searchable analytics reports

### 6.6 Add Query Performance Logging
- [ ] **File**: `backend/internal/search/client.go`
- [ ] Log all queries with:
  - Query string, filters, execution time
  - Number of results, latency percentiles
  - User who executed, timestamp
- [ ] Store in logs for analytics (could also index to ES)
- [ ] Use for identifying slow/inefficient queries
- **Acceptance**: Search queries logged with performance data

---

## 🚀 Phase 7: Optimization & Polish

### 7.1 Optimize Index Mappings
- [ ] Review ES index configurations for:
  - Field analyzers (are they optimal?)
  - Field weights/boosts (correct priorities?)
  - Keyword vs text fields (right type for each field?)
- [ ] Run relevance tests to verify
- [ ] Adjust analyzer settings if needed
- **Acceptance**: Index mappings optimized for relevance

### 7.2 Implement Index Sharding Strategy
- [ ] Current: Single shard per index (development)
- [ ] Update for production:
  - Users index: 1 shard, 1 replica
  - Posts index: 2-3 shards, 1 replica (can be large)
  - Stories index: 1 shard, 1 replica (time-based)
- [ ] Enables: Parallel search, high availability
- [ ] Update in docker-compose.prod.yml
- **Acceptance**: Production config includes sharding

### 7.3 Implement Search Caching
- [ ] **File**: `backend/internal/search/cache.go` (new)
- [ ] Cache popular searches: top 1000 queries
- [ ] TTL: 1 hour for regular queries, 24 hours for trending
- [ ] Use Redis (already in stack)
- [ ] Bypass cache for: highly personalized, with user-specific filters
- **Acceptance**: Cache hit rate > 30% for searches

### 7.4 Implement Query Suggestion Engine
- [ ] **File**: `backend/internal/search/suggestions.go` (new)
- [ ] When user searches for something that returns 0 results
- [ ] Suggest: Similar searches that DO have results
- [ ] Implementation: Fuzzy match on past successful queries
- [ ] Example: Search "tecnho" → Suggest "techno", "tech house"
- [ ] Implement for users, posts, and genres
- **Acceptance**: No results shows helpful suggestions

### 7.5 Add Advanced Ranking Features
- [ ] Current ranking: Engagement + Recency
- [ ] Add: User relevance (if following user, boost their posts)
- [ ] Add: Interaction history (if user liked similar posts, boost similar results)
- [ ] Add: Collaborative filtering signals from Gorse
- [ ] Implement via Elasticsearch scripting or separate scoring layer
- **Acceptance**: Personalized search results

### 7.6 Optimize for Mobile Web
- [ ] Responsive search UI components
- [ ] Touch-friendly: Larger buttons, no hover states
- [ ] Mobile-optimized filters (collapsible, bottom sheet)
- [ ] Fast: Preload suggestions on focus
- [ ] Test on mobile devices
- **Acceptance**: Search works smoothly on mobile

### 7.7 Implement Search Analytics Dashboard in Plugin
- [ ] **File**: `plugin/src/ui/admin/SearchAnalyticsComponent.h/cpp` (new)
- [ ] If user is admin: Show search metrics in plugin
- [ ] Display: Search volume, top queries, failed searches
- [ ] Only visible to admins (check user role)
- **Acceptance**: Admins can view search analytics in plugin

### 7.8 Add Search to Onboarding
- [ ] Update plugin onboarding to mention search features
- [ ] Show tutorial: How to search, filter, discover
- [ ] Link to search feature from home screen
- [ ] First-time user experience highlights search
- **Acceptance**: New users discover search feature

---

## 🔧 Phase 8: Fallback & Error Handling

### 8.1 Elasticsearch Fallback Strategy
- [ ] If ES unavailable, all searches fall back to PostgreSQL
- [ ] Log when fallback happens (monitor for issues)
- [ ] Client indicates fallback in response (`fallback: true`)
- [ ] Graceful degradation: Works, just less features
- **Acceptance**: Search works with or without ES

### 8.2 Add Resilience Tests
- [ ] **Test**: Stop Elasticsearch container mid-request
- [ ] Verify: Search still works (falls back)
- [ ] Verify: No error shown to user
- [ ] Verify: Logs show fallback event
- [ ] Verify: System recovers when ES comes back online
- **Acceptance**: Fallback behavior tested and working

### 8.3 Index Sync Error Recovery
- [ ] When indexing fails (ES down), queue for retry
- [ ] Retry queue: Store failed index operations
- [ ] On ES recovery: Replay queued operations
- [ ] Max retries: 3, then log and alert
- **Acceptance**: No data loss if ES temporarily unavailable

### 8.4 Search Query Error Handling
- [ ] Catch all ES query errors
- [ ] Log error with context (query, filters, user)
- [ ] Return user-friendly message (not raw ES error)
- [ ] Fallback to DB if ES error
- [ ] Alert on repeated errors from same query
- **Acceptance**: Users don't see technical errors

---

## 📝 Phase 9: Documentation & Training

### 9.1 Write Search Architecture Documentation
- [ ] **File**: `docs/SEARCH_ARCHITECTURE.md` (new)
- [ ] Explain: Index design, query structure, ranking algorithm
- [ ] Document: All indices, mappings, analyzer configuration
- [ ] Include: Performance characteristics, limits, scaling strategy
- **Acceptance**: New developer can understand search system

### 9.2 Write API Documentation
- [ ] **File**: `docs/API_SEARCH.md` (new)
- [ ] Document all endpoints:
  - `/api/v1/search/users`
  - `/api/v1/search/posts`
  - `/api/v1/search/stories`
  - `/api/v1/search/users/suggest`
  - `/api/v1/search/advanced`
- [ ] Include: Query examples, response format, filters, sorting
- [ ] Add: Error codes, rate limits, best practices
- **Acceptance**: API fully documented

### 9.3 Write Admin CLI Documentation
- [ ] **File**: `docs/CLI_SEARCH.md` (new)
- [ ] Document all CLI commands
- [ ] Examples: How to reindex, analyze, benchmark
- [ ] Troubleshooting: Common issues and fixes
- **Acceptance**: Admin can operate search system

### 9.4 Create Search Tuning Guide
- [ ] **File**: `docs/SEARCH_TUNING.md` (new)
- [ ] How to adjust ranking weights
- [ ] How to understand search metrics
- [ ] How to optimize slow queries
- [ ] When to add more shards/replicas
- **Acceptance**: Ops team can tune search system

### 9.5 Write Search Implementation Notes
- [ ] **File**: `notes/ELASTICSEARCH_IMPLEMENTATION_NOTES.md` (new)
- [ ] Design decisions: Why we chose certain boosts, weights
- [ ] Trade-offs: Relevance vs performance vs complexity
- [ ] Future improvements: What we could add
- [ ] Lessons learned: What worked, what didn't
- **Acceptance**: Documented for future developers

---

## ✅ Acceptance Criteria & Sign-Off

### Code Quality
- [ ] All new code reviewed and approved
- [ ] 80%+ test coverage for search code
- [ ] No linting errors
- [ ] Documentation complete and accurate

### Functionality
- [ ] All search endpoints working
- [ ] All filters functional
- [ ] Autocomplete working
- [ ] Pagination working
- [ ] Analytics tracking working

### Performance
- [ ] User search: < 100ms
- [ ] Post search: < 200ms
- [ ] Autocomplete: < 50ms
- [ ] No N+1 queries
- [ ] Index size reasonable (< 500MB for 10k posts)

### User Experience
- [ ] Search feels responsive
- [ ] Results feel relevant
- [ ] No confusing error messages
- [ ] Works on mobile
- [ ] Plugin/Web/CLI all functional

### Operations
- [ ] Monitoring in place
- [ ] Alerts configured
- [ ] Backup/restore tested
- [ ] Fallback mechanism tested
- [ ] Performance benchmarks established

### Documentation
- [ ] Architecture documented
- [ ] API documented
- [ ] CLI documented
- [ ] Tuning guide written
- [ ] Implementation notes written

---

## 🎯 Success Metrics

**User Metrics**:
- Search feature adoption rate > 20%
- Users per day using search > 100
- Average search latency < 150ms

**Business Metrics**:
- Search-to-engagement rate (searches that lead to follows/likes/plays)
- Discovery via search > 30% of discovery path
- Users report finding new artists via search (qualitative feedback)

**Technical Metrics**:
- Search error rate < 0.1%
- Index freshness < 1 second (data indexed within 1s of creation)
- Search cache hit rate > 30%
- Fallback to DB < 0.01% of searches

---

## 📅 Implementation Timeline

- **Week 1**: Phases 0-1 (Backend foundation + endpoints) → Code review
- **Week 2**: Phase 2 (Plugin UI) → Integration testing
- **Week 3**: Phase 3-4 (Web + CLI) → End-to-end testing, Phase 5 (Testing)
- **Week 4**: Phase 6-9 (Monitoring, Optimization, Documentation) → Final sign-off

**Parallelizable**:
- Backend (Phase 0-1) and Plugin (Phase 2) can happen simultaneously
- Web (Phase 3) can start once API endpoints ready

---

## 🚦 Status Tracking

| Phase | Status | Owner | Completed | Notes |
|-------|--------|-------|-----------|-------|
| 0: Backend Foundation | TODO | - | 0/8 | Core wiring |
| 1: Search Endpoints | TODO | - | 0/8 | REST API |
| 2: Plugin Search | TODO | - | 0/10 | C++ UI |
| 3: Web Search | TODO | - | 0/10 | React UI |
| 4: CLI Admin | TODO | - | 0/6 | Admin commands |
| 5: Testing | TODO | - | 0/6 | Unit + Integration |
| 6: Monitoring | TODO | - | 0/6 | Observability |
| 7: Optimization | TODO | - | 0/8 | Performance |
| 8: Fallback | TODO | - | 0/4 | Resilience |
| 9: Documentation | TODO | - | 0/5 | Docs + Training |

**Total Tasks**: 71
**Completed**: 0
**In Progress**: 0
**Remaining**: 71
