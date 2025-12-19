# Elasticsearch Production Setup

This document describes the Elasticsearch configuration for production deployment.

## Configuration

### Docker Compose Setup

Elasticsearch is configured in `docker-compose.prod.yml` with:

- **Image**: `docker.elastic.co/elasticsearch/elasticsearch:8.11.0`
- **Memory**: 256MB limit (128MB heap)
- **Network**: `sidechain` Docker network
- **Health Check**: Cluster health check every 10s
- **Data Persistence**: `elasticsearch_data` volume

### Backend Connection

The backend service is configured to connect to Elasticsearch via:

- **Environment Variable**: `ELASTICSEARCH_URL` (defaults to `http://elasticsearch:9200` in production)
- **Service Dependency**: Backend waits for Elasticsearch health check before starting
- **Graceful Fallback**: If Elasticsearch is unavailable, backend logs a warning and continues with PostgreSQL fallback

### Environment Variables

Set `ELASTICSEARCH_URL` in your `.env` file or docker-compose environment:

```bash
# Production (Docker network)
ELASTICSEARCH_URL=http://elasticsearch:9200

# Development (localhost)
ELASTICSEARCH_URL=http://localhost:9200
```

## Verification

### Quick Check

Run the verification script:

```bash
cd backend
./verify_elasticsearch.sh
```

### Manual Verification

1. **Check Elasticsearch is running:**
   ```bash
   docker ps | grep elasticsearch
   ```

2. **Check cluster health:**
   ```bash
   docker exec sidechain-elasticsearch curl -s http://localhost:9200/_cluster/health
   ```

3. **Check indices:**
   ```bash
   docker exec sidechain-elasticsearch curl -s http://localhost:9200/_cat/indices?v
   ```

4. **Check backend logs:**
   ```bash
   docker logs sidechain-backend | grep -i elasticsearch
   ```

   You should see:
   ```
   ✅ Elasticsearch indices initialized successfully
   ```

### Test Search Endpoints

Once Elasticsearch is running, test the search API:

```bash
# Search users
curl http://localhost:8787/api/v1/search/users?q=test

# Search posts
curl http://localhost:8787/api/v1/search/posts?q=techno&genre=techno
```

## Indexing Existing Data

If you have existing data in PostgreSQL that needs to be indexed in Elasticsearch:

```bash
cd backend
go run cmd/migrate/main.go index-elasticsearch
```

This will:
- Connect to Elasticsearch
- Create indices (users, posts, stories)
- Backfill all existing data from PostgreSQL
- Show progress and handle errors gracefully

## Troubleshooting

### Backend Can't Connect to Elasticsearch

1. **Check service is running:**
   ```bash
   docker ps | grep elasticsearch
   ```

2. **Check network connectivity:**
   ```bash
   docker exec sidechain-backend ping elasticsearch
   ```

3. **Check ELASTICSEARCH_URL:**
   ```bash
   docker exec sidechain-backend env | grep ELASTICSEARCH_URL
   ```

4. **Check Elasticsearch logs:**
   ```bash
   docker logs sidechain-elasticsearch
   ```

### Elasticsearch Health Check Failing

1. **Check cluster status:**
   ```bash
   docker exec sidechain-elasticsearch curl -s http://localhost:9200/_cluster/health
   ```

2. **Check disk space:**
   ```bash
   docker exec sidechain-elasticsearch df -h
   ```

3. **Check memory:**
   ```bash
   docker stats sidechain-elasticsearch
   ```

### Indices Not Created

Indices are created automatically on backend startup. If they're missing:

1. **Check backend logs** for initialization errors
2. **Manually create indices** by restarting the backend:
   ```bash
   docker restart sidechain-backend
   ```

3. **Or use the migration command** to create and populate indices

## Production Considerations

### Memory Limits

Current production configuration uses:
- **256MB total memory** (128MB heap)
- Suitable for small to medium deployments
- For larger deployments, consider increasing:
  ```yaml
  mem_limit: 512m
  ES_JAVA_OPTS=-Xms256m -Xmx256m
  ```

### Data Persistence

Elasticsearch data is stored in a Docker volume (`elasticsearch_data`). To backup:

```bash
# Backup volume
docker run --rm -v sidechain_elasticsearch_data:/data -v $(pwd):/backup \
  alpine tar czf /backup/elasticsearch-backup.tar.gz /data

# Restore volume
docker run --rm -v sidechain_elasticsearch_data:/data -v $(pwd):/backup \
  alpine tar xzf /backup/elasticsearch-backup.tar.gz -C /
```

### Monitoring

Monitor Elasticsearch health via:

1. **Docker health checks** (configured in docker-compose)
2. **Backend logs** (Elasticsearch connection status)
3. **Prometheus metrics** (if configured)
4. **Grafana dashboards** (if configured)

## Related Documentation

- [Elasticsearch Implementation Plan](../notes/ELASTICSEARCH_IMPLEMENTATION_PLAN.md)
- [Search API Documentation](../docs/API_SEARCH.md) (if exists)
- [Deployment Guide](../notes/DEPLOYMENT.md)
