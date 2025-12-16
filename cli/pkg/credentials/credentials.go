package credentials

import (
	"encoding/json"
	"os"
	"time"

	"github.com/zfogg/sidechain/cli/pkg/config"
)

type Credentials struct {
	AccessToken  string    `json:"access_token"`
	RefreshToken string    `json:"refresh_token"`
	ExpiresAt    time.Time `json:"expires_at"`
	UserID       string    `json:"user_id"`
	Username     string    `json:"username"`
	Email        string    `json:"email"`
	IsAdmin      bool      `json:"is_admin"`
}

// Load loads credentials from disk
func Load() (*Credentials, error) {
	path := config.GetCredentialsPath()

	data, err := os.ReadFile(path)
	if err != nil {
		if os.IsNotExist(err) {
			return nil, nil // Credentials don't exist yet
		}
		return nil, err
	}

	var creds Credentials
	if err := json.Unmarshal(data, &creds); err != nil {
		return nil, err
	}

	return &creds, nil
}

// Save saves credentials to disk
func Save(creds *Credentials) error {
	path := config.GetCredentialsPath()

	data, err := json.MarshalIndent(creds, "", "  ")
	if err != nil {
		return err
	}

	// Write with restricted permissions (owner read/write only)
	if err := os.WriteFile(path, data, 0600); err != nil {
		return err
	}

	return nil
}

// Delete deletes credentials from disk
func Delete() error {
	path := config.GetCredentialsPath()
	return os.Remove(path)
}

// IsExpired checks if the access token is expired
func (c *Credentials) IsExpired() bool {
	return time.Now().After(c.ExpiresAt)
}

// IsValid checks if credentials are valid
func (c *Credentials) IsValid() bool {
	return c.AccessToken != "" && !c.IsExpired()
}
