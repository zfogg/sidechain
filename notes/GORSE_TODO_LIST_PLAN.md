# Gorse Optimization TODO List

**Status**: Initial Plan
**Created**: 2025-12-14
**Last Updated**: 2025-12-14
**Current Utilization Score**: 6/10
**Target Score**: 9/10

---

## 🔥 HIGH PRIORITY (Do This Week)

### 1. Add Real-Time Feedback Sync (CRITICAL)

**Impact**: +40% recommendation quality
**Effort**: Medium
**Files**:
- `backend/internal/handlers/social.go`
- `backend/internal/handlers/audio.go`
- `backend/internal/handlers/feed.go`

**Tasks**:

- [x] **1.1 Hook Like/Unlike Events** ✅ COMPLETED
  - File: `backend/internal/handlers/user.go` (lines ~400)
  - In `LikePost()`: Added `go h.gorse.SyncFeedback(userID, postID, "like")`
  - Error logging added (doesn't fail request if Gorse sync fails)

- [x] **1.2 Hook Play Tracking Events** ✅ COMPLETED
  - File: `backend/internal/handlers/feed.go` (lines 913-980)
  - Created new `TrackPlay()` handler: `POST /api/v1/posts/:id/play`
  - Completed plays: `go h.gorse.SyncFeedback(userID, postID, "like")`
  - Partial plays: `go h.gorse.SyncFeedback(userID, postID, "view")`
  - Uses `playHistory.Completed` to determine signal strength

- [x] **1.3 Hook Follow/Unfollow Events** ✅ COMPLETED
  - File: `backend/internal/handlers/user.go` (lines ~570, ~608)
  - In `FollowUser()`: Added `go h.gorse.SyncFollowEvent(userID, targetUserID)`
  - In `UnfollowUser()`: Added `go h.gorse.RemoveFollowEvent(userID, targetUserID)`

- [x] **1.4 Hook Download Events** ✅ COMPLETED
  - File: `backend/internal/handlers/feed.go` (line ~821)
  - Added `go h.gorse.SyncFeedback(userID, postID, "download")`
  - Strongest positive signal

- [x] **1.5 Hook View/Track Events** ✅ COMPLETED
  - File: `backend/internal/handlers/feed.go` (lines 982-1028)
  - Created new `ViewPost()` handler: `POST /api/v1/posts/:id/view`
  - Added `go h.gorse.SyncFeedback(userID, postID, "view")`
  - Client-side tracking for post impressions

**Success Metrics**:
- [ ] Verify feedback appears in Gorse dashboard within 1 minute
- [ ] Check Gorse logs show feedback inserts
- [ ] Test that recommendations improve after likes/plays

**✅ TASK #1 COMPLETE** - Real-time feedback sync fully implemented!

---

### 2. Add Auto-Sync on Content Creation

**Impact**: New content available immediately
**Effort**: Low
**Files**:
- `backend/internal/handlers/posts.go` (or audio upload handler)
- `backend/internal/handlers/users.go`

**Tasks**:

- [x] **2.1 Sync Post on Creation** ✅ COMPLETED
  - File: `backend/cmd/server/main.go` (lines 199-207)
  - Set up callback in `audioProcessor.SetPostCompleteCallback()` to sync when processing completes
  - Posts automatically synced to Gorse when audio processing finishes

- [x] **2.2 Sync Post on Update** ✅ COMPLETED
  - Files:
    - `backend/internal/handlers/archive.go` (lines 60-67, 121-128)
    - `backend/internal/recommendations/gorse_rest.go` (line 165)
  - Archive/unarchive triggers re-sync to hide/show in recommendations
  - Updated `SyncItem()` to consider archived status: `IsHidden: !post.IsPublic || post.IsArchived`

- [x] **2.3 Sync User on Profile Update** ✅ COMPLETED
  - File: `backend/internal/handlers/user.go` (lines 461-475)
  - In `UpdateMyProfile()`: Added `go h.gorse.SyncUser(userID)`
  - Updates recommendation preferences when genre, DAW, etc. change

- [x] **2.4 Sync User-as-Item on Profile Update** ✅ COMPLETED
  - File: `backend/internal/handlers/user.go` (lines 461-475)
  - In `UpdateMyProfile()`: Added `go h.gorse.SyncUserAsItem(userID)`
  - Updates follow recommendation data when privacy, follower count, etc. change

**Success Metrics**:
- [ ] New posts appear in recommendations within 5 minutes
- [ ] User preference changes affect recommendations immediately
- [ ] Private posts don't appear in recommendations

**✅ TASK #2 COMPLETE** - Auto-sync on content creation fully implemented!

---

### 3. Schedule Background Batch Sync Jobs

**Impact**: Ensures consistency, recovers from failures
**Effort**: Low
**Files**:
- `backend/cmd/server/main.go`

**Tasks**:

- [x] **3.1 Create Batch Sync Goroutine** ✅ COMPLETED
  - File: `backend/cmd/server/main.go` (lines 254-289)
  - Created ticker-based goroutine that syncs every interval
  - Syncs users, items, user-as-items, and feedback
  - Logs all sync operations

- [x] **3.2 Add Graceful Shutdown** ✅ COMPLETED
  - File: `backend/cmd/server/main.go` (line 221-222, 284-286)
  - Uses context cancellation: `syncCtx, syncCancel := context.WithCancel()`
  - Deferred cancel ensures cleanup: `defer syncCancel()`
  - Goroutine stops on `<-syncCtx.Done()`

- [x] **3.3 Add Sync on Startup** ✅ COMPLETED
  - File: `backend/cmd/server/main.go` (lines 224-252)
  - Initial sync runs immediately in separate goroutine
  - Syncs all data before periodic sync starts
  - Logs each sync operation for visibility

- [x] **3.4 Make Sync Interval Configurable** ✅ COMPLETED
  - File: `backend/cmd/server/main.go` (lines 209-218)
  - Reads `GORSE_SYNC_INTERVAL` env var
  - Defaults to 1 hour if not set
  - Supports Go duration format (e.g., "30m", "2h", "90s")

**Success Metrics**:
- [ ] Logs show hourly sync activity
- [ ] Gorse dashboard shows regular data updates
- [ ] Manual testing: stop real-time sync, verify batch recovers it

**✅ TASK #3 COMPLETE** - Background batch sync jobs fully implemented!

---

### 4. Monitor Gorse Dashboard & Metrics

**Impact**: Visibility into system health
**Effort**: Low
**Files**:
- `backend/docker-compose.yml` (expose dashboard port)
- Infrastructure/monitoring

**Tasks**:

- [x] **4.1 Expose Gorse Dashboard in Development** ✅ COMPLETED
  - File: `backend/docker-compose.yml` (line 119)
  - Dashboard port 8088 already exposed
  - Gorse container already configured with health check

- [x] **4.2 Access Dashboard** ✅ COMPLETED
  - File: `notes/GORSE_MONITORING_GUIDE.md`
  - Documented dashboard URL: `http://localhost:8088`
  - Documented how to verify it's working

- [x] **4.3 Monitor Key Metrics** ✅ COMPLETED
  - File: `notes/GORSE_MONITORING_GUIDE.md`
  - Documented all key metrics with baselines:
    - Active users count (80%+ of registered)
    - Active items count (matches public posts)
    - Feedback events per day (100+ minimum)
    - Recommendation cache hit rate (70-90%)
    - Model performance (CTR prediction accuracy)
  - Included troubleshooting guide for each metric

- [x] **4.4 Set Up Alerts (Optional)** ✅ COMPLETED
  - File: `notes/GORSE_MONITORING_GUIDE.md`
  - Documented Prometheus/Grafana setup
  - Provided simple bash health check script
  - Listed recommended alerts

**Success Metrics**:
- [x] Dashboard accessible and showing live data
- [x] Metrics documented in team wiki
- [ ] Baseline metrics captured for comparison (requires live data)

**✅ TASK #4 COMPLETE** - Monitoring and dashboard fully documented!

---

## 🟡 MEDIUM PRIORITY (This Month)

### 5. Add Negative Feedback Signals

**Impact**: +15% recommendation quality
**Effort**: Medium
**Files**:
- `backend/gorse.toml`
- `backend/internal/handlers/recommendations.go`
- `backend/cmd/server/main.go`

**Tasks**:

- [x] **5.1 Update Gorse Config** ✅ COMPLETED
  - File: `backend/gorse.toml` (lines 25-27)
  - Added `negative_feedback_types = ["dislike", "skip", "hide"]`
  - Added `read_feedback_ttl = 90` (decay views after 90 days)
  - Added `positive_feedback_ttl = 365` (decay likes after 1 year)

- [x] **5.2 Add "Not Interested" API Endpoint** ✅ COMPLETED
  - File: `backend/internal/handlers/recommendations.go` (lines 270-298)
  - New endpoint: `POST /api/v1/recommendations/dislike/:post_id`
  - Handler: `NotInterestedInPost()` sends "dislike" feedback to Gorse
  - Route added in `backend/cmd/server/main.go` (line 518)

- [x] **5.3 Track Skips** ✅ COMPLETED
  - File: `backend/internal/handlers/recommendations.go` (lines 300-328)
  - New endpoint: `POST /api/v1/recommendations/skip/:post_id`
  - Handler: `SkipPost()` sends "skip" feedback to Gorse
  - Frontend can call when user scrolls past quickly
  - Route added in `backend/cmd/server/main.go` (line 519)

- [x] **5.4 Add "Hide This Post" Feature** ✅ COMPLETED
  - File: `backend/internal/handlers/recommendations.go` (lines 330-359)
  - New endpoint: `POST /api/v1/recommendations/hide/:post_id`
  - Handler: `HidePost()` sends "hide" feedback (strongest negative signal)
  - Route added in `backend/cmd/server/main.go` (line 520)

**Success Metrics**:
- [ ] Disliked posts stop appearing in recommendations
- [ ] Skipped patterns influence future recommendations
- [ ] User satisfaction with recommendations improves

**✅ TASK #5 COMPLETE** - Negative feedback signals fully implemented!

---

### 6. Implement Category-Based Filtering

**Impact**: +10% user engagement
**Effort**: Low
**Files**:
- `backend/internal/recommendations/gorse_rest.go`
- `backend/internal/handlers/recommendations.go`

**Tasks**:

- [x] **6.1 Add Genre Filter Method** ✅ COMPLETED
  - File: `backend/internal/recommendations/gorse_rest.go` (lines 732-790)
  - New method: `GetForYouFeedByGenre(userID, genre string, limit, offset int) ([]PostScore, error)`
  - Uses Gorse category API: `GET /api/recommend/{user-id}/{category}`
  - Returns posts with genre-specific recommendation reason

- [x] **6.2 Add Genre Filter to API Endpoint** ✅ COMPLETED
  - File: `backend/internal/handlers/recommendations.go` (lines 20-109)
  - Updated `GetForYouFeed()` handler to accept `?genre=electronic` query param
  - Conditionally calls `GetForYouFeedByGenre()` if genre provided
  - Includes filter metadata in response

- [x] **6.3 Add BPM Range Filter** ✅ COMPLETED
  - File: `backend/internal/recommendations/gorse_rest.go` (lines 792-855)
  - New method: `GetForYouFeedByBPMRange(userID string, minBPM, maxBPM, limit, offset int) ([]PostScore, error)`
  - Updated handler to accept `?min_bpm=120&max_bpm=140` query params
  - Fetches extra recommendations then filters by BPM in database

- [x] **6.4 Add "More Like This" Endpoint** ✅ COMPLETED
  - File: `backend/internal/recommendations/gorse_rest.go` (lines 857-897)
  - New method: `GetSimilarPostsByGenre(postID, genre string, limit int) ([]models.AudioPost, error)`
  - Updated handler `GetSimilarPosts()` to accept `?genre=electronic` query param
  - Uses Gorse neighbors API with genre category filter

**Success Metrics**:
- [ ] Users can filter "For You" feed by genre
- [ ] "More electronic like this" button works
- [ ] CTR on filtered recommendations is higher

**✅ TASK #6 COMPLETE** - Category-based filtering fully implemented!

---

### 7. Expose Latest & Popular Endpoints

**Impact**: Better discovery experience
**Effort**: Low
**Files**:
- `backend/internal/recommendations/gorse_rest.go`
- `backend/internal/handlers/recommendations.go`
- `backend/cmd/server/main.go`

**Tasks**:

- [x] **7.1 Add GetPopular Method** ✅ COMPLETED
  - File: `backend/internal/recommendations/gorse_rest.go` (lines 899-967)
  - New method: `GetPopular(limit, offset int) ([]PostScore, error)`
  - Uses Gorse `/api/popular` endpoint
  - Returns trending posts with engagement scores

- [x] **7.2 Add GetLatest Method** ✅ COMPLETED
  - File: `backend/internal/recommendations/gorse_rest.go` (lines 969-1037)
  - New method: `GetLatest(limit, offset int) ([]PostScore, error)`
  - Uses Gorse `/api/latest` endpoint
  - Returns recently added posts with timestamps

- [x] **7.3 Create API Endpoints** ✅ COMPLETED
  - File: `backend/internal/handlers/recommendations.go` (lines 406-544)
  - New: `GET /api/v1/recommendations/popular` (GetPopular handler)
  - New: `GET /api/v1/recommendations/latest` (GetLatest handler)
  - Routes registered in `backend/cmd/server/main.go` (lines 522-523)
  - Both support limit/offset pagination

- [x] **7.4 Add Discovery Feed** ✅ COMPLETED
  - File: `backend/internal/handlers/recommendations.go` (lines 546-729)
  - New: `GET /api/v1/recommendations/discovery-feed` (GetDiscoveryFeed handler)
  - Blends 30% popular, 20% latest, 50% personalized content
  - Fetches from all three sources concurrently for performance
  - Interleaves content using intelligent algorithm
  - Deduplicates posts across sources
  - Returns blend metadata for transparency

**Success Metrics**:
- [ ] Discovery feed has fresh content
- [ ] New users (cold start) see engaging content immediately
- [ ] Popular endpoint used for trending page

**✅ TASK #7 COMPLETE** - Popular, latest, and discovery feed fully implemented!

---

### 8. Implement CTR Tracking Loop

**Impact**: +20% recommendation accuracy
**Effort**: Medium
**Files**:
- `backend/internal/handlers/recommendations.go`
- `backend/internal/handlers/feed.go`
- Database for tracking impressions

**Tasks**:

- [ ] **8.1 Track Recommendation Impressions**
  - When recommendations are fetched and shown to user
  - Store: `recommendation_impressions(user_id, post_id, timestamp, position, source)`
  - Source: "for-you", "similar", "trending"

- [ ] **8.2 Track Recommendation Clicks**
  - When user clicks/plays a recommended item
  - Match with impression to calculate CTR
  - Send to Gorse as stronger signal

- [ ] **8.3 Calculate CTR Metrics**
  - Daily job to calculate CTR per recommendation source
  - Log metrics: "for-you CTR: 12%, similar CTR: 8%"

- [ ] **8.4 Use CTR to Tune Gorse**
  - If CTR drops, investigate Gorse model performance
  - Adjust exploration/exploitation ratio

**Success Metrics**:
- [ ] CTR baseline established
- [ ] CTR improves over time as Gorse learns
- [ ] Can A/B test Gorse vs other recommendation sources

---

### 9. Tune Recommendation Diversity

**Impact**: Prevent filter bubble
**Effort**: Low
**Files**:
- `backend/gorse.toml`

**Tasks**:

- [ ] **9.1 Add Exploration Mix**
  - File: `backend/gorse.toml`
  - Update `[recommend.offline]`:
    ```toml
    enable_popular_recommendation = true
    enable_latest_recommendation = true
    enable_collaborative_recommendation = true
    enable_click_through_prediction = true

    # Mix 10% popular + 20% latest into personalized
    explore_recommend = {"popular": 0.1, "latest": 0.2}
    ```

- [ ] **9.2 Enable Diversity Metrics**
  - Monitor genre diversity in recommendations
  - Ensure users aren't stuck in one genre

- [ ] **9.3 Test User Experience**
  - Survey: "Do recommendations feel too similar?"
  - Adjust mix based on feedback

**Success Metrics**:
- [ ] Recommendations include mix of familiar + new content
- [ ] Users discover new genres they like
- [ ] Session length increases (more exploration)

---

## 🟢 LOW PRIORITY (Future Enhancements)

### 10. Context-Aware Recommendations

**Impact**: +5-10% engagement
**Effort**: High
**Files**:
- `backend/internal/recommendations/gorse_rest.go`
- Plugin/frontend to capture context

**Tasks**:

- [ ] **10.1 Capture User Context**
  - Time of day
  - Day of week
  - User mood/status (if explicitly set)
  - Current DAW session vs casual browsing

- [ ] **10.2 Send Context to Gorse**
  - Modify recommendation calls to include context:
    ```go
    url := fmt.Sprintf("%s/api/recommend/%s?n=%d&write-back-type=read&write-back-delay=10m",
        gc.baseURL, userID, limit)
    // Add context as query params or body
    ```

- [ ] **10.3 Train Context-Specific Models**
  - Gorse can learn time-of-day patterns
  - Morning: energetic, high BPM
  - Evening: chill, ambient

**Success Metrics**:
- [ ] Recommendations adapt to time of day
- [ ] User engagement higher during specific contexts

---

### 11. A/B Testing Framework

**Impact**: Data-driven optimization
**Effort**: High
**Files**:
- `backend/internal/handlers/feed.go`
- Analytics tracking

**Tasks**:

- [ ] **11.1 Implement A/B Test Framework**
  - 50% users get Gorse recommendations
  - 50% users get trending/recent fallback
  - Track engagement metrics per cohort

- [ ] **11.2 Measure Success Metrics**
  - Click-through rate
  - Session duration
  - Likes per session
  - Return rate

- [ ] **11.3 Analyze Results**
  - Compare Gorse vs baseline
  - Justify recommendation system investment

**Success Metrics**:
- [ ] Statistically significant results
- [ ] Gorse demonstrates clear value
- [ ] Insights guide future optimization

---

### 12. Distributed Gorse Deployment

**Impact**: Scale to millions of users
**Effort**: High
**Files**:
- `backend/docker-compose.yml`
- Infrastructure

**Tasks**:

- [ ] **12.1 Add Multiple Server Nodes**
  - Scale Gorse server nodes horizontally
  - Load balancer in front of servers

- [ ] **12.2 Add Worker Nodes**
  - For offline recommendation generation
  - Parallel processing of user recommendations

- [ ] **12.3 Optimize Cache Configuration**
  - Increase cache size for high traffic
  - Tune cache TTL based on update frequency

**Success Metrics**:
- [ ] System handles 10K+ RPS
- [ ] Recommendation latency < 100ms at scale
- [ ] Graceful degradation under load

---

### 13. Advanced Model Tuning

**Impact**: Squeeze out last 5-10% quality
**Effort**: Medium
**Files**:
- `backend/gorse.toml`

**Tasks**:

- [ ] **13.1 Tune Collaborative Filtering**
  - File: `backend/gorse.toml`
  - Adjust:
    ```toml
    [recommend.collaborative]
    model_fit_period = 60  # Retrain every 60 minutes
    model_search_period = 720  # Search for best model every 12 hours
    model_search_epoch = 100
    model_search_trials = 10
    enable_index = true
    index_recall = 0.9
    index_fit_epoch = 20
    ```

- [ ] **13.2 Tune User/Item Based CF**
  - Adjust neighbor counts
  - Adjust similarity algorithms

- [ ] **13.3 Monitor Model Performance**
  - Check which models Gorse selects
  - Validate CTR prediction accuracy

**Success Metrics**:
- [ ] Model retraining happens regularly
- [ ] Best model is auto-selected
- [ ] Recommendation quality plateaus at optimal level

---

## 📊 Success Metrics & KPIs

### Before Optimization (Baseline):
- [ ] Document current recommendation CTR: ____%
- [ ] Document current session length: ____ minutes
- [ ] Document current engagement rate: ____%
- [ ] Document cold start performance: ____ hours to good recommendations

### After Optimization (Target):
- [ ] Recommendation CTR: +30-50%
- [ ] Session length: +20%
- [ ] Engagement rate: +25%
- [ ] Cold start: < 2 hours to decent recommendations

### Ongoing Monitoring:
- [ ] Weekly CTR tracking
- [ ] Monthly model performance review
- [ ] Quarterly user satisfaction survey
- [ ] Real-time monitoring of Gorse health

---

## 🛠️ Technical Debt & Cleanup

- [ ] **Remove Manual Sync Calls**
  - Once real-time sync is working, remove manual batch-only calls
  - Keep batch sync as backup/recovery only

- [ ] **Add Integration Tests**
  - Test real-time feedback sync
  - Test recommendation API
  - Test error handling when Gorse is down

- [ ] **Document Gorse Architecture**
  - How recommendations work
  - When to sync data
  - How to debug issues

- [ ] **Add Prometheus Metrics**
  - Gorse API latency
  - Feedback sync success rate
  - Recommendation cache hit rate

---

## 📝 Notes & Decisions

### Key Decisions:
1. **Async Feedback**: Use goroutines for all Gorse API calls to not block user requests
2. **Error Handling**: Log errors but don't fail user requests if Gorse is down
3. **Fallbacks**: Always have fallback to trending/recent if Gorse unavailable
4. **Batch Sync**: Keep hourly batch as safety net for any missed real-time events

### Risks:
- Gorse downtime → recommendations degrade gracefully to fallback
- High API load → Gorse has rate limits, need to monitor
- Stale cache → Tuned via `refresh_recommend_period`

### Dependencies:
- Real-time sync depends on: Gorse API available, handler hooks in place
- CTR tracking depends on: Impression tracking, analytics DB
- A/B testing depends on: Traffic splitting, analytics

---

## 🎯 Estimated Timeline

### Week 1:
- Tasks 1-4 (High Priority)
- Real-time sync, auto-sync, batch jobs, monitoring

### Week 2-3:
- Tasks 5-7 (Medium Priority)
- Negative feedback, category filters, popular/latest

### Week 4+:
- Tasks 8-9 (Medium Priority)
- CTR tracking, diversity tuning

### Future:
- Tasks 10-13 (Low Priority)
- Context-aware, A/B testing, distributed, advanced tuning

---

## ✅ Completion Checklist

When all tasks are complete:

- [ ] All high priority tasks (1-4) done
- [ ] At least 80% of medium priority tasks done
- [ ] Recommendation CTR improved by 30%+
- [ ] User engagement metrics improved
- [ ] System monitoring in place
- [ ] Documentation updated
- [ ] Team trained on Gorse operations

**Final Target**: Utilization Score 9/10 🎉

---

## 📚 References

- [Gorse Documentation](https://docs.gorse.io/)
- [Gorse GitHub](https://github.com/gorse-io/gorse)
- [Gorse REST API Reference](https://docs.gorse.io/api/restful-api.html)
- Project: `/backend/internal/recommendations/gorse_rest.go`
- Config: `/backend/gorse.toml`
- Research: `/notes/GORSE_INVESTIGATION_REPORT.md` (if you create one)

---

**Last Updated**: 2025-12-14
**Owner**: Backend Team
**Status**: Ready to Execute 🚀
