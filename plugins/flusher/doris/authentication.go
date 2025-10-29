// Copyright 2025 LoongCollector Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package doris

import (
	"fmt"
	"net/http"

	"github.com/alibaba/ilogtail/pkg/tlscommon"
)

// Authentication handles different authentication methods for Doris Flusher
type Authentication struct {
	// PlainText authentication
	PlainText *PlainTextConfig
	// TLS authentication
	TLS *tlscommon.TLSConfig
}

// PlainTextConfig contains basic auth credentials
type PlainTextConfig struct {
	// The username for connecting to Doris
	Username string
	// The password for connecting to Doris
	Password string
	// The default database
	Database string
}

// ConfigureAuthentication applies authentication settings to HTTP requests for Doris Stream Load
func (config *Authentication) ConfigureAuthentication(headers *http.Header, client *http.Client) error {
	if config.PlainText != nil {
		if err := config.PlainText.ConfigurePlaintext(headers); err != nil {
			return err
		}
	}

	if config.TLS != nil {
		if err := configureTLS(config.TLS, client); err != nil {
			return err
		}
	}

	return nil
}

// ConfigurePlaintext configures basic authentication settings
func (plainTextConfig *PlainTextConfig) ConfigurePlaintext(headers *http.Header) error {
	// Validate Auth info
	if plainTextConfig.Username == "" {
		return nil // Username is optional for Doris
	}

	// Allow empty password - Doris default root user has no password
	return nil
}

// configureTLS sets up TLS configuration for the HTTP client
func configureTLS(config *tlscommon.TLSConfig, client *http.Client) error {
	tlsConfig, err := config.LoadTLSConfig()
	if err != nil {
		return fmt.Errorf("error loading tls config: %w", err)
	}

	if tlsConfig != nil {
		transport := http.DefaultTransport.(*http.Transport).Clone()
		transport.TLSClientConfig = tlsConfig
		client.Transport = transport
	}

	return nil
}

// GetUsernamePassword returns the username and password from authentication config
func (config *Authentication) GetUsernamePassword() (string, string, error) {
	if config.PlainText == nil {
		return "", "", fmt.Errorf("plaintext authentication config is not set")
	}

	if config.PlainText.Username == "" {
		return "", "", fmt.Errorf("username is not configured")
	}

	// Allow empty password - Doris default root user has no password
	return config.PlainText.Username, config.PlainText.Password, nil
}
