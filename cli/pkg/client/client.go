package client

import (
	"time"

	"github.com/go-resty/resty/v2"
	"github.com/zfogg/sidechain/cli/pkg/config"
	"github.com/zfogg/sidechain/cli/pkg/logger"
)

var httpClient *resty.Client
var impersonateUser string

// Init initializes the HTTP client
func Init() {
	httpClient = resty.New()

	baseURL := config.GetString("api.base_url")
	timeout := time.Duration(config.GetInt("api.timeout")) * time.Second

	httpClient.SetBaseURL(baseURL)
	httpClient.SetTimeout(timeout)
	httpClient.SetHeader("User-Agent", "Sidechain-CLI/0.1.0")

	// Add request/response logging
	httpClient.OnBeforeRequest(func(c *resty.Client, req *resty.Request) error {
		logger.Debug("HTTP Request", "method", req.Method, "url", req.URL)

		// Add impersonation header if set
		if impersonateUser != "" {
			req.Header.Set("X-Impersonate-User", impersonateUser)
			logger.Debug("Impersonating user", "email", impersonateUser)
		}

		return nil
	})

	httpClient.OnAfterResponse(func(c *resty.Client, resp *resty.Response) error {
		logger.Debug("HTTP Response", "status", resp.StatusCode)
		return nil
	})
}

// GetClient returns the HTTP client
func GetClient() *resty.Client {
	if httpClient == nil {
		Init()
	}
	return httpClient
}

// SetAuthToken sets the authorization token
func SetAuthToken(token string) {
	if httpClient == nil {
		Init()
	}
	httpClient.SetHeader("Authorization", "Bearer "+token)
}

// ClearAuthToken clears the authorization token
func ClearAuthToken() {
	if httpClient == nil {
		Init()
	}
	// Re-init the client to clear auth headers
	httpClient = resty.New()
	baseURL := config.GetString("api.base_url")
	timeout := time.Duration(config.GetInt("api.timeout")) * time.Second
	httpClient.SetBaseURL(baseURL)
	httpClient.SetTimeout(timeout)
	httpClient.SetHeader("User-Agent", "Sidechain-CLI/0.1.0")
}

// SetImpersonateUser sets the user to impersonate for API requests (admin only)
func SetImpersonateUser(email string) {
	impersonateUser = email
}

// ClearImpersonateUser clears the impersonation
func ClearImpersonateUser() {
	impersonateUser = ""
}
