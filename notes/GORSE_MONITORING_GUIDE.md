# Gorse Monitoring & Dashboard Guide

## Overview

This guide covers how to monitor the Gorse recommendation system, access the dashboard, and interpret key metrics.

## Accessing the Dashboard

### Local Development

1. **Start the backend services**:
   ```bash
   cd backend
   docker-compose up -d
   ```

2. **Access the dashboard**:
   - URL: http://localhost:8088
   - The Gorse dashboard combines both the API and web UI

3. **Verify Gorse is running**:
   ```bash
   curl http://localhost:8088/api/health
   ```

### Production/Staging

- The Gorse dashboard should NOT be publicly exposed in production
- Access via:
  - VPN connection to internal network
  - SSH tunnel: `ssh -L 8088:localhost:8088 your-server`
  - Internal monitoring tools only

## Key Metrics to Monitor

### 1. Active Users Count

**What it is**: Number of unique users with at least one interaction

**Where to find**: Dashboard → Users tab

**What to watch for**:
- Should match or be close to your total user count
- Low count = users not syncing properly
- Check batch sync logs if count is too low

**Healthy baseline**: 80%+ of registered users

---

### 2. Active Items Count

**What it is**: Number of posts/items available for recommendations

**Where to find**: Dashboard → Items tab

**What to watch for**:
- Should match public post count
- Sudden drops = sync issue or posts being hidden
- Check `IsHidden` logic in `SyncItem()`

**Healthy baseline**: Matches database count: `SELECT COUNT(*) FROM audio_posts WHERE is_public = true AND is_archived = false`

---

### 3. Feedback Events Per Day

**What it is**: Number of user interactions (likes, plays, views, follows, downloads)

**Where to find**: Dashboard → Feedback tab

**What to watch for**:
- ⚠️ **Zero events** = Real-time sync broken, check logs
- Sudden drop = Sync failure or user activity drop
- Should increase as user base grows

**Healthy baseline**:
- Minimum: 100+ events/day (small user base)
- Target: 1000+ events/day (active community)

**Breakdown by type**:
- `view`: Most common (browse activity)
- `play`: Medium frequency (listened to post)
- `like`: Less frequent (high engagement)
- `download`: Rare (highest engagement)
- `follow`: Rare (social connection)

---

### 4. Recommendation Cache Hit Rate

**What it is**: Percentage of recommendation requests served from cache vs computed

**Where to find**: Dashboard → Performance tab or Gorse logs

**What to watch for**:
- Below 50% = Cache too small or users have too diverse behavior
- Above 90% = Good performance
- Near 0% = Cache disabled or not working

**Healthy baseline**: 70-90%

**How to improve**:
- Increase cache size in `gorse.toml`
- Adjust `refresh_recommend_period` (how often to refresh cache)
- Monitor memory usage

---

### 5. Model Performance (CTR Prediction)

**What it is**: How accurately Gorse predicts which items users will click/engage with

**Where to find**: Dashboard → Models tab → Metrics

**What to watch for**:
- Precision: What % of recommended items get clicked
- Recall: What % of items users click were recommended
- NDCG: Ranking quality (higher = better)

**Healthy baseline**:
- Precision: 5-15% (users click 1 in 7-20 recommendations)
- Recall: 30-60% (recommendations capture user's interests)
- NDCG: 0.3-0.6

---

## Monitoring Checklist

### Daily Checks (Automated Alerts Recommended)

- [ ] **Feedback events** > 0 in last hour (sync working)
- [ ] **API errors** < 1% of requests
- [ ] **Cache hit rate** > 50%

### Weekly Checks

- [ ] **User count** growing or stable
- [ ] **Item count** matches database
- [ ] **Feedback growth** proportional to user growth
- [ ] **Model retraining** happening (check logs)

### Monthly Reviews

- [ ] **CTR trends** - improving, stable, or declining?
- [ ] **User engagement** - are recommendations working?
- [ ] **Performance** - response times acceptable?
- [ ] **Resource usage** - CPU, memory, disk within limits?

---

## Setting Up Alerts (Optional)

### Prometheus + Grafana Setup

1. **Expose Gorse metrics**:
   ```bash
   # Gorse exposes Prometheus metrics at /api/metrics
   curl http://localhost:8088/api/metrics
   ```

2. **Configure Prometheus scrape**:
   ```yaml
   scrape_configs:
     - job_name: 'gorse'
       static_configs:
         - targets: ['localhost:8088']
       metrics_path: '/api/metrics'
   ```

3. **Create Grafana dashboard**:
   - Import Gorse dashboard JSON from Gorse docs
   - Or create custom dashboard with key metrics

### Simple Alerts (No Prometheus)

Create a monitoring script:

```bash
#!/bin/bash
# gorse_health_check.sh

# Check if Gorse is responding
if ! curl -f http://localhost:8088/api/health > /dev/null 2>&1; then
    echo "❌ Gorse is down!"
    # Send alert via Slack/email/PagerDuty
    exit 1
fi

# Check recent feedback count
FEEDBACK_COUNT=$(curl -s http://localhost:8088/api/feedback | jq '. | length')
if [ "$FEEDBACK_COUNT" -lt 10 ]; then
    echo "⚠️ Low feedback count: $FEEDBACK_COUNT (expected > 10 in recent window)"
    # Send warning
fi

echo "✅ Gorse health check passed"
```

Run via cron:
```cron
*/15 * * * * /path/to/gorse_health_check.sh
```

---

## Troubleshooting

### No Feedback Events Showing Up

**Symptoms**: Dashboard shows 0 feedback in last hour/day

**Causes**:
1. Real-time sync not working
2. Gorse API down
3. Network issues between backend and Gorse

**Solutions**:
```bash
# 1. Check backend logs for Gorse sync errors
docker logs sidechain-backend | grep -i gorse

# 2. Check if Gorse is reachable
curl http://localhost:8088/api/health

# 3. Manual test - send feedback directly
curl -X POST http://localhost:8088/api/feedback \
  -H "X-API-Key: sidechain_gorse_api_key" \
  -H "Content-Type: application/json" \
  -d '[{"FeedbackType": "like", "UserId": "test-user", "ItemId": "test-post"}]'

# 4. Trigger batch sync manually
# The next scheduled sync will fix any gaps
```

---

### Users/Items Count Incorrect

**Symptoms**: Dashboard shows fewer users/items than database

**Causes**:
1. Initial batch sync not run
2. Sync failing silently
3. Filter logic hiding items (private, archived)

**Solutions**:
```bash
# 1. Check batch sync logs
docker logs sidechain-backend | grep "batch sync"

# 2. Verify database counts
psql -U sidechain -d sidechain -c "SELECT COUNT(*) FROM users;"
psql -U sidechain -d sidechain -c "SELECT COUNT(*) FROM audio_posts WHERE is_public = true AND is_archived = false;"

# 3. Compare with Gorse
curl -s http://localhost:8088/api/user | jq '. | length'
curl -s http://localhost:8088/api/item | jq '. | length'

# 4. Manual full re-sync
# Restart backend - initial sync runs on startup
docker restart sidechain-backend
```

---

### Low Cache Hit Rate

**Symptoms**: Cache hit rate < 50%

**Causes**:
1. Cache too small for user base
2. Recommendations refreshing too frequently
3. Users have very diverse behavior

**Solutions**:
1. **Increase cache size** in `gorse.toml`:
   ```toml
   [recommend.cache_size]
   # Increase from default
   recommend_cache = 1000  # Per-user recommendation cache
   ```

2. **Adjust refresh period**:
   ```toml
   [recommend]
   # Refresh cached recommendations less frequently
   refresh_recommend_period = 120  # Minutes (default: 60)
   ```

3. **Monitor memory usage**: Ensure server has enough RAM for larger cache

---

### Poor Recommendation Quality

**Symptoms**: Users report bad recommendations, low CTR

**Causes**:
1. Not enough feedback data
2. Cold start problem (new users/items)
3. Model not training
4. Poor diversity settings

**Solutions**:
1. **Check feedback volume**: Need 100+ events minimum for good quality
2. **Wait for model training**: Check logs for "model training completed"
3. **Enable fallbacks**: Popular/trending for cold start users
4. **Tune diversity** in `gorse.toml`:
   ```toml
   [recommend.offline]
   explore_recommend = {"popular": 0.1, "latest": 0.2}
   ```

---

## Maintenance Tasks

### Weekly

- [ ] Review error logs: `docker logs sidechain-backend | grep -i "gorse.*error"`
- [ ] Check sync success rates
- [ ] Verify cache hit rate trends

### Monthly

- [ ] Review recommendation quality metrics
- [ ] Check model performance trends
- [ ] Optimize `gorse.toml` if needed
- [ ] Audit disk usage for Gorse data

### Quarterly

- [ ] Full system review: Is Gorse providing value?
- [ ] A/B test: Gorse recommendations vs baseline (trending/recent)
- [ ] Capacity planning: Scale Gorse if needed
- [ ] Update Gorse version if new features available

---

## Resources

- **Gorse Documentation**: https://docs.gorse.io/
- **Gorse GitHub**: https://github.com/gorse-io/gorse
- **Gorse API Reference**: https://docs.gorse.io/api/restful-api.html
- **Project Config**: `backend/gorse.toml`
- **Project Code**: `backend/internal/recommendations/gorse_rest.go`
- **TODO Plan**: `notes/GORSE_TODO_LIST_PLAN.md`

---

**Last Updated**: 2025-12-14
**Owner**: Backend Team
**Status**: Active Monitoring
