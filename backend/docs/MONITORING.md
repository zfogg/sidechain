# Prometheus & Grafana Monitoring Setup

This document describes the monitoring infrastructure for Sidechain backend using Prometheus for metrics collection and Grafana for visualization.

## Architecture Overview

```
┌──────────────────┐
│  Sidechain API   │
│  (Go Backend)    │
│  /metrics endpoint (Prometheus format)
└────────┬─────────┘
         │ scrapes every 10s
         ▼
┌──────────────────┐
│  Prometheus      │
│  Port: 9090      │
│  /metrics endpoint
└────────┬─────────┘
         │ queries
         ▼
┌──────────────────┐
│  Grafana         │
│  Port: 3000      │
│  Dashboards      │
└──────────────────┘
```

## Getting Started

### Start Monitoring Stack

```bash
# Start Prometheus and Grafana
docker-compose up prometheus grafana -d

# Or in development
docker-compose -f docker-compose.dev.yml up prometheus grafana -d
```

### Access URLs

- **Prometheus UI**: http://localhost:9090
- **Grafana UI**: http://localhost:3000
- **Backend Metrics Endpoint**: http://localhost:8787/metrics

### Grafana Login

- **Username**: `admin`
- **Password**: `admin` (set in docker-compose.yml)

## Metrics Collected

### HTTP Metrics

- `http_requests_total` - Total HTTP requests by method, path, and status
- `http_request_duration_seconds` - HTTP request latency (p50, p95, p99)
- `http_request_size_bytes` - Request body size distribution
- `http_response_size_bytes` - Response body size distribution
- `http_active_connections` - Currently active HTTP connections

### Cache Metrics

- `cache_hits_total` - Total cache hits
- `cache_misses_total` - Total cache misses
- `cache_hit_rate` - Derived: hits / (hits + misses)
- `cache_operation_duration_seconds` - Cache operation latency
- `cache_evictions_total` - Cache evictions

### Rate Limiting Metrics

- `rate_limit_exceeded_total` - Total rate limit violations
- `rate_limit_bucket_usage` - Current token bucket utilization per client

### Database Metrics

- `database_query_duration_seconds` - Query latency by type and table
- `database_queries_total` - Query count by type, table, and status
- `database_connections_open` - Open connections

### Redis Metrics

- `redis_operation_duration_seconds` - Redis operation latency
- `redis_operations_total` - Operation count by type and status
- `redis_connections_open` - Open connections

### Application Metrics

- `feed_generation_duration_seconds` - Time to generate feed
- `gorse_recommendations_total` - Recommendations fetched from Gorse
- `gorse_errors_total` - Gorse errors
- `errors_total` - Total errors by type and endpoint

## Available Dashboards

### 1. API Performance Dashboard

Monitors HTTP request metrics:
- Request rate (req/s)
- Request latency (p95, p99)
- Error rate (5xx)
- Response status distribution
- Requests by method and endpoint

**Access**: Grafana > Dashboards > API Performance

### 2. Cache Performance Dashboard

Monitors cache and Redis metrics:
- Cache hit/miss ratio (pie chart)
- Cache hit rate over time
- Cache operations rate
- Cache operation latency
- Redis connected clients
- Redis memory usage

**Access**: Grafana > Dashboards > Cache Performance

### 3. System Health Dashboard

Monitors system-level metrics:
- Monitored endpoints count
- Error rate percentage
- Memory usage
- Active goroutines
- Memory usage over time
- Response status distribution
- Hourly request counts

**Access**: Grafana > Dashboards > System Health

## Prometheus Configuration

Configuration is in `prometheus.yml`:

```yaml
global:
  scrape_interval: 15s      # Default scrape interval
  evaluation_interval: 15s

scrape_configs:
  - job_name: 'sidechain-backend'
    static_configs:
      - targets: ['localhost:8787']
    scrape_interval: 10s    # Scrape backend every 10s
    metrics_path: '/metrics'

  # Additional jobs for Redis, PostgreSQL, etc.
```

### Adding New Scrape Targets

Edit `prometheus.yml` and add a new scrape config:

```yaml
scrape_configs:
  - job_name: 'my-service'
    static_configs:
      - targets: ['localhost:8000']
    metrics_path: '/metrics'
    scrape_interval: 30s
```

## Grafana Provisioning

Dashboards and datasources are automatically provisioned via:

- **Datasources**: `grafana/provisioning/datasources/prometheus.yml`
- **Dashboards**: `grafana/provisioning/dashboards/` (JSON files)

### Adding Custom Dashboards

1. Create dashboard in Grafana UI
2. Export as JSON to `grafana/provisioning/dashboards/`
3. Restart Grafana: `docker-compose restart grafana`

Or use dashboard templates:
- [Prometheus](https://grafana.com/grafana/dashboards/1860)
- [Redis](https://grafana.com/grafana/dashboards/12114)
- [PostgreSQL](https://grafana.com/grafana/dashboards/9628)

## Recording Metrics Manually

The backend provides helper functions in `internal/middleware/prometheus_metrics.go`:

```go
// Cache metrics
middleware.RecordCacheHit("feed_cache")
middleware.RecordCacheMiss("feed_cache")
middleware.RecordCacheOperation("GET", "feed_cache", duration)

// Rate limiting
middleware.RecordRateLimitExceeded("/api/v1/feed", "GET")

// Database
middleware.RecordDatabaseQuery("SELECT", "users", duration, err)

// Redis
middleware.RecordRedisOperation("SET", "response:*", duration, err)

// Feed/Recommendations
middleware.RecordFeedGeneration("timeline", duration)
middleware.RecordGorseRecommendation("collaborative")

// Errors
middleware.RecordError("database_error", "/api/v1/feed")
```

## Querying Metrics

### Prometheus Query Examples

```promql
# Request rate
rate(http_requests_total[5m])

# Average latency
avg(http_request_duration_seconds) by (path)

# Cache hit rate
rate(cache_hits_total[5m]) / (rate(cache_hits_total[5m]) + rate(cache_misses_total[5m]))

# Error rate percentage
rate(http_requests_total{status=~"5.."}[5m]) / rate(http_requests_total[5m]) * 100

# Top 5 slowest endpoints
topk(5, avg(http_request_duration_seconds) by (path))

# Redis operations
rate(redis_operations_total[5m]) by (operation)
```

## Alerts (Future)

Configure alerts in `prometheus.yml`:

```yaml
alerting:
  alertmanagers:
    - static_configs:
        - targets: ['localhost:9093']  # Alertmanager address

rule_files:
  - "alerts.yml"
```

Example `alerts.yml`:

```yaml
groups:
  - name: sidechain_alerts
    rules:
      - alert: HighErrorRate
        expr: rate(http_requests_total{status=~"5.."}[5m]) > 0.05
        for: 5m
        annotations:
          summary: "High error rate detected"
```

## Retention and Storage

**Default Configuration**:
- Retention: 30 days (`--storage.tsdb.retention.time=30d`)
- Storage: `/prometheus` (Docker volume)

**Modify Retention**:

Edit `docker-compose.yml` or `docker-compose.dev.yml`:

```yaml
prometheus:
  command:
    - '--storage.tsdb.retention.time=90d'  # 90 days
```

## Performance Tuning

### Reduce Metrics Volume

Sample less frequently for verbose endpoints:

```yaml
scrape_configs:
  - job_name: 'sidechain-backend'
    scrape_interval: 30s        # Increase interval
    scrape_timeout: 10s         # Increase timeout
    metric_relabel_configs:
      # Drop low-cardinality metrics
      - source_labels: [__name__]
        regex: 'unused_.*'
        action: drop
```

### Optimize Cardinality

Limit label combinations to avoid metric explosion:

```go
// GOOD: Limited labels
m.HTTPRequestsTotal.WithLabelValues(method, "api", statusStr).Inc()

// BAD: High cardinality
m.HTTPRequestsTotal.WithLabelValues(method, fullPath, userID, statusStr).Inc()
```

## Troubleshooting

### Prometheus Not Scraping

1. Check `prometheus.yml` syntax: `promtool check config prometheus.yml`
2. Verify targets are reachable: http://localhost:9090/targets
3. Check logs: `docker-compose logs prometheus`

### No Data in Grafana

1. Verify Prometheus datasource is configured
2. Check Prometheus query in Grafana dashboard
3. Verify metrics are being collected: `curl http://localhost:8787/metrics | grep metric_name`

### High Memory Usage

1. Reduce retention period
2. Reduce scrape frequency
3. Drop unnecessary metrics with `metric_relabel_configs`

## Next Steps

1. **Set up Alertmanager** for notifications
2. **Configure persistent storage** for Prometheus and Grafana
3. **Add custom dashboards** for business metrics
4. **Integrate with APM tools** (Jaeger, Tempo)
5. **Set up log aggregation** (ELK, Loki)

## References

- [Prometheus Documentation](https://prometheus.io/docs/)
- [Grafana Documentation](https://grafana.com/docs/grafana/latest/)
- [Go Prometheus Client](https://github.com/prometheus/client_golang)
- [Prometheus Best Practices](https://prometheus.io/docs/practices/instrumentation/)
