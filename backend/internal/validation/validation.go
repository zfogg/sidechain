package validation

import (
	"context"
	"fmt"
	"net/http"
	"os"
	"strings"
	"time"

	"github.com/elastic/go-elasticsearch/v8"
	"github.com/zfogg/sidechain/backend/internal/cache"
	"github.com/zfogg/sidechain/backend/internal/logger"
	"github.com/zfogg/sidechain/backend/internal/storage"
	"go.uber.org/zap"
)

// ServiceValidator handles validation of optional services
type ServiceValidator struct {
	requiredServices []string
}

// NewServiceValidator creates a new service validator
func NewServiceValidator() *ServiceValidator {
	return &ServiceValidator{
		requiredServices: parseRequiredServices(),
	}
}

// ValidateServices validates all configured services
func (sv *ServiceValidator) ValidateServices(ctx context.Context) error {
	if len(sv.requiredServices) == 0 {
		logger.Log.Info("No required services configured for validation")
		return nil
	}

	logger.Log.Info("🔍 Validating required services",
		zap.Strings("services", sv.requiredServices),
	)

	services := sv.getServiceChecks()

	for _, serviceName := range sv.requiredServices {
		serviceChecker, ok := services[serviceName]
		if !ok {
			logger.Log.Warn("Unknown service type in validation",
				zap.String("service", serviceName),
			)
			continue
		}

		logger.Log.Info("Validating service",
			zap.String("service", serviceName),
		)

		timeoutCtx, cancel := context.WithTimeout(ctx, 10*time.Second)
		if err := serviceChecker(timeoutCtx); err != nil {
			cancel()
			errorMsg := fmt.Sprintf("❌ Required service '%s' validation failed: %v", serviceName, err)
			logger.Log.Error(errorMsg)
			return fmt.Errorf(errorMsg)
		}
		cancel()

		logger.Log.Info("✅ Service validated successfully",
			zap.String("service", serviceName),
		)
	}

	logger.Log.Info("✅ All required services validated successfully")
	return nil
}

// getServiceChecks returns a map of service names to their validation functions
func (sv *ServiceValidator) getServiceChecks() map[string]func(ctx context.Context) error {
	return map[string]func(ctx context.Context) error{
		"elasticsearch": validateElasticsearch,
		"s3":            validateS3,
		"redis":         validateRedis,
		"gorse":         validateGorse,
	}
}

// validateElasticsearch checks if Elasticsearch is reachable
func validateElasticsearch(ctx context.Context) error {
	esHost := os.Getenv("ELASTICSEARCH_HOST")
	if esHost == "" {
		esHost = "localhost"
	}
	esPort := os.Getenv("ELASTICSEARCH_PORT")
	if esPort == "" {
		esPort = "9200"
	}

	addresses := []string{fmt.Sprintf("http://%s:%s", esHost, esPort)}

	// If ELASTICSEARCH_SCHEME is set, use it
	if scheme := os.Getenv("ELASTICSEARCH_SCHEME"); scheme != "" {
		addresses = []string{fmt.Sprintf("%s://%s:%s", scheme, esHost, esPort)}
	}

	cfg := elasticsearch.Config{
		Addresses: addresses,
	}

	// Add username/password if provided
	if username := os.Getenv("ELASTICSEARCH_USERNAME"); username != "" {
		cfg.Username = username
		cfg.Password = os.Getenv("ELASTICSEARCH_PASSWORD")
	}

	client, err := elasticsearch.NewClient(cfg)
	if err != nil {
		return fmt.Errorf("failed to create Elasticsearch client: %w", err)
	}

	// Test the connection
	res, err := client.Info(ctx)
	if err != nil {
		return fmt.Errorf("failed to connect to Elasticsearch: %w", err)
	}
	defer res.Body.Close()

	if res.IsError() {
		return fmt.Errorf("Elasticsearch returned error status: %s", res.Status())
	}

	return nil
}

// validateS3 checks if S3 bucket is accessible
func validateS3(ctx context.Context) error {
	region := os.Getenv("AWS_REGION")
	bucket := os.Getenv("AWS_BUCKET")
	accessKeyID := os.Getenv("AWS_ACCESS_KEY_ID")
	secretAccessKey := os.Getenv("AWS_SECRET_ACCESS_KEY")

	if region == "" || bucket == "" {
		return fmt.Errorf("AWS_REGION and AWS_BUCKET are required for S3 validation")
	}

	if accessKeyID == "" || secretAccessKey == "" {
		return fmt.Errorf("AWS_ACCESS_KEY_ID and AWS_SECRET_ACCESS_KEY are required for S3 validation")
	}

	// Try to create an S3 uploader and check bucket access
	// This is the same pattern used in main.go
	cdnURL := os.Getenv("CDN_BASE_URL")
	if cdnURL == "" {
		cdnURL = "https://cdn.sidechain.app"
	}

	s3Uploader, err := storage.NewS3Uploader(region, bucket, cdnURL)
	if err != nil {
		return fmt.Errorf("failed to initialize S3 client: %w", err)
	}

	if err := s3Uploader.CheckBucketAccess(ctx); err != nil {
		return fmt.Errorf("S3 bucket access check failed: %w", err)
	}

	return nil
}

// validateRedis checks if Redis is reachable
func validateRedis(ctx context.Context) error {
	redisHost := os.Getenv("REDIS_HOST")
	redisPort := os.Getenv("REDIS_PORT")
	redisPassword := os.Getenv("REDIS_PASSWORD")

	if redisHost == "" {
		redisHost = "localhost"
	}
	if redisPort == "" {
		redisPort = "6379"
	}

	redisClient, err := cache.NewRedisClient(redisHost, redisPort, redisPassword)
	if err != nil {
		return fmt.Errorf("failed to connect to Redis: %w", err)
	}
	defer redisClient.Close()

	return nil
}

// validateGorse checks if Gorse is reachable
func validateGorse(ctx context.Context) error {
	gorseURL := os.Getenv("GORSE_URL")
	if gorseURL == "" {
		gorseURL = "http://localhost:8087"
	}

	// Simple HTTP GET to the Gorse health endpoint
	client := &http.Client{
		Timeout: 10 * time.Second,
	}

	// Try to access Gorse API health endpoint
	url := fmt.Sprintf("%s/api/health", gorseURL)
	req, err := http.NewRequestWithContext(ctx, http.MethodGet, url, nil)
	if err != nil {
		return fmt.Errorf("failed to create Gorse health check request: %w", err)
	}

	resp, err := client.Do(req)
	if err != nil {
		return fmt.Errorf("failed to connect to Gorse at %s: %w", gorseURL, err)
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		return fmt.Errorf("Gorse returned status %d", resp.StatusCode)
	}

	return nil
}

// parseRequiredServices parses the SIDECHAIN_BACKEND_REQUIRE_* environment variables
// Returns a list of service names that are required
func parseRequiredServices() []string {
	var required []string

	// List of all optional services
	services := []string{"elasticsearch", "s3", "redis", "gorse"}

	for _, service := range services {
		envVar := fmt.Sprintf("SIDECHAIN_BACKEND_REQUIRE_%s", strings.ToUpper(service))
		value := os.Getenv(envVar)

		if isTruthy(value) {
			required = append(required, service)
		}
	}

	return required
}

// isTruthy checks if a string value represents a truthy value
func isTruthy(value string) bool {
	if value == "" {
		return false
	}

	value = strings.ToLower(strings.TrimSpace(value))
	return value == "1" || value == "true" || value == "yes" || value == "on"
}
